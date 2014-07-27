#include "local.h"


int __libcoal_read_data_query( COALH *h, COALCONN *c, int wait )
{
	


}


int __libcoal_read_tree_query( COALH *h, COALCONN *c, int wait )
{
	




}



int __libcoal_read_query( COALH *h, COALCONN *c, int wait )
{
	return ( c->qry-tq ) ? __libcoal_read_tree_query( h, c, wait ) :
	                       __libcoal_read_data_query( h, c, wait );
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

		return __libcoal_read_query( h, c, wait );
	}

	// write query to network
	c->qry = q;

	uc    = c->wrptr;
	*uc++ = 0x01;
	*uc++ = ( q->tq ) ? BINF_TYPE_QUERY : BINF_TYPE_TREE;
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) ( q->path->len + 13 );
	tp    = (time_t *) us;
	*tp++ = q->start;
	*tp++ = q->end;
	uc    = (uint8_t *) tp;
	*uc++ = (uint8_t) q->metric;
	memcpy( uc, q->path->str, q->path->len );
	uc   += q->path->len;
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

	// are we waiting for an answer?
	if( ret == 0 && wait )
		return __libcoal_read_query( h, c, wait );

	// or just return the result of the write
	return ret;
}


void __libcoal_query_clean( COALQRY *q )
{
	int i;

	// tidy up existing
	if( q->path )
		free( q->path );

	if( q->tree )
	{
		if( q->tree->lengths )
			free( q->tree->lengths );
		if( q->tree->children )
		{
			for( i = 0; i < q->tree->count; i++ )
				free( q->tree->children[i] );
			free( q->tree->children );
		}
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

	q->tq     = 0;
	q->start  = 0;
	q->end    = 0;
	q->metric = 0;
	q->answer = 0;
}



int libcoal_tree_query( COALH *h, COALQRY **qp, char *path, int len )
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

	q->len  = ( len ) ? len : strlen( path );
	q->path = (char *) allocz( q->len + 1 );
	memcpy( q->path, path, q->len );

	q->tq   = 1;

	return 0;
}


int libcoal_data_query( COALH *h, COALQRY **qp, char *path, int len, time_t start, time_t end, int metric )
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

	return 0;
}

