#include "coal.h"




int json_send_children( HOST *h, QUERY *q )
{
	int i, hwmk;
	NODE *n;

	for( i = 0, n = q->node->children; n; n = n->next, i++ );

	h->outlen = snprintf( h->outbuf, MAX_PKT_OUT,
		"{\"path\":\"%s\",\"type\":\"%s\",\"count\":%d,\"nodes\":[",
		q->path->str,
		node_leaf_str( q->node ),
		i );

	net_write_data( h );

	hwmk = MAX_PKT_OUT - 134;

	for( i = 0, n = q->node->children; n; n = n->next, i++ )
	{
		h->outlen += snprintf( h->outbuf + h->outlen, 128, "%s[%s,%s]",
			( i ) ? "," : "",
			( n->flags & NODE_FLAG_LEAF ) ? "leaf" : "branch",
			n->name );

		if( h->outlen > hwmk )
			net_write_data( h );
	}

	// add closing braces
	h->outlen += snprintf( h->outbuf + h->outlen, 4, "]}\n" );
	net_write_data( h );

	return 0;
}



int json_send_result( HOST *h, QUERY *q )
{
	int start, hwmk;
	uint32_t t;
	C3PNT *p;

	if( q->tree )
		return json_send_children( h, q );

	h->outlen = snprintf( h->outbuf, MAX_PKT_OUT,
		"{\"path\":\"%s\",\"start\":%ld,\"end\":%ld,\"count\":%d,\"period\":%d,\"metric\":\"%s\",\"values\":[",
		q->path->str, q->start, q->end, q->res.count, q->res.period,
		c3db_metric_name( q->rtype ) );

	// write that
	net_write_data( h );

	p    = q->res.points;
	hwmk = MAX_PKT_OUT - 70;
	t    = q->start - ( q->start % q->res.period );

	for( ; t < q->end; t += q->res.period, p++ )
	{
		if( p->ts == t )
			h->outlen += snprintf( h->outbuf + h->outlen, 64, "%s[%u,%6f]",
					( start ) ? "" : ",", t, p->val );
		else
			h->outlen += snprintf( h->outbuf + h->outlen, 64, "%s[%u,null]",
					( start ) ? "" : ",", t );

		start = 0;

		if( h->outlen > hwmk )
		{
			net_write_data( h );
			h->outlen = 0;
		}
	}

	// add closing braces
	h->outlen += snprintf( h->outbuf + h->outlen, 4, "]}\n" );
	net_write_data( h );

	return 0;
}

