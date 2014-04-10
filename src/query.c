#include "coal.h"

int find_cached_node( QUERY *q )
{
	uint32_t sum, hval;
	PCACHE *pc;

	// . is the root node
	if( q->path->len  == 1 &&
	  *(q->path->str) == '.' )
	{
		q->node = ctl->node->nodes;
		return 0;
	}

	sum  = data_path_cksum( q->path->str, q->path->len );
	hval = sum % ctl->node->pcache_sz;

	for( pc = ctl->node->pcache[hval]; pc; pc = pc->next )
		if( pc->sum == sum
		 && pc->len == q->path->len
		 && !memcmp( q->path->str, pc->path, pc->len ) )
		{
			q->node = pc->target.node;
			return 0;
		}

	return -1;
}



int find_node( QUERY *q, NODE *parent )
{
	char *wd;
	int len;
	NODE *n;
	PATH *p;

	p   = q->path;
	wd  = p->w->wd[p->curr];
	len = p->w->len[p->curr];

	// move on because either we finish here,
	// or we are going down to the next string
	p->curr++;

	for( n = parent->children; n; n = n->next )
		if( n->name_len == len && !memcmp( n->name, wd, len ) )
		{
			if( n->flags & NODE_FLAG_ERROR )
				return -1;

			// are we there yet?
			if( p->curr < p->w->wc )
				return find_node( q, n );

			if( !q->tree && !( n->flags & NODE_FLAG_LEAF ) )
			{
				qwarn( "Trying to data query a branch node %s",
					n->dir_path );
				return -1;
			}

			q->node = n;
			return 0;
		}

	return -1;
}



int query_send_bin_children( NSOCK *s, QUERY *q )
{
	uint8_t *uc;
	uint32_t *ui, sz;
	uint16_t *us, j;
	NODE *n;

	// calculate the size
	for( sz = 12, j = 0, n = q->node->children; n; n = n->next )
	{
		sz += 5 + n->name_len;
		j++;
	}

	// write out our header - 24 bytes
	uc    = s->out.buf;
	*uc++ = 0x01;
	*uc++ = BINF_TYPE_TREE_RET;
	*uc++ = 0;   // query results don't have size here
	*uc++ = 0;   // because it may not fit in 64k
	ui    = (uint32_t *) uc;
	*ui++ = sz;
	uc    = (uint8_t *) ui;
	*uc++ = node_leaf_int( q->node );
	*uc++ = 0;   // padding
	us    = (uint16_t *) uc;
	*us++ = j;

	// now we're into sizes
	uc    = (uint8_t *) us;

	// write out the lengths
	for( n = q->node->children; n; n = n->next )
	{
		*uc++ = node_leaf_int( n );
		*uc++ = 0;   // padding
		us    = (uint16_t *) uc;
		*us++ = (uint16_t) n->name_len;
		uc    = (uint8_t *) us;

		if( uc > s->out.hwmk )
		{
			s->out.len = uc - s->out.buf;
			net_write_data( s );
			uc = s->out.buf;
		}
	}

	// write out the paths
	for( n = q->node->children; n; n = n->next )
	{
		// better to do this preemptively for the paths
		if( ( uc + n->name_len ) > s->out.hwmk )
		{
			s->out.len = uc - s->out.buf;
			net_write_data( s );
			uc = s->out.buf;
		}

		// there's already a null at the end
		memcpy( uc, n->name, n->name_len + 1 );
		uc += n->name_len + 1;
	}

	// add any additional empty space for alignment
	s->out.len = uc - s->out.buf;
	while( s->out.len % 4 )
	{
		*uc++ = '\0';
		s->out.len++;
	}

	// write any remainder
	if( s->out.len > 0 )
		net_write_data( s );

	return 0;
}




