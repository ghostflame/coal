#include "local.h"


const char *__libcoal_query_type_names[QUERY_TYPE_MAX] = {
	"data",
	"search",
	"tree"
};


int __libcoal_parse_data_query( COALH *h, COALCONN *c, COALQRY *q )
{
	COALDANS *an = q->data;	
	uint32_t *dp;
	COALPT *cp;
	int i;

	// find the start of the data
	dp = (uint32_t *) ( c->inbuf + 8 );

	an->start = (time_t) *dp++;
	an->end   = (time_t) *dp++;

	// step over metric, padding, path length
	dp++;

	an->count  = (int) *dp++;
	an->points = (COALPT *) allocz( an->count * sizeof( COALPT ) );

	for( cp = an->points, i = 0; i < an->count; i++, cp++ )
	{
		cp->ts    = (time_t) *dp++;
		cp->valid = ( *dp++ & 0x1 ) ? 1 : 0;
		cp->val   = *((float *) dp++);
	}

	return 0;
}


int __libcoal_parse_tree_query( COALH *h, COALCONN *c, COALQRY *q )
{
	COALTANS *an = q->tree;
	COALTREE *t, *prev;
	uint8_t *p;
	int i;

	// find the start of the data
	p = c->inbuf + 10;

	an->count = (int) *((uint16_t *) p);
	p        += 2;
	prev      = NULL;

	// run across, picking up the lengths
	for( i = 0; i < an->count; i++ )
	{
		t = (COALTREE *) allocz( sizeof( COALTREE ) );

		t->is_leaf = *((uint8_t *) p);
		p         += 2;
		t->len     = (unsigned short) *((uint16_t *) p);
		p         += 2;
		t->path    = allocz( t->len + 1 );

		if( prev )
			prev->next = t;
		else
		  	an->children = t;

		prev = t;
	}

	// and then the strings
	for( t = an->children; t; t = t->next )
	{
		memcpy( t->path, p, t->len );
		// the strings come with nulls on
		p += t->len + 1;
	}

	return 0;
}



int __libcoal_read_query( COALH *h, COALCONN *c, int wait )
{
	uint8_t type;
	uint32_t sz;
	int ret;

	c->qry->state = COAL_QUERY_WAITING;

	while( 1 )
	{
		ret = libcoal_net_read( h, c );
		if( ret < 0 )
			return ret;

		if( c->inlen > 8 && c->qry->state == COAL_QUERY_WAITING )
		{
			type = *((uint8_t *)  (c->inbuf + 1));
			sz   = *((uint32_t *) (c->inbuf + 4));

			// add the alignment padding to the end
			sz += ( 4 - ( sz % 4 ) ) % 4;

			c->qry->size  = (int) sz;
			c->qry->state = COAL_QUERY_READ;
		}

		// do we have the whole answer yet?
		if( c->inlen >= c->qry->size )
			break;

		// if we're not waiting, just return success
		if( !wait )
			return 0;

		// otherwise... patience, grasshopper
		usleep( COAL_QUERY_SLEEP_USEC );
	}

	return ( 1 ) ? __libcoal_parse_tree_query( h, c, c->qry ) :
	               __libcoal_parse_data_query( h, c, c->qry );
}



