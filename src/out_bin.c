#include "coal.h"

#define LLFID LLFOB

void out_bin_write_header( NSOCK *s, QUERY *q, uint32_t sz )
{
	uint32_t *ui;
	uint8_t *uc;

	uc    = s->out.buf;
	*uc++ = 0x01;
	*uc++ = BINF_TYPE_QUERY_RET;
	*uc++ = (uint8_t) q->type;
	*uc++ = q->id;

	ui    = (uint32_t *) uc;
	*ui   = sz;
}



int out_bin_search( NSOCK *s, QUERY *q )
{
	uint32_t *ui, sz, j;
	uint16_t *us;
	uint8_t *uc;
	NODE *n;

	// calculate the size
	for( sz = 12, j = 0; j < q->rcount; j++ )
		sz += 3 + q->nodes[j]->dpath_len;

	out_bin_write_header( s, q, sz );

	ui    = (uint32_t *) ( s->out.buf + 8 );
	*ui++ = q->rcount;

	// now sizes
	us    = (uint16_t *) ui;

	for( j = 0; j < q->rcount; j++ )
	{
		*us++ = q->nodes[j]->dpath_len;

		if( ((uint8_t *) us) > s->out.hwmk )
		{
			uc = (uint8_t *) us;
			s->out.len = uc - s->out.buf;
			net_write_data( s );
			us = (uint16_t *) s->out.buf;
		}
	}

	// now strings
	uc    = (uint8_t *) us;

	for( j = 0; j < q->rcount; j++ )
	{
		n = q->nodes[j];

		// better to do this preemptively for the paths
		if( ( uc + n->dpath_len ) > s->out.hwmk )
		{
			s->out.len = uc - s->out.buf;
			net_write_data( s );
			uc = s->out.buf;
		}

		// there's already a null at the end
		memcpy( uc, n->dir_path, n->dpath_len + 1 );
		uc += n->dpath_len + 1;
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


// write out the tree response
int out_bin_tree( NSOCK *s, QUERY *q )
{
	uint16_t *us, j;
	uint8_t *uc;
	uint32_t sz;
	NODE *n;

	// calculate the size
	for( sz = 12, j = 0; j < q->rcount; j++ )
		sz += 5 + q->nodes[j]->name_len;

	out_bin_write_header( s, q, sz );

	uc    = s->out.buf + 8;
	*uc++ = node_leaf_int( q->node );
	*uc++ = 0;   // padding
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) q->rcount;

	// now we're into sizes
	uc    = (uint8_t *) us;

	// write out the lengths
	for( j = 0; j < q->rcount; j++ )
	{
		n = q->nodes[j];

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
	for( j = 0; j < q->rcount; j++ )
	{
		n = q->nodes[j];

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


// write out the data response
int out_bin_data( NSOCK *s, QUERY *q )
{
	uint32_t *ui, sz;
	uint16_t *us;
	uint8_t *uc;
	float *fp;
	time_t t;
	C3PNT *p;
	int j;

	sz = 24 + ( 12 * q->results->count ) + q->path->len + 1;

	out_bin_write_header( s, q, sz );

	ui    = (uint32_t *) ( s->out.buf + 8 );
	*ui++ = (uint32_t) q->results->from;
	*ui++ = (uint32_t) q->results->to;
	*ui++ = (uint32_t) q->last;

	uc    = (uint8_t *) ui;
	*uc++ = (uint8_t) q->rtype;
	*uc++ = 0;   // padding
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) q->path->len;
	ui    = (uint32_t *) us;
	*ui++ = (uint32_t) q->results->count;


	// sync to the period boundary
	t = q->start - ( q->start % q->results->period );
	p = q->results->points;

	for( j = 0; t < q->end; t += q->results->period, p++, j++ )
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
	uc += q->path->len;
	*uc++ = '\0';

	s->out.len = uc - s->out.buf;
	while( s->out.len % 4 )
	{
		*uc++ = '\0';
		s->out.len++;
	}

	net_write_data( s );

	return 0;
}


// write out a bad-query response
int out_bin_invalid( NSOCK *s, QUERY *q )
{
	uint32_t sz;
	uint8_t *uc;
	int l;

	l = strlen( q->error );

	sz = 9 + l;

	out_bin_write_header( s, q, sz );

	uc    = s->out.buf + 8;
	memcpy( uc, q->error, l );
	uc   += l;
	*uc++ = '\0';

	s->out.len = uc - s->out.buf;
	while( s->out.len % 4 )
	{
		*uc++ = '\0';
		s->out.len++;
	}

	return 0;
}


#undef LLFID

