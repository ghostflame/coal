#include "coal.h"

#define LLFID LLFQU

const char *query_type_name_strings[QUERY_TYPE_MAX] = {
	"data",
	"search",
	"tree"
};


// used to access the output functions
qoutput_fn *query_output_function_pointers[QUERY_FMT_MAX][QUERY_TYPE_MAX] = {
	{	&out_lines_data,	&out_lines_tree,	&out_lines_search	},
	{	&out_json_data,		&out_json_tree,		&out_json_search	},
	{	&out_bin_data,		&out_bin_tree,		&out_bin_search		}
};



const char *query_type_names( int type )
{
	if( type > QUERY_TYPE_INVALID
	 && type < QUERY_TYPE_MAX )
		return query_type_name_strings[type];

	return "invalid";
}

int query_get_type( char *str )
{
	int i;

	for( i = 0; i < QUERY_TYPE_MAX; i++ )
		if( !strcasecmp( str, query_type_name_strings[i] ) )
			return i;

	return QUERY_TYPE_INVALID;
}

int query_min_fields( int type )
{
	switch( type )
	{
		case QUERY_TYPE_DATA:
			return 6;
		case QUERY_TYPE_SEARCH:
		case QUERY_TYPE_TREE:
			return 3;
	}

	// invalid queries always fail
	return 1000000;
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




// make sure the times are OK
int query_align_times( QUERY *q )
{
	// do some timestamp checks
	if( !q->start )
		// you don't really mean all time, do you?
		// an informal way of saying '24hrs please'
		q->start = (time_t) ( ctl->curr_time - 86400.0 );

	if( !q->end )
		// an informal way of saying 'now'
		q->end = (time_t) ctl->curr_time;

	return ( q->end - q->start );
}


// look up nodes for the query
int query_find_node( QUERY *q )
{
	if( q->type == QUERY_TYPE_SEARCH )
	{
		// make a regex
		if( regcomp( &(q->search), q->path->str, REG_EXTENDED|REG_ICASE|REG_NOSUB ) )
		{
			qwarn( 0x0501, "Invalid query search expression: '%s'", q->path->str );
			return -1;
		}

		return 0;
	}


	// fine, we go looking the hard way
	if( !( q->node = node_find( q->path ) ) )
	{
		qinfo( 0x0502, "Query for unknown node: '%s'", q->path->str );
		return -1;
	}

	return 0;
}



QUERY *query_bin_read( HOST *h )
{
	int i, len, type, qtype;
	QUERY *q, *list;
	uint32_t *ui;
	uint8_t *uc;
	void *buf;

	list = NULL;

	while( net_read_bin( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			// each of these is a binary chunk
			buf   = h->all->wd[i];
			len   = h->all->len[i];

			ui    = (uint32_t *) buf;
			uc    = (uint8_t *) buf;

			type  = *(uc + 1);
			qtype = *(uc + 12);

			if( type != BINF_TYPE_QUERY )
			{
				qwarn( 0x0601, "Received type %d/%s from host %s on query bin connection.",
					type, data_bin_type_names( type ), h->net->name );
				h->net->flags |= HOST_CLOSE;
				return NULL;
			}
			if( qtype <= QUERY_TYPE_INVALID
			 || qtype >= QUERY_TYPE_MAX )
			{
				qwarn( 0x0602, "Received query type %d from host %s on query bin connection.",
					qtype, h->net->name );
				// never mind
				continue;
			}

			q         = mem_new_query( );
			q->format = QUERY_FMT_BIN;
			q->type   = qtype;


			if( q->type == QUERY_TYPE_DATA )
			{
				q->start = (time_t) *(ui + 1);
				q->end   = (time_t) *(ui + 2);
				q->rtype = (int) *(uc + 13);

				if( q->rtype <= C3DB_REQ_INVLD || q->rtype >= C3DB_REQ_END )
				{
					qwarn( 0x0603, "Invalid metric %d in query from host %s",
						q->rtype, h->net->name );
					mem_free_query( &q );
					continue;
				}

				if( query_align_times( q ) < 0 )
				{
					qwarn( 0x0604, "End < start in query from host %s", h->net->name );
					mem_free_query( &q );
					continue;
				}
			}

			q->path = mem_new_path( (char *) ( buf + 14 ), len - 15 );
			q->next = list;
			list    = q;
		}

	return list;
}


QUERY *query_line_read( HOST *h )
{
	int i, len, type, fmt;
	QUERY *q, *list;
	char *wd;

	list = NULL;

	while( net_read_lines( h ) > 0 )
		for( i = 0; i < h->all->wc; i++ )
		{
			wd  = h->all->wd[i];
			len = h->all->len[i];

			qdebug( 0x0701, "Received query from host %s: (%d) %s",
				h->net->name, len, wd );

			// we have very variable requirements here
			if( strwords( h->val, wd, len, FIELD_SEPARATOR ) < 3
			 || ( type = query_get_type( h->val->wd[QUERY_FIELD_TYPE] ) ) == QUERY_TYPE_INVALID
			 || h->val->wc < query_min_fields( type )
			 || ( fmt = query_format_type( h->val->wd[QUERY_FIELD_FORMAT] ) ) )
			{
				qinfo( 0x0702, "Invalid line from query host %s", h->net->name );
				continue;
			}

			// create a new with some defaults
			q         = mem_new_query( );
			q->type   = (int8_t) type;
			q->format = (int8_t) fmt;

			if( type == QUERY_TYPE_TREE )
			{
				q->path = mem_new_path( h->val->wd[QUERY_FIELD_PATH], h->val->len[QUERY_FIELD_PATH] );
			}
			else if( type == QUERY_TYPE_DATA )
			{
				q->path   = mem_new_path( h->val->wd[QUERY_FIELD_PATH], h->val->len[QUERY_FIELD_PATH] );
				// read in the timestamp fields and type
				q->end    = (time_t) strtoul( h->val->wd[QUERY_FIELD_END], NULL, 10 );
				q->start  = (time_t) strtoul( h->val->wd[QUERY_FIELD_START], NULL, 10 );
				q->rtype  = c3db_metric( h->val->wd[QUERY_FIELD_METRIC] );

				// do some checks on the timestamps
				if( query_align_times( q ) < 0 )
				{
					qwarn( 0x0703, "End < start in query from host %s", h->net->name );
					mem_free_query( &q );
					continue;
				}
			}

			// cannot send binary format answers on this connection
			if( q->format == QUERY_FMT_BIN )
			{
				qwarn( 0x0704, "Asked for binary format query answer on a line format connection from host %s",
					h->net->name );
				mem_free_query( &q );
				continue;
			}

			q->next = list;
			list    = q;
		}

	return list;
}



void query_handle_connection( HOST *h )
{
	struct pollfd p;
	QUERY *list, *q;
	int rv;

	p.fd     = h->net->sock;
	p.events = POLL_EVENTS;
	list     = NULL;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
			// don't sweat an interrupted poll call
			if( errno == EINTR )
				continue;

			qwarn( 0x0901, "Error polling query connection from %s -- %s",
			       h->net->name, Err );
			h->net->flags |= HOST_CLOSE;
			break;
		}

		if( !rv )
			continue;

		// are they going away?
		if( p.revents & POLLHUP )
			break;

		// read some data if anything happened
		list = (*(h->qrf))( h );

		// see what we have
		while( list )
		{
			q       = list;
			list    = q->next;
			q->next = NULL;

			qdebug( 0x0902, "Found a query to handle." );

			// look up the node
			if( query_find_node( q ) != 0 )
			{
				mem_free_query( &q );
				continue;
			}

			switch( q->type )
			{
				case QUERY_TYPE_DATA:
					search_data( q );
					break;
				case QUERY_TYPE_TREE:
					search_tree( q );
					break;
				case QUERY_TYPE_SEARCH:
					search_nodes( q );
					break;
			}

			// access the right output function
			(*(query_output_function_pointers[q->format][q->type]))( h->net, q );
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

	qinfo( 0x0a01, "Accepted query connection from host %s", h->net->name );

	// make sure we can be cancelled
	pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
	pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

	query_handle_connection( h );

	net_close_host( h );

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

	loop_mark_start( );

	while( ctl->run_flags & RUN_LOOP )
	{
		// this value is all about responsiveness to shutdown
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
		  	if( errno != EINTR )
			{
				qerr( 0x0b01, "Poll error on query socket -- %s", Err );
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
				thread_throw_watched( query_connection, h );
		}
	}

	// say we are shut down
	loop_mark_done( );

	free( t );
	return NULL;
}



// nothing yet
int query_config_line( AVP *av )
{
	return 0;
}


QRY_CTL *query_config_defaults( void )
{
	QRY_CTL *q;

	q = (QRY_CTL *) allocz( sizeof( QRY_CTL ) );

	return q;
}


#undef LLFID

