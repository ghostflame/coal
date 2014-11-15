#include "local.h"



int libcoal_data_add( COALH *h, time_t ts, float val, char *path, int len )
{
	COALCONN *c;
	COALDAT d;
	int sz;

	if( !h )
		return -1;

	c = h->data;

	if( !c->enabled )
	{
		herr( NOT_ENABLED, "Data sending not enabled." );
		return -1;
	}

	if( !c->connected )
	{
		herr( NOT_CONNECTED, "Not connected to remote coal server." );
		return -1;
	}

	if( !len )
		len = strlen( path );

	sz = sizeof( COALDAT ) + len + 1;

	// will we have space?
	if( ( c->wrptr + sz ) > c->hwmk )
		libcoal_net_flush( h, c );

	d.vers = 1;
	d.type = BINF_TYPE_DATA;
	d.size = sz;
	d.ts   = ts;
	d.val  = val;

	memcpy( c->wrptr, &d, sizeof( COALDAT ) );
	c->wrptr += sizeof( COALDAT );

	memcpy( c->wrptr, path, len );
	c->wrptr += len;
	*(c->wrptr++) = '\0';

	// and pad it for alignment
	while( sz % 4 )
	{
		*(c->wrptr++) = '\0';
		sz++;
	}

	return 0;
}


int libcoal_data_send( COALH *h, time_t ts, float val, char *path, int len ) {

	int ret = 0;

  	ret += libcoal_data_send( h, ts, val, path, len );
	ret += libcoal_net_flush( h, h->data );

	return ret;
}


