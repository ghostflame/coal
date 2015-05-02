#include "coal.h"

#define LLFID LLFNE

/*
 * Watched sockets
 *
 * This works by thread_throw_network creating a watcher
 * thread.  As there are various lock-up issues with
 * networking, we have a watcher thread that keeps a check
 * on the networking thread.  It watches for the thread to
 * still exist and the network start time to match what it
 * captured early on.
 *
 * If it sees the time since last activity get too high, we
 * assume that the connection has died.  So we kill the
 * thread and close the connection.
 */

void *net_watched_socket( void *arg )
{
	double stime;
	//pthread_t nt;
	THRD *t;
	HOST *h;

	t = (THRD *) arg;
	h = (HOST *) t->arg;

	// capture the start time - it's sort of a thread id
	stime = h->started;
	debug( 0x0101, "Connection from %s starts at %.6f", h->net->name, stime );

	// capture the thread ID of the watched thread
	// when throwing the handler function
	//nt = thread_throw( t->fp, t->arg );
	thread_throw( t->fp, t->arg );

	while( ctl->run_flags & RUN_LOOP )
	{
		// safe because we never destroy host structures
		if( h->started != stime )
		{
            debug( 0x0102, "Socket has been freed or re-used." );
			break;
		}

		// just use our maintained clock
		if( ( ctl->curr_time - h->last ) > ctl->net->dead_time )
		{
			// cancel that thread
			//pthread_cancel( nt );
			notice( 0x0103, "Connection from host %s timed out.", h->net->name );
            h->net->flags |= HOST_CLOSE;
			// net_close_host( h );
			break;
		}

		// we are not busy threads around these parts
		usleep( 200000 );
	}

	debug( 0x0104, "Watcher of thread %lu, exiting." );
	free( t );
	return NULL;
}


void net_disconnect( int *sock, char *name )
{
	if( shutdown( *sock, SHUT_RDWR ) )
		err( 0x0201, "Shutdown error on connection with %s -- %s",
			name, Err );

	close( *sock );
	*sock = -1;
}


void net_close_host( HOST *h )
{
	net_disconnect( &(h->net->sock), h->net->name );
	debug( 0x0301, "Closed connection from host %s.", h->net->name );

	mem_free_host( &h );
}


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
		err( 0x0401, "Accept error -- %s", Err );
		return NULL;
	}

	snprintf( buf, 32, "%s:%hu", inet_ntoa( from.sin_addr ),
		ntohs( from.sin_port ) );

	h            = mem_new_host( );
	h->type      = type;
	h->peer      = from;
	h->net->name = strdup( buf );
	h->net->sock = d;
	// should be a unique timestamp
	h->started   = timedbl( NULL );
	h->last      = h->started;

	switch( type )
	{
		case NET_COMM_LINE:
			h->qrf = &query_line_read;
			h->drf = &data_line_read;
			break;
		case NET_COMM_BIN:
			h->qrf = &query_bin_read;
			h->drf = &data_bin_read;
			break;
	}

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
		warn( 0x0501, "Net connect called on connected socket - disconnecting." );
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
		err( 0x0502, "Unable to make tcp socket for %s -- %s",
			label, Err );
		return -1;
	}

	if( setsockopt( s->sock, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof( int ) ) )
	{
		err( 0x0503, "Unable to set keepalive on socket for %s -- %s",
			label, Err );
		close( s->sock );
		s->sock = -1;
		return -1;
	}

	if( connect( s->sock, s->peer, sizeof( struct sockaddr_in ) ) < 0 )
	{
		err( 0x0504, "Unable to connect to %s:%hu for %s -- %s",
			inet_ntoa( s->peer->sin_addr ), ntohs( s->peer->sin_port ),
			label, Err );
		close( s->sock );
		s->sock = -1;
		return -1;
	}

	info( 0x0506, "Connected (%d) to remote host %s:%hu for %s.",
		s->sock, inet_ntoa( s->peer->sin_addr ),
		ntohs( s->peer->sin_port ), label );

	return s->sock;
}