int query_send_bin_result( NSOCK *s, QUERY *q )
{
	uint32_t *ui;
	uint16_t *us;
	uint8_t *uc;
	float *fp;
	time_t t;
	C3PNT *p;
	int j;

	// write out our header - 24 bytes
	uc    = s->out.buf;
	*uc++ = 0x01;
	*uc++ = BINF_TYPE_QUERY_RET;
	*uc++ = 0;   // query results don't have size here
	*uc++ = 0;   // because it may not fit in 64k
	ui    = (uint32_t *) uc;
	*ui++ = 24 + ( 12 * q->res.count ) + q->path->len + 1;
	*ui++ = (uint32_t) q->res.from;
	*ui++ = (uint32_t) q->res.to;
	uc    = (uint8_t *) ui;
	*uc++ = (uint8_t) q->rtype;
	*uc++ = 0;   // padding
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) q->path->len;
	ui    = (uint32_t *) us;
	*ui++ = (uint32_t) q->res.count;


	// sync to the period boundary
	t = q->start - ( q->start % q->res.period );
	p = q->res.points;

	for( j = 0; t < q->end; t += q->res.period, p++, j++ )
	{
		*ui++ = (uint32_t) t;
		if( p->ts == t )
		{
			*ui++ = 0x1;
			fp    = (float *) ui++;
			*fp   = p->val;
		}
		else
		{
			*ui++ = 0;
			*ui++ = 0;
		}

		uc = (uint8_t *) ui;
		if( uc > s->out.hwmk )
		{
			s->out.len = uc - s->out.buf;
			net_write_data( s );
			ui = (uint32_t *) s->out.buf;
		}
	}

	uc = (uint8_t *) ui;

	// is there enough room on the end for the path
	if( ( uc + q->path->len ) > s->out.hwmk )
	{
		s->out.len = uc - s->out.buf;
		net_write_data( s );
		uc = s->out.buf;
	}

	// and write the path on the end
	memcpy( uc, q->path->str, q->path->len + 1 );

	// step over and write some empty space
	uc += q->path->len + 1;
	s->out.len = uc - s->out.buf;
	while( s->out.len % 4 )
	{
		*uc++ = '\0';
		s->out.len++;
	}

	net_write_data( s );

	return 0;
}




int query_send_children( NSOCK *s, QUERY *q )
{
	char *to;
	NODE *n;
	int i;

	for( i = 0, n = q->node->children; n; n = n->next, i++ );

	to  = (char *) s->out.buf;
	to += snprintf( to, s->out.sz, "%s 1 0 0 tree 0 %d %d\n",
	               q->path->str, node_leaf_int( q->node ), i );

	for( n = q->node->children; n; n = n->next )
	{
		to += snprintf( to, 1024, "%s,%s\n",
			( n->flags & NODE_FLAG_LEAF ) ? "leaf" : "branch", n->name );

		if( to > (char *) s->out.hwmk )
		{
			s->out.len = to - (char *) s->out.buf;
			net_write_data( s );
			to = (char *) s->out.buf;
		}
	}

	if( to > (char *) s->out.buf )
	{
	  	s->out.len = to - (char *) s->out.buf;
		net_write_data( s );
	}

	return 0;
}



int query_send_fields( NSOCK *s, QUERY *q )
{
	uint32_t t;
	C3PNT *p;
	char *to;
	int j;

	to  = (char *) s->out.buf;
	to += snprintf( to, 1024, "%s 0 %ld %ld %s %d %d %d\n",
	                q->path->str, q->res.from, q->res.to,
	                c3db_metric_name( q->rtype ),
	                q->res.period, node_leaf_int( q->node ),
	                q->res.count );

	p = q->res.points;

	// sync to the period boundaries
	t = q->start - ( q->start % q->res.period );

	for( j = 0; t < q->end; t += q->res.period, p++, j++ )
	{
		if( p->ts == t )
			to += snprintf( to, 64, "%u,%6f\n", t, p->val );
		else
			to += snprintf( to, 64, "%u,null\n", t );

		if( to > (char *) s->out.hwmk )
		{
			s->out.len = to - (char *) s->out.buf;
			net_write_data( s );
			to = (char *) s->out.buf;
		}
	}

	if( to > (char *) s->out.buf )
	{
	  	s->out.len = to - (char *) s->out.buf;
		net_write_data( s );
	}

	return 0;
}




