#include "coal.h"

#define LLFID LLFDA


char *data_bin_type_names( int type )
{
	switch( type )
	{
		case BINF_TYPE_DATA:
			return "data";
		case BINF_TYPE_QUERY:
			return "query";
		case (BINF_TYPE_QUERY|BINF_TYPE_REPLY):
			return "query answer";
	}

	return "unknown";
}



// check what to do with a point
void data_point_to_node( NODE *n, POINT *p )
{
	if( n->flags & NODE_FLAG_RELAY )
		relay_add_point( n->policy->dest, p );
	else
		node_add_point( n, p );
}



void data_point( POINT *p, PATH *path, NODE *parent )
{
	int len, leaf;
	char *wd;
	NODE *n;

	wd   = path->w->wd[path->curr];
	len  = path->w->len[path->curr];
	// we move on path->curr because either we finish here
	// or we are going onto the next level down
	path->curr++;
	leaf = ( path->curr == path->w->wc ) ? 1 : 0;

	for( n = parent->children; n; n = n->next )
	{
		if( n->name_len != len
		 || memcmp( n->name, wd, len ) )
			continue;

		// found it
		if( n->flags & NODE_FLAG_ERROR )
			mem_free_point( &p );
		else if( leaf )
			// shouldn't happen much with pcache
		  	data_point_to_node( n, p );
		else
			data_point( p, path, n );

		// and done
		return;
	}

	// we need a new node
	n = node_create( wd, len, parent, p->path, leaf );

	// so either add the node, or move on
	if( leaf )
		data_point_to_node( n, p );
	else
		data_point( p, path, n );
}





// as fast as possible here - this is the happy path
// for finding where to send a point
int data_route_point( POINT *p )
{
	PCACHE *pc;

	if( !( pc = node_find_pcache( p->path ) ) )
		return -1;

	if( pc->relay )
		relay_add_point( pc->target.dest, p );
	else
		node_add_point( pc->target.node, p );

	return 0;
}




void data_push_points( POINT *list )
{
	POINT *p;

	while( list )
	{
		p       = list;
		list    = p->next;
		p->next = NULL;

		data_point( p, p->path, ctl->node->nodes );
	}
}




void queue_up_incoming( POINT *head, POINT *tail )
{
	pthread_mutex_lock( &(ctl->locks->data) );

	tail->next   = ctl->data_in;
	ctl->data_in = head;

	pthread_mutex_unlock( &(ctl->locks->data) );
}



void grab_incoming( POINT **list )
{
	if( !list )
		return;
	*list = NULL;

	pthread_mutex_lock( &(ctl->locks->data) );

	*list        = ctl->data_in;
	ctl->data_in = NULL;

	pthread_mutex_unlock( &(ctl->locks->data) );
}


// write a pong immediately
void data_pong( HOST *h, void *buf, int len )
{
	uint32_t *ul, ts;
	uint16_t *us, ms;
	uint8_t *uc;
	double ct;

	ts = *((uint32_t *) (buf + 4));
	ms = *((uint16_t *) (buf + 12));
	ct = timedbl( NULL );

	uc = (uint8_t *) (h->net->out.buf + h->net->out.len);

	*uc++ = 0x01;
	*uc++ = BINF_TYPE_PONG;
	us    = (uint16_t *) uc;
	*us++ = 12;
	ul    = (uint32_t *) us;
	*ul++ = ts;
	*ul++ = (uint32_t) ct;
	us    = (uint16_t *) ul;
	*us++ = ms;
	// get msec
	ct   -= (uint32_t) ct; 
	ct   *= 1000;
	*us++ = (uint16_t) ct;

	h->net->out.len += 16;
	net_write_data( h->net );
}