int net_listen_sock( PORT_CTL *pc, int backlog )
{
	if( !backlog )
		backlog = 5;

	if( listen( pc->sock, backlog ) < 0 )
	{
		err( 0x0e01, "Listen error for %s -- %s", pc->label, Err );
		close( pc->sock );
		return -1;
	}

	info( 0x0e02, "Listening on port %hu for %s", pc->port, pc->label );
	return 0;
}



int net_port_sock( PORT_CTL *pc, uint32_t ip )
{
	struct sockaddr_in sa;
	int s, so;

	if( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		err( 0x0601, "Unable to make tcp socket for %s -- %s",
			pc->label, Err );
		return -1;
	}

	so = 1;
	if( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &so, sizeof( int ) ) )
	{
		err( 0x0602, "Set socket options error for %s -- %s",
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
			err( 0x0603, "Bind to %s:%hu failed for %s -- %s",
				inet_ntoa( sa.sin_addr ), pc->port, pc->label, Err );
			close( s );
			return -3;
		}
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
			warn( 0x0701, "Poll error writing to host %s -- %s",
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
			warn( 0x0702, "Error writing to host %s -- %s",
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

	//debug( 0x0703, "Wrote to %d bytes to %d/%s", ( ptr - s->out.buf ), s->sock, s->name );

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
			err( 0x0801, "Recv error for host %s -- %s",
				s->name, Err );
			s->flags |= HOST_CLOSE;
			return 0;
		}
		return i;
	}

	// got some data then
	s->in.len += i;

	//debug( 0x0802, "Received %d bytes on socket %d/%s", i, s->sock, s->name );

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

	// mark the socket as active
	h->last = ctl->curr_time;

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
			warn( 0x0901, "Invalid version (%d) from host %s.",
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

	// mark the socket as active
	h->last = ctl->curr_time;

	if( n->in.buf[n->in.len - 1] == LINE_SEPARATOR )
		// remove any trailing separator
		n->in.buf[--(n->in.len)] = '\0';
	else
	 	// make a note to keep the last line back
		keeplast = 1;

	if( strwords( h->all, (char *) n->in.buf, n->in.len, LINE_SEPARATOR ) < 0 )
	{
		debug( 0x0a01, "Invalid buffer from data host %s.", n->name );
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


int net_listen_type( NET_TYPE_CTL *ntc )
{
	int d, q;

	if( !ntc->enabled )
		return 0;

	if( ntc->data->sock )
		d = net_listen_sock( ntc->data, 10 );

	if( ntc->query->sock )
		q = net_listen_sock( ntc->query, 10 );

	return (d|q);
}


int net_start_type( NET_TYPE_CTL *ntc )
{
	int d, q;

	if( !ntc->enabled )
		return 0;

	if( ( d = net_port_sock( ntc->data, 0 ) ) < 0 )
		return -1;

	if( ( q = net_port_sock( ntc->query, 0 ) ) < 0 )
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
	int ret = 0;

	notice( 0x0f01, "Starting network listening." );

	ret += net_listen_type( ctl->net->line );
	ret += net_listen_type( ctl->net->bin );

	return ret;
}


int net_bind( void )
{
	int ret = 0, en = 0;

	en += ctl->net->line->enabled;
	en += ctl->net->bin->enabled;

	if( !en )
	{
		err( 0x0b01, "No networking enabled." );
		return -1;
	}

	notice( 0x0b02, "Starting networking." );
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
	notice( 0x0c01, "Stopping networking." );
	net_stop_type( ctl->net->line );
	net_stop_type( ctl->net->bin );
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

	net->dead_time			= NET_DEAD_CONN_TIMER;

	return net;
}


int net_config_line( AVP *av )
{
  	NET_TYPE_CTL *ntc = NULL;
	PORT_CTL *pc = NULL;
	char *d, *p;

	// only a few are single words
	if( !( d = strchr( av->att, '.' ) ) )
	{
		if( attIs( "timeout" ) )
		{
			ctl->net->dead_time = strtod( av->val, NULL );
			ndebug( 0x0d01, "Dead connection timeout set to %.2f sec.", ctl->net->dead_time );
			return 0;
		}

		return -1;
	}

	// then it's by line. or data.
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

#undef LLFID

