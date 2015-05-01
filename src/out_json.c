#include "coal.h"

#define LLFID LLFJS


int out_json_tree( NSOCK *s, QUERY *q )
{
	char *to;
	int i;
	NODE *n;


	to  = (char *) s->out.buf;
	to += snprintf( to, 1044,
		"{\"path\":\"%s\",\"type\":\"%s\",\"count\":%d,\"nodes\":[",
		q->path->str,
		node_leaf_str( q->node ),
		q->rcount );

	for( i = 0; i < q->rcount; i++ )
	{
		n = q->nodes[i];

		to += snprintf( to, 1024, "%s[%s,%s]",
			( i ) ? "," : "",
			( n->flags & NODE_FLAG_LEAF ) ? "leaf" : "branch",
			n->name );

		if( to > (char *) s->out.hwmk )
		{
			s->out.len = to - (char *) s->out.buf;
			net_write_data( s );
			to = (char *) s->out.buf;
		}
	}

	// add closing braces
	to += snprintf( to, 4, "]}\n" );
	s->out.len = to - (char *) s->out.buf;
	net_write_data( s );

	return 0;
}



int out_json_data( NSOCK *s, QUERY *q )
{
	uint32_t t;
	int start;
	char *to;
	C3PNT *p;

	to  = (char *) s->out.buf;
	to += snprintf( to, 1096,
		"{\"path\":\"%s\",\"start\":%ld,\"end\":%ld,\"count\":%d,\"period\":%d,\"metric\":\"%s\",\"values\":[",
		q->path->str, q->start, q->end, q->results->count, q->results->period,
		c3db_metric_name( q->rtype ) );

	p = q->results->points;
	t = q->start - ( q->start % q->results->period );

	for( ; t < q->end; t += q->results->period, p++ )
	{
		if( p->ts == t )
			to += snprintf( to, 64, "%s[%u,%6f]",
					( start ) ? "" : ",", t, p->val );
		else
			to += snprintf( to, 64, "%s[%u,null]",
					( start ) ? "" : ",", t );

		start = 0;

		if( to > (char *) s->out.hwmk )
		{
			s->out.len = to - (char *) s->out.buf;
			net_write_data( s );
			to = (char *) s->out.buf;
		}
	}

	// add closing braces
	to += snprintf( to, 4, "]}\n" );
	s->out.len = to - (char *) s->out.buf;
	net_write_data( s );

	return 0;
}


int out_json_search( NSOCK *s, QUERY *q )
{
	return 0;
}


#undef LLFID

