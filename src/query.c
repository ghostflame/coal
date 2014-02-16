#include "coal.h"

int find_cached_node( QUERY *q )
{
	uint32_t sum, hval;
	PCACHE *pc;

	sum  = data_path_cksum( q->path->str, q->path->len );
	hval = sum % ctl->node->pcache_sz;

	for( pc = ctl->node->pcache[hval]; pc; pc = pc->next )
		if( pc->sum == sum
		 && pc->len == q->path->len
		 && !memcmp( q->path->str, pc->path, pc->len ) )
		{
			q->node = pc->node;
			return 0;
		}

	return -1;
}



int find_node( QUERY *q, NODE *parent )
{
	int len, leaf;
	char *wd;
	NODE *n;
	PATH *p;

	p   = q->path;
	wd  = p->w->wd[p->curr];
	len = p->w->len[p->curr];

	// move on because either we finish here,
	// or we are going down to the next string
	p->curr++;

	leaf = ( p->curr == p->w->wc ) ? 1 : 0;

	for( n = parent->children; n; n = n->next )
		if( n->name_len == len && !memcmp( n->name, wd, len ) )
		{
			if( n->flags & NODE_FLAG_ERROR )
				return -1;
			else if( leaf )
			{
				if( !( n->flags & NODE_FLAG_LEAF ) )
				{
					qwarn( "Trying to query a non-leaf node %s",
						n->dir_path );
					return -1;
				}

				q->node = n;
				return 0;
			}
			else
				return find_node( q, n );
		}

	return -1;
}



int query_send_fields( HOST *h, QUERY *q )
{
	int j, max, hwmk;
	uint32_t t;
	C3PNT *p;

	h->outlen = snprintf( h->outbuf, MAX_PKT_OUT,
					"%s %ld %ld %s %d\n",
					q->path->str,
					q->res.from, q->res.to,
					c3db_metric_name( q->rtype ),
					q->res.count );

	// send that header
	net_write_data( h );

	p    = q->res.points;
	hwmk = MAX_PKT_OUT - 66;
	max  = q->res.count - 1;

	// sync to the period boundaries
	t = q->start - ( q->start % q->res.period );

	for( j = 0; t < q->end; t += q->res.period, p++, j++ )
	{
		if( p->ts == t )
			h->outlen += snprintf( h->outbuf + h->outlen, 64, "[%u,%6f]%s",
							t, p->val, ( j < max ) ? "," : "" );
		else
			h->outlen += snprintf( h->outbuf + h->outlen, 64, "[%u,null]%s",
							t, ( j < max ) ? "," : "" );

		if( h->outlen > hwmk )
		{
			// add a newline
			h->outbuf[h->outlen++] = '\n';
			h->outbuf[h->outlen]   = '\0';

			net_write_data( h );
		}
	}

	if( h->outlen )
	{
		// add a newline
		h->outbuf[h->outlen++] = '\n';
		h->outbuf[h->outlen]   = '\0';

		net_write_data( h );
	}

	return 0;
}




int query_send_result( HOST *h, QUERY *q )
{
	switch( q->format )
	{
		case QUERY_FMT_FIELDS:
			return query_send_fields( h, q );
		case QUERY_FMT_JSON:
			return json_send_result( h, q );
	}

	qwarn( "Cannot send request in format %d", q->format );
	return 0;
}



char *query_format_strings[QUERY_FMT_MAX] =
{
	"fields",
	"json"
};


char *query_format_name( int type )
{
	if( type > QUERY_FMT_INVALID
	 && type < QUERY_FMT_MAX )
		return query_format_strings[type];

	return "invalid";
}


int query_format_type( char *str )
{
	int i;

	for( i = 0; i < QUERY_FMT_MAX; i++ )
		if( !strcasecmp( str, query_format_strings[i] ) )
			return i;

	return QUERY_FMT_INVALID;
}




