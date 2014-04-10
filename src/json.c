#include "coal.h"




int json_send_children( NSOCK *s, QUERY *q )
{
	char *to;
	int i;
	NODE *n;

	for( i = 0, n = q->node->children; n; n = n->next, i++ );

	to  = (char *) s->out.buf;
	to += snprintf( to, 1044,
		"{\"path\":\"%s\",\"type\":\"%s\",\"count\":%d,\"nodes\":[",
		q->path->str,
		node_leaf_str( q->node ),
		i );

	for( i = 0, n = q->node->children; n; n = n->next, i++ )
	{
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



int json_send_result( NSOCK *s, QUERY *q )
{
	uint32_t t;
	int start;
	char *to;
	C3PNT *p;

	if( q->tree )
		return json_send_children( s, q );

	to  = (char *) s->out.buf;
	to += snprintf( to, 1096,
		"{\"path\":\"%s\",\"start\":%ld,\"end\":%ld,\"count\":%d,\"period\":%d,\"metric\":\"%s\",\"values\":[",
		q->path->str, q->start, q->end, q->res.count, q->res.period,
		c3db_metric_name( q->rtype ) );

	p = q->res.points;
	t = q->start - ( q->start % q->res.period );

	for( ; t < q->end; t += q->res.period, p++ )
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

