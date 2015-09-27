#include "local.h"

int libcoal_connect_to( COALH *h, struct sockaddr_in to )
{
	int s, so;

	if( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		herr( MAKE_SOCKET, "Could not create a socket -- %s", Err );
		return -1;
	}

	so = 1;
	if( setsockopt( s, SOL_SOCKET, SO_KEEPALIVE, &so, sizeof( int ) ) )
	{
		herr( SET_KEEPALIVE, "Could not set keepalive socket option -- %s", Err );
		close( s );
		return -1;
	}

	if( connect( s, &to, sizeof( struct sockaddr_in ) ) )
	{
		herr( CONNECT, "Could not connect to host %s:%hu -- %s",
			inet_ntoa( to.sin_addr ), ntohs( to.sin_port ),
			Err );
		close( s );
		return -1;
	}

	return s;
}



int libcoal_connect( COALH *h )
{
	if( !h )
		return -1;

	if( h->data->enabled )
	{
		if( ( h->data->sock = libcoal_connect_to( h, h->data->to ) ) < 0 )
			return -1;
	}

	if( h->query->enabled )
	{
		if( ( h->query->sock = libcoal_connect_to( h, h->query->to ) ) < 0 )
		{
			if( h->data->enabled )
			{
				shutdown( h->data->sock, SHUT_RDWR );
				close( h->data->sock );
			}
			return -1;
		}
	}

	return 0;
}


// write data to our remote host
int libcoal_net_flush( COALH *h, COALCONN *c )
{
	int l, b, rv, tries, ret;
	unsigned char *u;
	struct pollfd p;

	if( !h )
		return -1;

	// no connection specified?  Do both
	if( !c )
		return libcoal_net_flush( h, h->data )
			 + libcoal_net_flush( h, h->query );

	// anything to do?
	if( !c->enabled )
		return 0;

	// are we connected?
	if( !c->connected )
	{
		herr( NOT_CONNECTED, "Host %s not connected - cannot write", h->host );
		return -2;
	}

	l        = c->wrptr - c->outbuf;
	u        = c->outbuf;
	p.fd     = c->sock;
	p.events = POLLOUT;
	tries    = 5;
	ret      = 0;

	while( l > 0 )
	{
		if( ( rv = poll( &p, 1, 20 ) ) < 0 )
		{
			herr( POLL_ERROR, "Poll error write to host %s -- %s",
				h->host, Err );
			ret = -3;
			goto Flush_Finished;
		}

		if( !rv )
		{
			if( tries-- > 0 )
				continue;

			ret = -4;
			goto Flush_Finished;
		}

		if( ( b = send( c->sock, u, l, 0 ) ) < 0 )
		{
			herr( POLL_ERROR, "Send error to host %s -- %s",
				h->host, Err );
			ret = -5;
			goto Flush_Finished;
		}

		u += b;
		l -= b;
	}

Flush_Finished:
	// TODO keep unwritten data
	c->wrptr = c->outbuf;

	return ret;
}




int libcoal_net_read( COALH *h, COALCONN *c )
{
	int i;

	if( !( i = recv( c->sock, c->inbuf + c->inlen, INBUF_SZ - c->inlen, MSG_DONTWAIT ) ) )
	{
		// that would be the fin, then
		close( c->sock );
		c->connected = 0;
		return 0;
	}
	else if( i < 0 )
	{
		if( errno != EAGAIN
		 && errno != EWOULDBLOCK )
		{
			herr( RECV_ERROR, "Recv error for host %s -- %s", h->host, Err );
			return -1;
		}
		return 0;
	}

	c->inlen += i;

	return i;
}


// read until timeout
int libcoal_net_read_query_answer( COALH *h, COALCONN *c, double tmout )
{
	uint32_t rlen, qlen;
	double start, curr;

	libcoal_tmdbl( &start );
	curr = start;

	while( c->inlen < 8 )
	{
		libcoal_net_read( h, c );
		usleep( 10000 );

		libcoal_tmdbl( &curr );
		if( ( curr - start ) > tmout ) {
			herr( TIMEOUT, "Timed out waiting for query answer." );
			return -1;
		}
	}

	// get the length
	qlen = *((uint32_t *) (c->inbuf + 4));

	// add the padding
	rlen = qlen + ( ( 4 - ( qlen % 4 ) ) % 4 );

	while( c->inlen < rlen )
	{
		libcoal_net_read( h, c );
		usleep( 2000 );

		libcoal_tmdbl( &curr );
		if( ( curr - start ) > tmout )
		{
			herr( TIMEOUT, "Timed out waiting for complete query answer." );
			return -2;
		}
	}

	return 0;
}