int libcoal_query( COALH *h, COALQRY *q, int wait )
{
	uint16_t *us;
	COALCONN *c;
	uint8_t *uc;
	time_t *tp;
	int ret;

	c = h->query;

	if( !c->connected )
	{
		herr( NOT_CONNECTED, "Not connected." );
		return -1;
	}

	// are we already waiting for a query on this socket?
	if( c->waiting )
	{
		if( q != c->qry ) {
			herr( EXISTING_QRY, "Another query in progress." );
			return -1;
		}

		// are we done yet?
		return __libcoal_read_query( h, c, wait );
	}

	if( q->state != COAL_QUERY_PREPD )
	{
		herr( NO_QRY, "No prepared query ready to send." );
		return -1;
	}

	// write query to network
	c->qry = q;

	uc    = c->wrptr;
	*uc++ = 0x01;
	//*uc++ = ( q->tq ) ? BINF_TYPE_QUERY : BINF_TYPE_TREE;
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) ( q->len + 13 );
	tp    = (time_t *) us;
	*tp++ = q->start;
	*tp++ = q->end;
	uc    = (uint8_t *) tp;
	*uc++ = (uint8_t) q->metric;
	memcpy( uc, q->path, q->len );
	uc   += q->len;
	*uc++ = 0x00;

	c->wrptr = uc;
	c->outlen = c->wrptr - c->outbuf;

	while( c->outlen % 4 )
	{
		c->outlen++;
		*(c->wrptr++) = 0x00;
	}

	// flatten the inbuf
	c->rdptr   = c->inbuf;
	c->keep    = NULL;
	c->inlen   = 0;
	c->keepLen = 0;

	// and mark us as waiting for an answer
	c->waiting = 1;

	// and write the query
	ret = libcoal_net_flush( h, c );

	// how did that go?
	if( ret == 0 )
	{
		q->state = COAL_QUERY_SENT;

		// are we waiting for an answer?
		if( wait )
			return __libcoal_read_query( h, c, wait );
	}

	// or just return the result of the write
	return ret;
}


void libcoal_clean_tree( COALTREE **top )
{
	COALTREE *cp, *cpn;

	if( (*top)->path )
		free( (*top)->path );

	for( cp = (*top)->children; cp; cp = cpn )
	{
		cpn = cp->next;
		libcoal_clean_tree( &cp );
	}

	free( *top );
	*top = NULL;
}


void __libcoal_query_clean( COALQRY *q )
{
	// tidy up existing
	if( q->path )
		free( q->path );

	if( q->tree )
	{
		if( q->tree->children )
			libcoal_clean_tree( &(q->tree->children) );
		memset( q->tree, 0, sizeof( COALTANS ) );
	}
	else
	{
		q->tree = (COALTANS *) allocz( sizeof( COALTANS ) );
	}

	if( q->data )
	{
		if( q->data->points )
			free( q->data->points );
		memset( q->data, 0, sizeof( COALDANS ) );
	}
	else
	{
		q->data = (COALDANS *) allocz( sizeof( COALDANS ) );
	}

	q->start  = 0;
	q->end    = 0;
	q->metric = 0;
	q->size   = 0;
	q->state  = COAL_QUERY_EMPTY;
}



int libcoal_prepare_tree_query( COALH *h, COALQRY **qp, char *path, int len )
{
	COALQRY *q;

	if( !h )
		return -1;

	if( !qp )
	{
		herr( NO_QRY, "No query object pointer supplied." );
		return -1;
	}

	if( *qp )
	{
		q = *qp;
		__libcoal_query_clean( q );
	}
	else
	{
		q   = (COALQRY *) allocz( sizeof( COALQRY ) );
		*qp = q;
	}

	q->len   = ( len ) ? len : strlen( path );
	q->path  = (char *) allocz( q->len + 1 );
	memcpy( q->path, path, q->len );
	q->state = COAL_QUERY_PREPD;

	return 0;
}


int libcoal_prepare_data_query( COALH *h, COALQRY **qp, char *path, int len, time_t start, time_t end, int metric )
{
	COALQRY *q;
	time_t now;

	if( !h )
		return -1;

	if( !qp )
	{
		herr( NO_QRY, "No query object pointer supplied." );
		return -1;
	}

	if( *qp )
	{
		q = *qp;
		__libcoal_query_clean( q );
	}
	else
	{
		q   = (COALQRY *) allocz( sizeof( COALQRY ) );
		*qp = q;
	}

	q->len  = ( len ) ? len : strlen( path );
	q->path = (char *) allocz( q->len + 1 );
	memcpy( q->path, path, q->len );

	time( &now );

	q->start  = ( start ) ? start : now - 86400;
	q->end    = ( end )   ? end   : now;
	q->metric = ( metric > C3DB_REQ_INVLD && metric < C3DB_REQ_END ) ? metric : C3DB_REQ_MEAN;
	q->state  = COAL_QUERY_PREPD;

	return 0;
}

