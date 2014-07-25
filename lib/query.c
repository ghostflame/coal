#include "local.h"

int libcoal_query( COALH *h, COALQRY *q )
{
	COALCONN *c;

	c = h->query;

	if( !c->connected )
	{
		herr( NOT_CONNECTED, "Not connected." );
		return -1;
	}

	// write query to network, wait for answer



	return 0;
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

