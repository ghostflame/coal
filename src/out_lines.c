#include "coal.h"

#define LLFID LLFOL


int out_lines_search( NSOCK *s, QUERY *q )
{
	return 0;
}


int out_lines_tree( NSOCK *s, QUERY *q )
{
	char *to;
	NODE *n;
	int i;

	to  = (char *) s->out.buf;
	to += snprintf( to, s->out.sz, "%s 1 0 0 tree 0 %d %d\n",
	                q->path->str, node_leaf_int( q->node ),
	                q->rcount );

	for( i = 0; i < q->rcount; i++ )
	{
		n = q->nodes[i];

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



int out_lines_data( NSOCK *s, QUERY *q )
{
	uint32_t t;
	C3PNT *p;
	char *to;
	int j;

	to  = (char *) s->out.buf;
	to += snprintf( to, 1024, "%s 0 %ld %ld %s %d %d %d\n",
	                q->path->str, q->results->from, q->results->to,
	                c3db_metric_name( q->rtype ),
	                q->results->period, node_leaf_int( q->node ),
	                q->results->count );

	p = q->results->points;

	// sync to the period boundaries
	t = q->start - ( q->start % q->results->period );

	for( j = 0; t < q->end; t += q->results->period, p++, j++ )
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


#undef LLFID