//fnid 3
POINT *data_bin_read( HOST *h )
{
	int i, len, type;
	POINT *list, *p;
	void *buf;

	list = NULL;

	while( net_read_bin( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			// each of these is a binary chunk
			buf  = h->all->wd[i];
			len  = h->all->len[i];

			// read the type
			type = (int) *((uint8_t *) (buf + 1));

			// just respond to pings
			if( type == BINF_TYPE_PING )
			{
				data_pong( h, buf, len );
				continue;
			}

			if( type != BINF_TYPE_DATA )
			{
				warn( 0x0301, "Received type %d/%s from host %s on data bin connection.",
					type, data_bin_type_names( type ), h->net->name );
				h->net->flags |= HOST_CLOSE;
				return NULL;
			}

			p           = mem_new_point( );
			p->data.ts  = *((time_t *) ( buf + 4 ));
			p->data.val = *((float *)  ( buf + 8 ));
			p->path     = mem_new_path( (char *) ( buf + 12 ), len - 13 );

			p->next = list;
			list    = p;
		}

	return list;
}

//fnid 4
POINT *data_line_read( HOST *h )
{
	POINT *list, *p;
	int i, len;
	char *wd;

	list = NULL;

	while( net_read_lines( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			wd  = h->all->wd[i];
			len = h->all->len[i];

			if( strwords( h->val, wd, len, FIELD_SEPARATOR ) != DATA_FIELD_COUNT )
			{
				debug( 0x0401, "Invalid line from data host %s", h->net->name );
				continue;
			}

			p           = mem_new_point( );
			p->data.val = atof( h->val->wd[DATA_FIELD_VALUE] );
			p->data.ts  = (time_t) strtoul( h->val->wd[DATA_FIELD_TS], NULL, 10 );
			p->path     = mem_new_path( h->val->wd[DATA_FIELD_PATH],
			                            h->val->len[DATA_FIELD_PATH] );

			p->next = list;
			list    = p;
		}

	return list;
}


// fnid 5
void data_handle_connection( HOST *h )
{
	POINT *incoming, *end, *vals, *pt;
	struct pollfd p;
	double lastPush;
	int rv;

	p.fd     = h->net->sock;
	p.events = POLL_EVENTS;
	lastPush = ctl->curr_time;
	incoming = NULL;
	end      = NULL;


	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( ctl->curr_time - lastPush ) > 1.0 )
		{
			if( incoming && end )
			{
				queue_up_incoming( incoming, end );
				end = incoming = NULL;
			}

			lastPush = ctl->curr_time;
		}

		// timeout is responsiveness to RUN_LOOP
		if( ( rv = poll( &p, 1, 500 ) ) < 0 )
		{
			// don't sweat an interrupted poll call
			if( errno == EINTR )
				continue;

			warn( 0x0501, "Poll error talking to host %s -- %s",
				h->net->name, Err );
			break;
		}

		else if( !rv )
			continue;

		// are they going away?
		if( p.revents & POLLHUP )
			break;

		// try to get some data
		vals = (*(h->drf))( h );

		// let's see what happened
		while( vals )
		{
			pt   = vals;
			vals = pt->next;

			pt->next = NULL;

			// try it the quick way - pcache
			if( data_route_point( pt ) == 0 )
			{
				++(h->points);
				continue;
			}

			// ass - do it the slow way
			if( node_path_parse( pt->path ) <= 0 )
			{
				info( 0x0502, "Invalid path string: '%s'",
					h->val->wd[DATA_FIELD_PATH] );
				// frees the path
				mem_free_point( &pt );
				continue;
			}

			h->points++;

			// either start the list
			// or append it
			if( !incoming )
			{
				incoming = pt;
				end      = pt;
			}
			else
			{
				end->next = pt;
				end       = pt;
			}
		}

		// allows data_fetch to signal us to close the host
		if( h->net->flags & HOST_CLOSE )
			break;
	}

	// push, synchronous
	if( incoming && end )
		queue_up_incoming( incoming, end );
}


// fnid 6
void *data_connection( void *arg )
{
	THRD *t;
	HOST *h;

	t = (THRD *) arg;
	h = (HOST *) t->arg;

	info( 0x0601, "Accepted data connection from host %s", h->net->name );

	// make sure we can be cancelled
	pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
	pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

	data_handle_connection( h );

	info( 0x0602, "Closing connection from host %s after %lu data points.",
				h->net->name, h->points );

	net_close_host( h );

  	free( t );
	return NULL;
}


void *push_loop( void *arg )
{
	POINT *inc = NULL;
	THRD *t = (THRD *) arg;

	loop_mark_start( );

	while( ctl->run_flags & RUN_LOOP )
	{
		// anything for us?
		if( ctl->data_in )
		{
			grab_incoming( &inc );
			data_push_points( inc );
		}
		else
			usleep( 20000 );
	}

	// one last time
	if( ctl->data_in )
	{
		grab_incoming( &inc );
		data_push_points( inc );
	}

	loop_mark_done( );

	free( t );
	return NULL;
}


// fnid 7
void *data_loop( void *arg )
{
	NET_TYPE_CTL *ntc;
	struct pollfd p;
	PORT_CTL *pc;
	HOST *h;
	THRD *t;
	int rv;

	t   = (THRD *) arg;
	ntc = (NET_TYPE_CTL *) t->arg;
	pc  = ntc->data;

	p.fd     = pc->sock;
	p.events = POLL_EVENTS;

	loop_mark_start( );

	while( ctl->run_flags & RUN_LOOP )
	{
	  	// this value is all about responsiveness to shutdown
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				err( 0x0701, "Poll error on data socket -- %s", Err );
				loop_end( "polling error on data hosts" );
				break;
			}

			continue;
		}
		else if( !rv )
			continue;

		if( p.revents & POLL_EVENTS )
		{
			if( ( h = net_get_host( p.fd, ntc->type ) ) )
				thread_throw_watched( data_connection, h );
		}
	}

	// say we are shut down
	loop_mark_done( );

	free( t );
	return NULL;
}


#undef LLFID

