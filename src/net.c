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

	h            = mem_new_host( );
	h->type      = type;
	h->peer      = from;
	h->net->name = strdup( buf );
	h->net->sock = d;

	return h;
}


NSOCK *net_make_sock( int insz, int outsz, char *name, struct sockaddr_in *peer )
{
	NSOCK *ns;

	ns = (NSOCK *) allocz( sizeof( NSOCK ) );
	if( name )
		ns->name = strdup( name );
	ns->peer = peer;

	if( insz )
	{
		ns->in.sz    = insz;
		ns->in.buf   = (unsigned char *) allocz( insz );
		// you can fiddle with these if you like
		ns->in.hwmk  = ns->in.buf + ( ( 5 * insz ) / 6 );
	}

	if( outsz )
	{
		ns->out.sz   = outsz;
		ns->out.buf  = (unsigned char *) allocz( outsz );
		// you can fiddle with these if you like
		ns->out.hwmk = ns->out.buf + ( ( 5 * outsz ) / 6 );
	}

	// no socket yet
	ns->sock = -1;

	return ns;
}


int net_connect( NSOCK *s )
{
	int opt = 1;
	char *label;

	if( s->sock != -1 )
	{
		warn( "Net connect called on connected socket - disconnecting." );
		shutdown( s->sock, SHUT_RDWR );
		close( s->sock );
		s->sock = -1;
	}

	if( !s->name || !*(s->name) )
		label = "unknown socket";
	else
		label = s->name;

	if( ( s->sock = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		err( "Unable to make tcp socket for %s -- %s",
			label, Err );
		return -1;
	}

	if( setsockopt( s->sock, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof( int ) ) )
	{
		err( "Unable to set keepalive on socket for %s -- %s",
			label, Err );
		close( s->sock );
		s->sock = -1;
		return -1;
	}

	if( connect( s->sock, s->peer, sizeof( struct sockaddr_in ) ) < 0 )
	{
		err( "Unable to connect to %s:%hu for %s -- %s",
			inet_ntoa( s->peer->sin_addr ), ntohs( s->peer->sin_port ),
			label, Err );
		close( s->sock );
		s->sock = -1;
		return -1;
	}

	info( "Connected (%d) to remote host %s:%hu for %s.",
		s->sock, inet_ntoa( s->peer->sin_addr ),
		ntohs( s->peer->sin_port ), label );

	return s->sock;
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


int net_write_data( NSOCK *s )
{
	unsigned char *ptr;
	int rv, b, tries;
	struct pollfd p;

	p.fd     = s->sock;
	p.events = POLLOUT;
	tries    = 3;
	ptr      = s->out.buf;

	while( s->out.len > 0 )
	{
		if( ( rv = poll( &p, 1, 20 ) ) < 0 )
		{
			warn( "Poll error writing to host %s -- %s",
				s->name, Err );
			s->flags |= HOST_CLOSE;
			return -1;
		}

		if( !rv )
		{
			// wait another 20msec and try again
			if( tries-- > 0 )
				continue;

			// we cannot write just yet
			return 0;
		}

		if( ( b = send( s->sock, ptr, s->out.len, 0 ) ) < 0 )
		{
			warn( "Error writing to host %s -- %s",
				s->name, Err );
			s->flags |= HOST_CLOSE;
			return -2;
		}

		s->out.len -= b;
		ptr        += b;
	}

	// weirdness
	if( s->out.len < 0 )
		s->out.len = 0;

	//debug( "Wrote to %d bytes to %d/%s", ( ptr - s->out.buf ), s->sock, s->name );

	// what we wrote
	return ptr - s->out.buf;
}



int net_read_data( NSOCK *s )
{
	int i;

	if( s->keep.len )
	{
	  	// can we shift the kept string
		if( ( s->keep.buf - s->in.buf ) >= s->keep.len )
			memcpy( s->in.buf, s->keep.buf, s->keep.len );
		else
		{
			// no, we can't
			unsigned char *p, *q;

			i = s->keep.len;
			p = s->in.buf;
			q = s->keep.buf;

			while( i-- > 0 )
				*p++ = *q++;
		}

		s->keep.buf = NULL;
		s->in.len   = s->keep.len;
		s->keep.len = 0;
	}
	else
		s->in.len = 0;

	if( !( i = recv( s->sock, s->in.buf + s->in.len, s->in.sz - ( s->in.len + 2 ), MSG_DONTWAIT ) ) )
	{
	  	// that would be the fin, then
		s->flags |= HOST_CLOSE;
		return 0;
	}
	else if( i < 0 )
	{
		if( errno != EAGAIN
		 && errno != EWOULDBLOCK )
		{
			err( "Recv error for host %s -- %s",
				s->name, Err );
			s->flags |= HOST_CLOSE;
			return 0;
		}
		return i;
	}

	// got some data then
	s->in.len += i;

	//debug( "Received %d bytes on socket %d/%s", i, s->sock, s->name );

	return i;
}


// we quite badly abuse the strwords object to point
// to our data structures
int net_read_bin( HOST *h )
{
	int i, l, ver, sz, rdsz;
	unsigned char *start;
	NSOCK *n = h->net;

	// try to read some more data
	if( ( i = net_read_data( n ) ) <= 0 )
		return i;

	// anything to handle?
	if( !n->in.len )
		return 0;

	// we'll chew through this data lump by lump
	l     = n->in.len;
	start = n->in.buf;
	memset( h->all, 0, sizeof( WORDS ) );

	// break the buffer up into our binary chunks
	while( l > 0 )
	{
		// do we have a full start structure?
		if( l < 4 )
		{
			n->keep.buf = start;
			n->keep.len = l;

			return h->all->wc;
		}

		// check version
		ver = (int) *start;

		// we only know version 1 so far
		if( ver != 1 )
		{
			warn( "Invalid version (%d) from host %s.",
				ver, n->name );
			n->flags |= HOST_CLOSE;
			return h->all->wc;
		}

		// read the record size
		sz   = (int) *((uint16_t *) ( start + 2 ));
		// but we have the read up to an alignment boundary
		rdsz = ( sz % 4 ) ? sz + 4 - ( sz % 4 ) : sz;

		// do we have a full record?
		if( l < rdsz )
		{
			// no
			n->keep.buf = start;
			n->keep.len = l;

			return h->all->wc;
		}

		// we don't interpret the chunks here
		// we just keep them
		h->all->wd[h->all->wc]  = (char *) start;
		h->all->len[h->all->wc] = sz;
		h->all->wc++;

		// and move one
		start += rdsz;
		l     -= rdsz;
	}

	return h->all->wc;
}



int net_read_lines( HOST *h )
{
	int i, keeplast = 0, l;
	NSOCK *n = h->net;
	char *w;

	// try to read some data
	if( ( i = net_read_data( n ) ) <= 0 )
		return i;

	// do we have anything at all?
	if( !n->in.len )
		return 0;

	if( n->in.buf[n->in.len - 1] == LINE_SEPARATOR )
		// remove any trailing separator
		n->in.buf[--(n->in.len)] = '\0';
	else
	 	// make a note to keep the last line back
		keeplast = 1;

	if( strwords( h->all, (char *) n->in.buf, n->in.len, LINE_SEPARATOR ) < 0 )
	{
		debug( "Invalid buffer from data host %s.", n->name );
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
			n->keep.buf = (unsigned char *) h->all->wd[h->all->wc];
			n->keep.len = h->all->len[h->all->wc];
		}
		else
		{
			// it's the only line
			n->in.len   = h->all->len[0];
			n->keep.buf = NULL;
			n->keep.len = 0;
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

	net->line               = (NET_TYPE_CTL *) allocz( sizeof( NET_TYPE_CTL ) );
	net->line->data         = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->line->data->label  = strdup( "line data" );
	net->line->data->port   = DEFAULT_NET_LINE_DATA_PORT;
	net->line->data->sock   = -1;
	net->line->query        = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->line->query->label = strdup( "line queries" );
	net->line->query->port  = DEFAULT_NET_LINE_QUERY_PORT;
	net->line->query->sock  = -1;
	net->line->type         = NET_COMM_LINE;
	net->line->enabled      = DEFAULT_NET_LINE_ENABLED;

	net->bin                = (NET_TYPE_CTL *) allocz( sizeof( NET_TYPE_CTL ) );
	net->bin->data          = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->bin->data->label   = strdup( "binary data" );
	net->bin->data->port    = DEFAULT_NET_BIN_DATA_PORT;
	net->bin->data->sock    = -1;
	net->bin->query         = (PORT_CTL *) allocz( sizeof( PORT_CTL ) );
	net->bin->query->label  = strdup( "binary queries" );
	net->bin->query->port   = DEFAULT_NET_BIN_QUERY_PORT;
	net->bin->query->sock   = -1;
	net->bin->type          = NET_COMM_BIN;
	net->bin->enabled       = DEFAULT_NET_BIN_ENABLED;

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
		ntc = ctl->net->bin;
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


