#include "coal.h"

#define LLFID LLFOL


#define out_lines_header( _q, fm, to, pr, mt, lf, pa )	\
	snprintf( buf, s->out.sz, "%s %u %u %d %s %hhu %d %s\n", \
	query_type_get_name( _q ), (uint32_t) fm, (uint32_t) to, pr, mt, \
	(uint8_t) lf, (_q)->rcount, pa )


int out_lines_search( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	int i;

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_lines_header( q, 0, 0, 0, "none", 0, q->path->str );

	for( i = 0; i < q->rcount; i++ )
	{
		to += snprintf( to, 1024, "%s\n", q->nodes[i]->dir_path );
		buf_check( to, hwmk );
	}

	buf_check( to, buf );

	return 0;
}


int out_lines_tree( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	NODE *n;
	int i;

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_lines_header( q, 0, 0, 0, "none", node_leaf_int( q->node ), q->path->str );

	for( i = 0; i < q->rcount; i++ )
	{
		n = q->nodes[i];

		to += snprintf( to, 1024, "%s,%s\n",
			( n->flags & NODE_FLAG_LEAF ) ? "leaf" : "branch", n->name );

		buf_check( to, hwmk );
	}

	buf_check( to, buf );

	return 0;
}



int out_lines_data( NSOCK *s, QUERY *q )
{
	char *to, *buf, *hwmk;
	uint32_t t;
	C3PNT *p;
	C3RES *r;
	int j;

	r = q->results;
	p = r->points;

	hwmk = (char *) s->out.hwmk;
	buf  = (char *) s->out.buf;
	to   = buf + out_lines_header( q, r->from, r->to, r->period,
	              c3db_metric_name( q->rtype ), node_leaf_int( q->node ),
	              q->path->str );

	// sync to the period boundaries
	t = q->start - ( q->start % q->results->period );

	for( j = 0; t < q->end; t += q->results->period, p++, j++ )
	{
		if( p->ts == t )
			to += snprintf( to, 64, "%u,%6f\n", t, p->val );
		else
			to += snprintf( to, 64, "%u,null\n", t );

		buf_check( to, hwmk );
	}

	buf_check( to, buf );

	return 0;
}


int out_lines_invalid( NSOCK *s, QUERY *q )
{
	char *to, *buf;

	q->type = QUERY_TYPE_INVALID;

	buf = (char *) s->out.buf;
	to  = buf + out_lines_header( q, 0, 0, 0, "none", 0, "none" );

	to  += snprintf( buf, 4096, "%s\n", q->error );

	buf_check( to, buf );

	return 0;
}


#undef out_lines_header

#undef LLFID
