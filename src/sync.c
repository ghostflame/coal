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
	unsigned long sid = ctl->sync->sync_id;
	THRD *t = (THRD *) arg;

	debug( "Starting synchronization thread (%02d) %08lu/%lu.",
		ctl->sync->curr_threads, sid, (unsigned long) t->id );

	sync_nodes( ctl->node->nodes );

	debug( "Finished synchronization thread (%02d) %08lu/%lu.",
		ctl->sync->curr_threads, sid, (unsigned long) t->id );

	// keep track of threads
	pthread_mutex_lock( &(ctl->locks->sync) );
	// don't let us do anything stupid
	if( ctl->sync->curr_threads > 0 )
		ctl->sync->curr_threads--;
	pthread_mutex_unlock( &(ctl->locks->sync) );

	free( t );
	return NULL;
}




void *sync_loop( void *arg )
{
	double syncTick, makeTick;

	THRD *t = (THRD *) arg;

	loop_mark_start( );

	// throw them into the future
	syncTick = ctl->curr_time + ctl->sync->sync_sec;
	makeTick = ctl->curr_time + ctl->sync->make_sec;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ctl->curr_time > syncTick )
		{
			if( ctl->sync->curr_threads < ctl->sync->max_threads )
			{
				// keep track of threads
				pthread_mutex_lock( &(ctl->locks->sync) );
				ctl->sync->curr_threads++;
				ctl->sync->sync_id++;
				thread_throw( sync_all_nodes, NULL );
				pthread_mutex_unlock( &(ctl->locks->sync) );
			}

			syncTick += ctl->sync->sync_sec;
		}

		if( ctl->curr_time > makeTick )
		{
			thread_throw( node_maker, NULL );
			makeTick += ctl->sync->make_sec;
		}

		usleep( 100000 );
	}

	// say we're shut down
	loop_mark_done( );

	free( t );
	return NULL;
}




int sync_config_line( AVP *av )
{
	if( attIs( "sync_sec" ) )
		ctl->sync->sync_sec = strtod( av->val, NULL );
	else if( attIs( "make_sec" ) )
		ctl->sync->make_sec = strtod( av->val, NULL );
	else if( attIs( "max_sync_threads" ) )
	  	ctl->sync->max_threads = strtoul( av->val, NULL, 10 );
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

	sc->sync_sec    = DEFAULT_SYNC_INTERVAL;
	sc->make_sec    = DEFAULT_MAKE_INTERVAL;
	sc->tick_usec   = DEFAULT_MAIN_TICK_USEC;
	sc->max_threads = DEFAULT_MAX_SYNC_THREADS;

	return sc;
}
