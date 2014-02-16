#include "coal.h"


int json_send_result( HOST *h, QUERY *q )
{
	int start, hwmk;
	uint32_t t;
	C3PNT *p;

	h->outlen = snprintf( h->outbuf, MAX_PKT_OUT,
		"{\"from\":%ld,\"to\":%ld,\"count\":%d,\"period\":%d,\"metric\":\"%s\",\"values\":[",
		q->start, q->end, q->res.count, q->res.period,
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

