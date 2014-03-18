#include "coal.h"

HOST *net_get_host( int sock, int type )
{
	struct sockaddr_in from;
	socklen_t sz;
	char buf[32];
	HOST *h;
	int d;

	sz = sizeof( from );
	if( ( d = accept( sock, (struct sockaddr *) &from, &sz ) ) < 0 )
	{
		// broken
		err( "Accept error -- %s", Err );
		return NULL;
	}

	snprintf( buf, 32, "%s:%hu", inet_ntoa( from.sin_addr ),
		ntohs( from.sin_port ) );

	h		= mem_new_host( );
	h->fd   = d;
	h->type = type;
	h->peer = from;
	h->name = strdup( buf );

	return h;
}


int net_port_sock( PORT_CTL *pc, uint32_t ip, int backlog )
{
	struct sockaddr_in sa;
	int s, so;

	if( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		err( "Unable to make tcp socket for %s -- %s",
			pc->label, Err );
		return -1;
	}

	so = 1;
	if( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &so, sizeof( int ) ) )
	{
		err( "Set socket options error for %s -- %s",
			pc->label, Err );
		close( s );
		return -2;
	}

	if( pc->port > 0 )
	{
		memset( &sa, 0, sizeof( struct sockaddr_in ) );
		sa.sin_family = AF_INET;
		sa.sin_port   = htons( pc->port );

		// ip as well?
		sa.sin_addr.s_addr = ( ip ) ? ip : INADDR_ANY;

		// try to bind
		if( bind( s, (struct sockaddr *) &sa, sizeof( struct sockaddr_in ) ) < 0 )
		{
			err( "Bind to %s:%hu failed for %s -- %s",
				inet_ntoa( sa.sin_addr ), pc->port, pc->label, Err );
			close( s );
			return -3;
		}

		if( !backlog )
			backlog = 5;

		if( listen( s, backlog ) < 0 )
		{
			err( "Listen error for %s -- %s", pc->label, Err );
			close( s );
			return -4;
		}

		info( "Listening on port %hu for %s", pc->port, pc->label );
	}

	return s;
}




int net_write_data( HOST *h )
{
	unsigned char *ptr;
	struct pollfd p;
	int rv, b;

	p.fd     = h->fd;
	p.events = POLLOUT;

	ptr = h->outbuf;

	while( h->outlen > 0 )
	{
		if( ( rv = poll( &p, 1, 20 ) ) < 0 )
		{
			warn( "Poll error writing to host %s -- %s",
				h->name, Err );
			h->flags |= HOST_CLOSE;
			return -1;
		}

		if( !rv )
			// we cannot write just yet
			return 0;

		if( ( b = send( h->fd, ptr, h->outlen, 0 ) ) < 0 )
		{
			warn( "Error writing to host %s -- %s",
				h->name, Err );
			h->flags |= HOST_CLOSE;
			return -2;
		}

		h->outlen -= b;
		ptr       += b;
	}

	// weirdness
	if( h->outlen < 0 )
		h->outlen = 0;

	// what we wrote
	return ptr - h->outbuf;
}



int net_read_data( HOST *h )
{
	int i;

	if( h->keepLen )
	{
	  	// can we shift the kept string
		if( ( h->keep - h->inbuf ) >= h->keepLen )
			memcpy( h->inbuf, h->keep, h->keepLen );
		else
		{
			// no, we can't
			unsigned char *p, *q;

			i = h->keepLen;
			p = h->inbuf;
			q = h->keep;

			while( i-- > 0 )
				*p++ = *q++;
		}

		h->inlen   = h->keepLen;
		h->keep    = NULL;
		h->keepLen = 0;
	}
	else
		h->inlen = 0;

	if( !( i = recv( h->fd, h->inbuf + h->inlen, MAX_PKT_IN, MSG_DONTWAIT ) ) )
	{
	  	// that would be the fin, then
		h->flags |= HOST_CLOSE;
		return 0;
	}
	else if( i < 0 )
	{
		if( errno != EAGAIN
		 && errno != EWOULDBLOCK )
		{
			err( "Recv error for host %s -- %s",
				h->name, Err );
			h->flags |= HOST_CLOSE;
			return 0;
		}
		return i;
	}

	// got some data then
	h->inlen += i;

	return i;
}


// we quite badly abuse the strwords object to point
// to our data structures
int net_read_bin( HOST *h )
{
	unsigned char *start;
	int i, l, ver, sz;

	// try to read some more data
	if( ( i = net_read_data( h ) ) <= 0 )
		return i;

	// anything to handle?
	if( !h->inlen )
		return 0;

	// we'll chew through this data lump by lump
	l     = h->inlen;
	start = h->inbuf;
	memset( h->all, 0, sizeof( WORDS ) );

	// break the buffer up into our binary chunks
	while( l > 0 )
	{
		// do we have a full start structure?
		if( l < 4 )
		{
			h->keep    = start;
			h->keepLen = l;

			return h->all->wc;
		}

		// check version
		ver = (int) *start;

		// we only know version 1 so far
		if( ver != 1 )
		{
			warn( "Invalid version (%d) from host %s.",
				ver, h->name );
			h->flags |= HOST_CLOSE;
			return h->all->wc;
		}

		// read the record size
		sz = (int) *((uint16_t *) ( start + 2 ));

		// do we have a full record?
		if( l < sz )
		{
			// no
			h->keep    = start;
			h->keepLen = l;

			return h->all->wc;
		}

		// we don't interpret the chunks here
		// we just keep them
		h->all->wd[h->all->wc]  = (char *) start;
		h->all->len[h->all->wc] = sz;
		h->all->wc++;

		// and move one
		start += sz;
		l     -= sz;
	}

	return h->all->wc;
}



