#include "coal.h"


int data_path_parse( PATH *p )
{
	// make a copy for strwords to eat
	p->copy = p->str + p->len + 1;
	memcpy( p->copy, p->str, p->len );
	// have to cap it, this might not be virgin memory
	*(p->copy + p->len) = '\0';

	if( strwords( p->w, p->copy, p->len, PATH_SEPARATOR ) < 0 )
	{
		warn( "Could not parse path '%s'", p->str );
		p->copy = NULL;

		return -1;
	}

	return p->w->wc;
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
			node_add_point( n, p );
		else
			data_point( p, path, n );

		// and done
		return;
	}

	// we need a new node
	n = node_create( wd, len, parent, p->path, leaf );

	// so either add the node, or move on
	if( leaf )
		node_add_point( n, p );
	else
		data_point( p, path, n );
}




static uint32_t data_cksum_primes[8] =
{
	2909, 3001, 3083, 3187, 3259, 3343, 3517, 3581
};


uint32_t data_path_cksum( char *str, int len )
{
#ifdef CKSUM_64BIT
	register uint64_t *p, sum = 0xbeef;
#else
	register uint32_t *p, sum = 0xbeef;
#endif
	int rem;

#ifdef CKSUM_64BIT
	rem = len & 0x7;
	len = len >> 3;

	for( p = (uint64_t *) str; len > 0; len-- )
#else
	rem = len & 0x3;
	len = len >> 2;

	for( p = (uint32_t *) str; len > 0; len-- )
#endif

		sum ^= *p++;

	// and capture the rest
	str = (char *) p;
	while( rem-- > 0 )
		sum += *str++ * data_cksum_primes[rem];

	return sum;
}


// as fast as possible here - this is the happy path
// for finding where to send a point
int data_route_point( POINT *p )
{
	register PCACHE *pc;
  	uint32_t sum, hval;

	sum  = data_path_cksum( p->path->str, p->path->len );
	hval = sum % ctl->node->pcache_sz;

	for( pc = ctl->node->pcache[hval]; pc; pc = pc->next )
		if( pc->sum == sum
		 && pc->len == p->path->len
		 && !memcmp( pc->path, p->path->str, pc->len ) )
		{
			if( pc->relay )
				relay_add_point( pc->target.dest, p );
			else
				node_add_point( pc->target.node, p );
			return 0;
		}

	return -1;
}




void data_add_path_cache( PATH *p, NODE *n, RDEST *d )
{
	uint32_t hval;
	PCACHE *pc;

	pc       = (PCACHE *) allocz( sizeof( PCACHE ) );
	pc->len  = p->len;
	pc->path = str_dup( p->str, pc->len );
	pc->sum  = data_path_cksum( pc->path, pc->len );

	if( n )
		pc->target.node = n;
	else
	{
		pc->target.dest = d;
		pc->relay       = 1;
	}

	hval = pc->sum % ctl->node->pcache_sz;

	pthread_mutex_lock( &(ctl->locks->cache) );

	pc->next = ctl->node->pcache[hval];
	ctl->node->pcache[hval] = pc;

	if( n )
	{
		ndebug( "Node %u added to path cache at position %u",
			n->id, hval );
	}

	pthread_mutex_unlock( &(ctl->locks->cache) );
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




void data_bin_connection( HOST *h )
{
	return;
}



POINT *data_line_fetch( HOST *h )
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
				debug( "Invalid line from data host %s", h->name );
				continue;
			}

			p            = mem_new_point( );
			p->data.val  = atof( h->val->wd[DATA_FIELD_VALUE] );
			p->data.ts   = (time_t) strtoul( h->val->wd[DATA_FIELD_TS], NULL, 10 );
			p->path      = mem_new_path( h->val->wd[DATA_FIELD_PATH],
			                             h->val->len[DATA_FIELD_PATH] );

			// try it the quick way - pcache
			if( data_route_point( p ) == 0 )
			{
				++(h->points);
				continue;
			}

			// ass - do it the slow way
			if( data_path_parse( p->path ) <= 0 )
			{
				info( "Invalid path string: '%s'",
					h->val->wd[DATA_FIELD_PATH] );
				// frees the path
				mem_free_point( &p );
				continue;
			}

			++(h->points);

			p->next = list;
			list    = p;
		}

	return list;
}



void data_line_connection( HOST *h )
{
	POINT *incoming, *inc_end, *vals, *pt;
	struct pollfd p;
	double lastPush;
	int rv;

	p.fd     = h->fd;
	p.events = POLL_EVENTS;
	lastPush = ctl->curr_time;
	incoming = NULL;
	inc_end  = NULL;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( ctl->curr_time - lastPush ) > 1.0 )
		{
			if( incoming && inc_end )
			{
				queue_up_incoming( incoming, inc_end );
				inc_end = incoming = NULL;
			}

			lastPush = ctl->curr_time;
		}

		// timeout is responsiveness to RUN_LOOP
		if( ( rv = poll( &p, 1, 500 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				warn( "Poll error talking to host %s -- %s",
					h->name, Err );
				break;
			}
		}
		else if( !rv )
			continue;

		if( ( vals = data_line_fetch( h ) ) )
		{
			for( pt = vals; pt->next; pt = pt->next )
				h->points++;

			if( incoming )
				pt->next = incoming;
			else
			  	inc_end  = pt;

			incoming = vals;
		}

		// allows data_fetch to signal us to close the host
		if( h->flags & HOST_CLOSE )
			break;
	}

	// push, synchronous
	if( incoming && inc_end )
		queue_up_incoming( incoming, inc_end );
}




void *data_connection( void *arg )
{
	THRD *t;
	HOST *h;

	t = (THRD *) arg;
	h = (HOST *) t->arg;

	info( "Accepted data connection from host %s", h->name );

	switch( h->type )
	{
		case NET_COMM_LINE:
			data_line_connection( h );
		case NET_COMM_BIN:
			data_bin_connection( h );
		default:
			break;
	}

	info( "Closing connection from host %s after %lu data points.",
				h->name, h->points );

	if( shutdown( h->fd, SHUT_RDWR ) )
		err( "Shutdown error on host %s -- %s",
				h->name, Err );

	close( h->fd );
	mem_free_host( &h );

  	free( t );
	return NULL;
}


void *push_loop( void *arg )
{
	POINT *inc = NULL;
	THRD *t = (THRD *) arg;

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

	free( t );
	return NULL;
}



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

	while( ctl->run_flags & RUN_LOOP )
	{
	  	// this value is all about responsiveness to shutdown
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				err( "Poll error on data socket -- %s", Err );
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
				thread_throw( data_connection, h );
		}
	}

	// TODO
	// say we are shut down

	free( t );
	return NULL;
}



