#include "local.h"


void *allocz( int size )
{
	return memset( malloc( size ), 0, size );
}


COALH *libcoal_handle( char *host, int doData, int dport, int doQuery, int qport )
{
	struct hostent *he;
	COALH *h;

	h        = (COALH *) allocz( sizeof( COALH ) );
	h->dport = ( dport ) ? (unsigned short) dport : COAL_BIN_DATA_PORT;
	h->qport = ( qport ) ? (unsigned short) qport : COAL_BIN_QUERY_PORT;

	if( !( he = gethostbyname( host ) )
	 || !inet_aton( he->h_addr, &(h->svr) ) )
    {
		herr( BAD_HOST, "Could not look up host '%s'", host );
		return h;
	}

	h->host  = strdup( host );
	h->data  = (COALCONN *) allocz( sizeof( COALCONN ) );
	h->query = (COALCONN *) allocz( sizeof( COALCONN ) );

	h->data->to.sin_addr  = h->svr;
	h->data->to.sin_port  = htons( h->dport );
	if( doData )
	{
		h->data->enabled      = 1;
		h->data->outbuf       = (unsigned char *) allocz( OUTBUF_SZ );
		h->data->hwmk         = h->data->outbuf + ( OUTBUF_SZ - 1072 );
	}

	h->query->to.sin_addr = h->svr;
	h->query->to.sin_port = htons( h->qport );
	if( doQuery )
	{
	  	h->query->enabled     = 1;
		h->query->outbuf      = (unsigned char *) allocz( OUTBUF_SZ );
		h->query->hwmk        = h->query->outbuf + ( OUTBUF_SZ - 512 );
		h->query->inbuf       = (unsigned char *) allocz( INBUF_SZ );
	}

	return h;
}