int net_read_lines( HOST *h )
{
	int i, keeplast = 0, l;
	char *w;

	// try to read some data
	if( ( i = net_read_data( h ) ) <= 0 )
		return i;

	// do we have anything at all?
	if( !h->inlen )
		return 0;

	if( h->inbuf[h->inlen - 1] == LINE_SEPARATOR )
		// remove any trailing separator
		h->inbuf[--(h->inlen)] = '\0';
	else
	 	// make a note to keep the last line back
		keeplast = 1;

	if( strwords( h->all, (char *) h->inbuf, h->inlen, LINE_SEPARATOR ) < 0 )
	{
		debug( "Invalid buffer from data host %s.", h->name );
		return -1;
	}

	// clean \r's
	for( i = 0; i < h->all->wc; i++ )
	{
		w = h->all->wd[i];
		l = h->all->len[i];

		// might be at the start
		if( *w == '\r' )
		{
			++(h->all->wd[i]);
			--(h->all->len[i]);
			--l;
		}

		// remove trailing carriage returns
		if( *(w + l - 1) == '\r' )
			h->all->wd[--(h->all->len[i])] = '\0';
	}



	// claw back the last line - it was incomplete
	if( h->all->wc && keeplast )
	{
		// if we have several lines we can't move the last line
		if( --(h->all->wc) )
		{
			// move it next time
			h->keep    = (unsigned char *) h->all->wd[h->all->wc];
			h->keepLen = h->all->len[h->all->wc];
		}
		else
		{
			// it's the only line
			h->inlen   = h->all->len[0];
			h->keep    = NULL;
			h->keepLen = 0;
		}
	}

	return h->all->wc;
}


int net_start_type( NET_TYPE_CTL *ntc )
{
	int d, q;

	if( !ntc->enabled )
		return 0;

	if( ( d = net_port_sock( ntc->data, 0, 10 ) ) < 0 )
		return -1;

	if( ( q = net_port_sock( ntc->query, 0, 10 ) ) < 0 )
	{
		close( d );
		return -1;
	}

	ntc->data->sock  = d;
	ntc->query->sock = q;

	return 0;
}



int net_start( void )
{
	int ret = 0, en = 0;

	en += ctl->net->line->enabled;
	en += ctl->net->bin->enabled;

	if( !en )
	{
		err( "No networking enabled." );
		return -1;
	}

	notice( "Starting networking." );
	ret += net_start_type( ctl->net->line );
	ret += net_start_type( ctl->net->bin );

	return ret;
}


void net_stop_type( NET_TYPE_CTL *ntc )
{
	if( !ntc->enabled )
		return;

	if( ntc->data->sock > -1 )
	{
		close( ntc->data->sock );
		ntc->data->sock = -1;
	}

	if( ntc->query->sock > -1 )
	{
		close( ntc->query->sock );
		ntc->query->sock = -1;
	}
}


void net_stop( void )
{
	notice( "Stopping networking." );
	net_stop_type( ctl->net->line );
	net_stop_type( ctl->net->bin );
	return;
}



NET_CTL *net_config_defaults( void )
{
	NET_CTL *net;

	net                     = (NET_CTL *) allocz( sizeof( NET_CTL ) );

	net->line                = (NET_TYPE_CTL *) allocz( sizeof( NET_TYPE_CTL ) );
	net->line->data          = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->line->data->label   = strdup( "line data" );
	net->line->data->port    = DEFAULT_NET_LINE_DATA_PORT;
	net->line->data->sock    = -1;
	net->line->query         = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->line->query->label  = strdup( "line queries" );
	net->line->query->port   = DEFAULT_NET_LINE_QUERY_PORT;
	net->line->query->sock   = -1;
	net->line->type          = NET_COMM_LINE;
	net->line->enabled       = DEFAULT_NET_LINE_ENABLED;

	net->bin                 = (NET_TYPE_CTL *) allocz( sizeof( NET_TYPE_CTL ) );
	net->bin->data           = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->bin->data->label    = strdup( "binary data" );
	net->bin->data->port     = DEFAULT_NET_BIN_DATA_PORT;
	net->bin->data->sock     = -1;
	net->bin->query          = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->bin->query->label   = strdup( "binary queries" );
	net->bin->query->port    = DEFAULT_NET_BIN_QUERY_PORT;
	net->bin->query->sock    = -1;
	net->bin->type           = NET_COMM_BIN;
	net->bin->enabled        = DEFAULT_NET_BIN_ENABLED;

	return net;
}


int net_config_line( AVP *av )
{
  	NET_TYPE_CTL *ntc = NULL;
	PORT_CTL *pc = NULL;
	char *d, *p;

	if( !( d = strchr( av->att, '.' ) ) )
		return -1;
	p = d + 1;

	// they have top begin with a type name
	if( !strncasecmp( av->att, "line.", 5 ) )
		ntc = ctl->net->line;
	else if( !strncasecmp( av->att, "bin.", 4 ) )
		ntc = ctl->net->line;
	else
		return -1;

	if( !( d = strchr( p, '.' ) ) )
	{
		// check for net-type based things
		if( !strcasecmp( p, "enabled" ) )
			ntc->enabled = atoi( av->val ) & 0x1;
		else
			return -1;

		return 0;
	}

	d++;

	// now we need a port name
	if( !strncasecmp( p, "data.", 5 ) )
		pc = ntc->data;
	else if( !strncasecmp( p, "query.", 6 ) )
		pc = ntc->query;
	else
		return -1;

	// and here for port based things
	if( !strcasecmp( d, "port" ) )
		pc->port = (unsigned short) strtoul( av->val, NULL, 10 );
	else if( !strcasecmp( d, "label" ) )
	{
		free( pc->label );
		pc->label = strdup( av->val );
	}
	else
		return -1;

	return 0;
}


