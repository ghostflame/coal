#include "coal.h"


void sync_single_node( NODE *n )
{
	POINT *list, *p;
	int i, ct, wr;
	C3PNT *c3p;
	C3HDL *h;

	list = NULL;
	ct   = 0;

	node_lock( n );
	if( !( n->flags & NODE_FLAG_CREATED ) )
	{
		// can't do this one yet
		node_unlock( n );
		return;
	}
	else if( n->flags & NODE_FLAG_WRITING )
	{
		// some other thread has this one
		node_unlock( n );
		return;
	}

	if( n->waiting > 0 )
	{
		list = n->incoming;
		ct   = n->waiting;
		n->incoming = NULL;
		n->waiting  = 0;

		// block other threads
		n->flags |= NODE_FLAG_WRITING;
	}
	node_unlock( n );

	if( !ct )
		return;

	// build the points list
	c3p = (C3PNT *) allocz( ct * sizeof( C3PNT ) );
	for( i = 0, p = list; p; p = p->next, i++ )
		c3p[i] = p->data;

	h = c3db_open( n->dir_path, C3DB_RW );
	c3db_write( h, ct, c3p );
	c3db_flush( h, &wr );
	c3db_close( h );

	// lock it again to change the flags
	node_lock( n );
	n->flags &= ~NODE_FLAG_WRITING;
	node_unlock( n );

	// don't need these any more
	free( c3p );
}



void sync_nodes( NODE *n )
{
	NODE *cn;

	if( n->flags & NODE_FLAG_LEAF )
		sync_single_node( n );
	else
		// depth-first is fine
		for( cn = n->children; cn; cn = cn->next )
			sync_nodes( cn );
}




void *sync_all_nodes( void *arg )
{
	THRD *t = (THRD *) arg;

	sync_nodes( ctl->node->nodes );

	free( t );
	return NULL;
}




void *sync_loop( void *arg )
{
	double syncTick, makeTick;

	THRD *t = (THRD *) arg;

	// throw them into the future
	syncTick = ctl->curr_time + ctl->sync->sync_sec;
	makeTick = ctl->curr_time + ctl->sync->make_sec;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ctl->curr_time > syncTick )
		{
			syncTick += ctl->sync->sync_sec;
			throw_thread( sync_all_nodes, NULL );
		}

		if( ctl->curr_time > makeTick )
		{
			makeTick += ctl->sync->make_sec;
			throw_thread( node_maker, NULL );
		}

		usleep( 100000 );
	}

	// TODO
	// say we're shut down

	free( t );
	return NULL;
}




int sync_config_line( AVP *av )
{
	if( attIs( "sync_sec" ) )
		ctl->sync->sync_sec = strtod( av->val, NULL );
	else if( attIs( "make_sec" ) )
		ctl->sync->make_sec = strtod( av->val, NULL );
	else if( attIs( "tick_usec" ) )
		ctl->sync->tick_usec = strtoul( av->val, NULL, 10 );
	else
		return -1;

	return 0;
}



SYNC_CTL *sync_config_defaults( void )
{
	SYNC_CTL *sc;

	sc = (SYNC_CTL *) allocz( sizeof( SYNC_CTL ) );

	sc->sync_sec  = DEFAULT_SYNC_INTERVAL;
	sc->make_sec  = DEFAULT_MAKE_INTERVAL;
	sc->tick_usec = DEFAULT_MAIN_TICK_USEC;

	return sc;
}
