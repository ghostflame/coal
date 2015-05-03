#include "coal.h"

#define LLFID LLFJS

// yes, we hand-roll our JSON.  Why, how did you *think* we were going to do it?





int out_json_header( char *buf, QUERY *q, int period, int leaf, char *metric, char *pkey )
{
	int l, sz;

	l  = 4096;
	sz = 0;
	
	sz += snprintf( buf, l, "\"header\":{\"type\":\"%s\",",
	                query_type_get_name( q ) );

	if( q->start )
		sz += snprintf( buf + sz, l - sz, "\"start\":%u,", (uint32_t) q->start );
	if( q->end )
		sz += snprintf( buf + sz, l - sz, "\"end\":%u,", (uint32_t) q->end );
	if( period )
		sz += snprintf( buf + sz, l - sz, "\"period\":%d,", period );
	if( metric )
		sz += snprintf( buf + sz, l - sz, "\"metric\":\"%s\",", metric );
	if( leaf > -1 )
		sz += snprintf( buf + sz, l - sz, "\"leaf\":%d,", leaf );
	if( pkey )
		sz += snprintf( buf + sz, l - sz, "\"%s\":\"%s\",", pkey, q->path->str );

	sz += snprintf( buf + sz, l - sz, "\"count\":%d},", q->rcount );

	return sz;
}


int out_json_tree( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	NODE *n;
	int i;

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_json_header( buf, q, 0, node_leaf_int( q->node ), NULL, "path" );

	to   += snprintf( to, 256, "\"nodes\":[" );

	for( i = 0; i < q->rcount; i++ )
	{
		n   = q->nodes[i];
		to += snprintf( to, 1024, "%s[%d,%s]", ( i ) ? "," : "",
		                node_leaf_int( n ), n->name );

		buf_check( to, hwmk );
	}

	// add closing braces
	to += snprintf( to, 4, "]}\n" );

	// this will always happen
	buf_check( to, buf );

	return 0;
}



int out_json_data( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	uint32_t t;
	int start;
	C3PNT *p;
	C3RES *r;

	r = q->results;
	p = r->points;
	t = q->start - ( q->start % r->period );

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_json_header( buf, q, r->period, -1, c3db_metric_name( q->rtype ), "path" );

	to  += snprintf( to, 256, "\"values\":[" );

	for( ; t < q->end; t += r->period, p++ )
	{
		if( p->ts == t )
			to += snprintf( to, 64, "%s[%u,%6f]",
					( start ) ? "" : ",", t, p->val );
		else
			to += snprintf( to, 64, "%s[%u,null]",
					( start ) ? "" : ",", t );

		start = 0;

		buf_check( to, hwmk );
	}

	// add closing braces
	to += snprintf( to, 4, "]}\n" );

	buf_check( to, buf );

	return 0;
}


int out_json_search( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	int i;

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_json_header( buf, q, 0, -1, NULL, "search" );

	to  += snprintf( to, 256, "\"paths\":[" );

	for( i = 0; i < q->rcount; i++ )
	{
		to += snprintf( to, 1024, "%s%s", ( i ) ? "," : "", q->nodes[i]->dir_path );

		buf_check( to, hwmk );
	}

	// add closing braces
	to += snprintf( to, 4, "]}\n" );

	buf_check( to, buf );

	return 0;
}


int out_json_invalid( NSOCK *s, QUERY *q )
{
	char *to, *buf;

	buf = (char *) s->out.buf;
	to  = buf + out_json_header( buf, q, 0, -1, NULL, NULL );

	to += snprintf( to, 4120, "\"error\":\"%s\"}\n", q->error );

	buf_check( to, buf );

	return 0;
}


#undef LLFID