int query_send_result( HOST *h, QUERY *q )
{
	switch( q->format )
	{
		case QUERY_FMT_FIELDS:
			return query_send_fields( h->net, q );
		case QUERY_FMT_JSON:
			return json_send_result( h->net, q );
		case QUERY_FMT_BIN:
			return query_send_bin_result( h->net, q );
	}

	qwarn( "Cannot send query result in format %d", q->format );
	return 0;
}


int query_send_tree( HOST *h, QUERY *q )
{
	switch( q->format )
	{
		case QUERY_FMT_FIELDS:
			return query_send_children( h->net, q );
		case QUERY_FMT_JSON:
			return json_send_children( h->net, q );
		case QUERY_FMT_BIN:
			return query_send_bin_children( h->net, q );
	}

	qwarn( "Cannot send tree result in format %d", q->format );
	return 0;
}




char *query_format_strings[QUERY_FMT_MAX] =
{
	"fields",
	"json",
	"bin"
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


QUERY *query_bin_read( HOST *h )
{
	int i, len, type;
	QUERY *q, *list;
	void *buf;

	list = NULL;

	while( net_read_bin( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			// each of these is a binary chunk
			buf = h->all->wd[i];
			len = h->all->len[i];

			type = (int) *((uint8_t *) ( buf + 1 ));

			if( type != BINF_TYPE_QUERY
			 && type != BINF_TYPE_TREE )
			{
				warn( "Received type %d/%s from host %s on query bin connection.",
					type, data_bin_type_names( type ), h->net->name );
				h->net->flags |= HOST_CLOSE;
				return NULL;
			}

			q         = mem_new_query( );
			q->format = QUERY_FMT_BIN;

			if( type == BINF_TYPE_TREE )
				q->tree = 1;
			else
			{
				q->start = *((time_t *) ( buf + 4 ));
				q->end   = *((time_t *) ( buf + 8 ));
				q->rtype = (int) *((uint8_t *) ( buf + 12 ));

				if( q->rtype <= C3DB_REQ_INVLD || q->rtype >= C3DB_REQ_END )
				{
					qwarn( "Invalid metric %d in query from host %s",
						q->rtype, h->net->name );
					mem_free_query( &q );
					continue;
				}

				// do some timestamp checks
				if( !q->start )
					// you don't really mean all time, do you?
					// an informal way of saying '24hrs please'
					q->start = (time_t) ( ctl->curr_time - 86400.0 );

				if( !q->end )
					// an informal way of saying 'now'
					q->end = (time_t) ctl->curr_time;
				else if( q->end < q->start )
				{
					qwarn( "End < start in query from host %s", h->net->name );
					mem_free_query( &q );
					continue;
				}
			}

			q->path = mem_new_path( (char *) ( buf + 13 ), len - 14 );


			if( find_cached_node( q ) != 0 )
			{
				if( data_path_parse( q->path ) <= 0 )
				{
					qinfo( "Invalid path string: '%s'",
						(char *) buf + 13 );
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


QUERY *query_line_read( HOST *h )
{
	QUERY *q, *list;
	int i, len;
	char *wd;

	list = NULL;

	while( net_read_lines( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			wd  = h->all->wd[i];
			len = h->all->len[i];

			qdebug( "Received query from host %s: (%d) %s",
				h->net->name, len, wd );

			// we have very variable requirements here
			if( strwords( h->val, wd, len, FIELD_SEPARATOR ) < QUERY_FIELD_COUNT )
			{
				qinfo( "Invalid line from query host %s", h->net->name );
				continue;
			}

			// create a new with some defaults
			q         = mem_new_query( );
			q->path   = mem_new_path( h->val->wd[QUERY_FIELD_PATH],
			                          h->val->len[QUERY_FIELD_PATH] );
			q->format = query_format_type( h->val->wd[QUERY_FIELD_FORMAT] );
			q->end    = (time_t) strtoul( h->val->wd[QUERY_FIELD_END], NULL, 10 );
			q->start  = (time_t) strtoul( h->val->wd[QUERY_FIELD_START], NULL, 10 );

			// we can get metric 'tree'
			if( !strcmp( h->val->wd[QUERY_FIELD_METRIC], "tree" ) )
			{
				q->tree  = 1;
				q->rtype = C3DB_REQ_INVLD;
			}
			else
			{
				q->rtype = c3db_metric( h->val->wd[QUERY_FIELD_METRIC] );

				// do some checks on the timestamps
				if( !q->start ) {
					// you don't really mean all time, do you?
					// an informal way of saying '24hrs please'
					q->start = (time_t) ( ctl->curr_time - 86400.0 );
				}

				if( q->end == 0 ) {
					// an informal way of saying 'now'
					q->end = (time_t) ctl->curr_time;
				} else if( q->end < q->start ) {
					qwarn( "End < start in query from host %s", h->net->name );
					mem_free_query( &q );
					continue;
				}
			}


			// did we get sensible strings?
			if( ( !q->tree && q->rtype == C3DB_REQ_INVLD )
			 || q->format == QUERY_FMT_INVALID )
			{
				qwarn( "Invalid type or format from host %s", h->net->name );
				mem_free_query( &q );
				continue;
			}

			// cannot send binary format answers on this connection
			if( q->format == QUERY_FMT_BIN )
			{
				qwarn( "Asked for binary format query answer on a line format connection from host %s",
					h->net->name );
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





int query_data( QUERY *q )
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



void query_bin_connection( HOST *h )
{
	struct pollfd p;
	QUERY *list, *q;
	int rv;

	p.fd     = h->net->sock;
	p.events = POLL_EVENTS;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				qwarn( "Error polling query connection from %s -- %s",
						h->net->name, Err );
				h->net->flags |= HOST_CLOSE;
				break;
			}
			continue;
		}
		else if( !rv )
			continue;

		// we what we have
		if( !( list = query_line_read( h ) ) )
		{
			if( h->net->flags & HOST_CLOSE )
				break;

			continue;
		}

		while( list )
		{
			q    = list;
			list = q->next;

			qdebug( "Found a query to handle." );

			if( q->tree )
			{
				// we've already found the node
				query_send_tree( h, q );
			}
			else
			{
				if( query_data( q ) == 0 )
					query_send_result( h, q );
			}
		}

		// error?
		if( h->net->flags & HOST_CLOSE )
			break;
	}
}


void query_line_connection( HOST *h )
{
	struct pollfd p;
	QUERY *list, *q;
	int rv;

	p.fd     = h->net->sock;
	p.events = POLL_EVENTS;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			if( errno != EINTR )
			{
				qwarn( "Error polling query connection from %s -- %s",
						h->net->name, Err );
				h->net->flags |= HOST_CLOSE;
				break;
			}
			continue;
		}
		else if( !rv )
			continue;

		// we what we have
		if( !( list = query_line_read( h ) ) )
		{
			if( h->net->flags & HOST_CLOSE )
				break;

			continue;
		}

		while( list )
		{
			q    = list;
			list = q->next;

			qdebug( "Found a query to handle." );

			if( q->tree )
			{
				// we've already found the node
				query_send_result( h, q );
			}
			else
			{
				if( query_data( q ) == 0 )
					query_send_result( h, q );
			}
		}

		// error?
		if( h->net->flags & HOST_CLOSE )
			break;
	}
}



void *query_connection( void *arg )
{
	THRD *t;
	HOST *h;

	t = (THRD *) arg;
	h = (HOST *) t->arg;

	info( "Accepted query connection from host %s", h->net->name );

	switch( h->type )
	{
		case NET_COMM_LINE:
			query_line_connection( h );
		case NET_COMM_BIN:
			query_bin_connection( h );
		default:
			break;
	}

	// all done
	if( shutdown( h->net->sock, SHUT_RDWR ) )
		qerr( "Shutdown error on host %s -- %s",
			h->net->name, Err );
	close( h->net->sock );
	h->net->sock = -1;
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
				thread_throw( query_connection, h );
		}
	}

	// TODO
	// say we are shut down


	free( t );
	return NULL;
}