QUERY *query_read( HOST *h )
{
	QUERY *q, *list;
	int i, len;
	char *wd;

	list = NULL;

	while( net_read_data( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			wd  = h->all->wd[i];
			len = h->all->len[i];

			strwords( h->val, wd, len, FIELD_SEPARATOR );

			// format is
			// path from to [rtype [format]]
			if( h->val->wc < QUERY_FIELD_METRIC )
			{
				qdebug( "Invalid line from query host %s", h->name );
				continue;
			}

			q        = mem_new_query( );
			q->path  = mem_new_path( h->val->wd[QUERY_FIELD_PATH],
			                         h->val->len[QUERY_FIELD_PATH] );
			q->start = (time_t) strtoul( h->val->wd[QUERY_FIELD_FROM], NULL, 10 );
			q->end   = (time_t) strtoul( h->val->wd[QUERY_FIELD_TO],   NULL, 10 );

			// do we have a metric?
			if( h->val->wc >= QUERY_FIELD_METRIC )
				q->rtype = c3db_metric( h->val->wd[QUERY_FIELD_METRIC] );
			else
				q->rtype = C3DB_REQ_MEAN;

			// do we have a format?
			if( h->val->wc >= QUERY_FIELD_FORMAT )
				q->format = query_format_type( h->val->wd[QUERY_FIELD_FORMAT] );
			else
				q->format = QUERY_FMT_FIELDS;

			// did we get sensible strings?
			if( q->rtype  == C3DB_REQ_INVLD
			 || q->format == QUERY_FMT_INVALID )
			{
				qwarn( "Invalid type or format from host %s", h->name );
				mem_free_query( &q );
				continue;
			}

			if( find_cached_node( q ) != 0 )
			{
				if( data_path_parse( q->path ) <= 0 )
				{
					qinfo( "Invalid path string: '%s'",
						h->val->wd[QUERY_FIELD_PATH] );
					mem_free_query( &q );
					continue;
				}

				if( find_node( q, ctl->node->nodes ) != 0 )
				{
					qwarn( "Query for unknown node '%s'",
						q->path->str );
					mem_free_query( &q );
					continue;
				}
			}

			q->next = list;
			list    = q;
		}

	return list;
}



POINT *get_cached_points( NODE *n )
{
	POINT *p, *q, *list;
	int i;

	node_lock( n );

	if( !n->waiting )
	{
		node_unlock( n );
		return NULL;
	}

	list = mem_new_points( n->waiting );

	for( p = list, q = n->incoming, i = 0; i < n->waiting; i++ )
	{
		if( q->data.ts )
		{
			p->data.ts  = q->data.ts;
			p->data.val = q->data.val;
			q           = q->next;
		}	
		p = p->next;
	}

	node_unlock( n );

	return list;
}



int query_handle( QUERY *q )
{
	C3HDL *h;
	NODE *n;

	n = q->node;

	// force flush the node
	if( n->waiting )
		sync_single_node( n );

	// then open and read
	h = c3db_open( n->dir_path, C3DB_RO );
	if( c3db_status( h ) )
	{
		qwarn( "Could not open node %s -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		return -1;
	}

	// impose our config
	if( c3db_read( h, q->start, q->end, q->rtype, &(q->res) ) )
	{
		qwarn( "Unable to read node %s -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		return -2;
	}

	// and done
	c3db_close( h );
	return 0;
}



void *query_connection( void *arg )
{
  	THRD *t = (THRD *) arg;
	struct pollfd p;
	QUERY *list, *q;
	HOST *h;
	int rv;

	h        = (HOST *) t->arg;
	p.fd     = h->fd;
	p.events = POLL_EVENTS;

	qinfo( "Query connection from %s", h->name );

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				qwarn( "Error polling query connection from %s -- %s",
						h->name, Err );
				h->flags |= HOST_CLOSE;
				break;
			}
			continue;
		}
		else if( !rv )
			continue;

		// we what we have
		if( !( list = query_read( h ) ) )
		{
			if( h->flags & HOST_CLOSE )
				break;

			continue;
		}

		while( list )
		{
			q    = list;
			list = q->next;

			qdebug( "Found a query to handle." );

			if( query_handle( q ) == 0 )
				query_send_result( h, q );
		}

		// error?
		if( h->flags & HOST_CLOSE )
			break;
	}

	// all done
	if( shutdown( h->fd, SHUT_RDWR ) )
		qerr( "Shutdown error on host %s -- %s",
			h->name, Err );
	close( h->fd );
	h->fd = -1;
	mem_free_host( &h );

	free( t );
	return NULL;
}



void *query_loop( void *arg )
{
	NET_TYPE_CTL *ntc;
	struct pollfd p;
	PORT_CTL *pc;
	HOST *h;
	THRD *t;
	int rv;

	t   = (THRD *) arg;
	ntc = (NET_TYPE_CTL *) t->arg;
	pc  = ntc->query;

	p.fd     = pc->sock;
	p.events = POLL_EVENTS;

	while( ctl->run_flags & RUN_LOOP )
	{
	  	// this value is all about responsiveness to shutdown
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
		  	if( errno != EINTR )
			{
				qerr( "Poll error on query socket -- %s", Err );
				loop_end( "polling error on query hosts" );
				break;
			}

			continue;
		}
		else if( !rv )
			continue;

		if( p.revents & POLL_EVENTS )
		{
			if( ( h = net_get_host( p.fd, ntc->type ) ) )
				throw_thread( query_connection, h );
		}
	}

	// TODO
	// say we are shut down


	free( t );
	return NULL;
}

