#include "coal.h"

#define LLFID LLFRE

void relay_collect_points( RDEST *d )
{
	POINT *list, *p;
	int i;

	for( i = 0; i < d->qcount; i++ )
	{
		list = NULL;

		// grab one queue
		pthread_mutex_lock( &(d->locks[i]) );

		if( d->incoming[i] )
		{
			list           = d->incoming[i];
			d->incoming[i] = NULL;
		}

		pthread_mutex_unlock( &(d->locks[i]) );

		// did we find anything?
		if( !list )
			continue;

		// walk along and find the last element
		for( p = list; p->next; p = p->next );

		// then add that to the outgoing
		p->next     = d->outgoing;
		d->outgoing = list;
	}
}



void relay_write_point_bin( NSOCK *s, POINT *p )
{
	uint16_t *us;
	uint8_t *uc;
	time_t *tp;
	float *fp;

	// write the header
	uc    = s->out.buf + s->out.len;
	*uc++ = 0x01;
	*uc++ = BINF_TYPE_DATA;
	us    = (uint16_t *) uc;
	*us++ = (uint16_t) ( p->path->len + 13 );
	tp    = (time_t *) us;
	*tp++ = p->data.ts;
	fp    = (float *) tp;
	*fp++ = p->data.val;
	uc    = (uint8_t *) fp;
	memcpy( uc, p->path->str, p->path->len );
	uc += p->path->len;
	*uc++ = 0x00;

	s->out.len = uc - s->out.buf;
	while( s->out.len % 4 )
	{
		*uc++ = 0x00;
		s->out.len++;
	}

	if( uc > s->out.hwmk )
		net_write_data( s );
}


void relay_write_point_line( NSOCK *s, POINT *p )
{
	char *op;

	// write the line
	op = (char *) s->out.buf + s->out.len;

	s->out.len += snprintf( op, s->out.sz - s->out.len, "%s %f %ld\n",
				p->path->str, p->data.val, (long) p->data.ts );

	if( ( s->out.buf + s->out.len ) > s->out.hwmk )
		net_write_data( s );
}



void *relay_flush( void *arg )
{
	POINT *p;
	RDEST *d;
	THRD *t;

	t = (THRD *) arg;
	d = (RDEST *) t->arg;
	free( t );

	// the logic here is to get the lock, check there's no outgoing stuff and
	// abort if there is - another thread must be working on that
	pthread_mutex_lock( &(d->pt_ctl) );

	// anything already?
	if( d->outgoing )
	{
		pthread_mutex_unlock( &(d->pt_ctl) );
		return NULL;
	}

	// collect any points
	relay_collect_points( d );

	// anything to do?
	if( !d->outgoing )
	{
		pthread_mutex_unlock( &(d->pt_ctl) );
		return NULL;
	}

	// process the list
	while( d->outgoing )
	{	
		p = d->outgoing;
		d->outgoing = p->next;

		(d->rfun)( d->data, p );
	}

	// flush any remaining data
	if( d->data->out.len )
		net_write_data( d->data );

	// and unlock
	pthread_mutex_unlock( &(d->pt_ctl) );

	return NULL;
}




void *relay_loop( void *arg )
{
	RDEST *d;
	THRD *t;

	t = (THRD *) arg;
	d = (RDEST *) t->arg;
	free( t );

	loop_mark_start( );

	while( ctl->run_flags & RUN_LOOP )
	{
		// flush each relay
		for( d = ctl->relay->dests; d; d = d->next )
			thread_throw( relay_flush, d );

		// and sleep
		usleep( ctl->relay->usdelay );
	}

	loop_mark_done( );

	return NULL;
}


// add a point
void relay_add_point( RDEST *d, POINT *p )
{
	int i;

	// assess which lock/queue
	i = p->path->len % d->qcount;

	pthread_mutex_lock( &(d->locks[i]) );

	p->next        = d->incoming[i];
	d->incoming[i] = p;

	pthread_mutex_unlock( &(d->locks[i]) );
}


void relay_stop( void )
{
	RDEST *d;

	for( d = ctl->relay->dests; d; d = d->next )
	{
		notice( 0x0101, "Shutting down destination '%s'", d->name );
		shutdown( d->data->sock, SHUT_RDWR );
		close( d->data->sock );
		d->data->sock = -1;
	}
}


void relay_pong( RDEST *d, uint32_t *sec, uint16_t *msec )
{
	int bytes = 0, tries = 0;

	while( bytes < 16 && tries < 3 )
	{
		bytes += net_read_data( d->data );
		if( bytes < 16 )
		{
			tries++;
			usleep( 1000 );
		}
	}

    if( tries >= 3 ) {
        return;
    }

	// read the pong
}


void relay_ping( RDEST *d, uint32_t sec, uint16_t msec )
{
	uint8_t *uc;
	uint32_t *ts;
	uint16_t *ms;
	NSOCK *n = d->data;

	pthread_mutex_lock( &(d->pt_ctl) );

	uc = n->out.buf;
	*uc++ = 0x01;
	*uc++ = BINF_TYPE_PING;

	ms = (uint16_t *) uc;
	*ms++ = 16;

	ts = (uint32_t *) ms;
	*ts++ = sec;
	*ts++ = 0;

	ms = (uint16_t *) ts;
	*ms++ = msec;
	*ms++ = 0;

	n->out.len += 16;

	net_write_data( d->data );

	pthread_mutex_unlock( &(d->pt_ctl) );
}




void *relay_connection_watcher( void *arg )
{
	double when, recvd;
	int rv, ping_sent;
	struct pollfd p;
	uint32_t its;
	uint16_t ims;
	RDEST *dst;
	NSOCK *sk;
	THRD *t;

	t   = (THRD *) arg;
	dst = (RDEST *) t->arg;
	sk  = dst->data;
	free( t );

	ping_sent = 0;
	p.fd      = sk->sock;
	p.events  = POLL_EVENTS;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( sk->flags & HOST_CLOSE )
			break;

		if( sk->sock < 0 )
		{
			net_connect( sk );
			usleep( 1000000 );
			continue;
		}

		when = ctl->curr_time;
		its  = (uint32_t) when;
		ims  = (uint16_t) ( 1000.0 * ( when - (double) its ) );

		if( !ping_sent )
		{
			// so sent a ping!
			relay_ping( dst, its, ims );
			ping_sent = 1;
		}

		rv = poll( &p, 1, 5000 );
		if( rv < 0 )
		{
			if( errno != EINTR )
			{
			  	warn( 0x0201, "Error polling relay connection to %s -- %s",
					dst->name, Err );
				net_disconnect( &(sk->sock), dst->name );
				debug( 0x0202, "Closed relay connection to %s.", dst->name );
			}
		}
		else if( !rv )
		{
			warn( 0x0203, "No response from relay connection to %s.", dst->name );
            continue;
		}

		relay_pong( dst, &its, &ims );
		ping_sent = 0;
        recvd  = (double) ims;
        recvd /= 1000.0;
        recvd += (double) its;

		usleep( 2000000 );
	}

	if( shutdown( sk->sock, SHUT_RDWR ) )
		err( 0x0204, "Shutdown error to relay %s -- %s", dst->name, Err );

	close( sk->sock );
	sk->sock = -1;
	debug( 0x0205, "Closed relay connection to %s.", dst->name );

	return NULL;
}



int relay_start( void )
{
	char label[128];
	RDEST *d;
	int i;

	for( d = ctl->relay->dests; d; d = d->next )
	{
		snprintf( label, 128, "data relay connection to %s", d->name );

		// and copy that to the socket
		d->data->name = strdup( label );

		// make space for the queues and their locks
		d->incoming = (POINT **) allocz( d->qcount * sizeof( POINT * ) );
		d->locks    = (pthread_mutex_t *) allocz( d->qcount * sizeof( pthread_mutex_t ) );

		// init the locks
		for( i = 0; i < d->qcount; i++ )
			pthread_mutex_init( &(d->locks[i]), NULL );

		pthread_mutex_init( &(d->pt_ctl), NULL );

		// and connect to the relay destination
		if( net_connect( d->data ) < 0 )
		{
			rerr( 0x0301, "Failed to connect to relay host %s.", d->name );
			return -1;
		}

		rnotice( 0x0302, "Connected to relay host %s.", d->name );
	}

	return 0;
}


// create a blank, default relay struct
RDEST *relay_dest_create( void )
{
	RDEST *d;

	d         = (RDEST *) allocz( sizeof( RDEST ) );
	d->type   = NET_COMM_MAX;	
	d->qcount = RELAY_DEFAULT_QUEUES;
	d->data   = net_make_sock( 0, COAL_NETBUF_SZ, NULL, &(d->data_peer) );

	// set up the connection types
	d->data_peer.sin_family  = AF_INET;
	d->query_peer.sin_family = AF_INET;

	return d;
}




static RDEST *rd_cfg_curr = NULL;

int relay_config_line( AVP *av )
{
	struct hostent *he;
	RDEST *d;

	if( !rd_cfg_curr )
		rd_cfg_curr = relay_dest_create( );

	d = rd_cfg_curr;

	if( attIs( "name" ) )
	{
		if( d->name )
		{
			warn( 0x0401, "Relay destination '%s' also contains name '%s' - is a 'done' missing?",
				d->name, av->val );
			return -1;
		}

		d->name = str_dup( av->val, av->vlen );

		debug( 0x0402, "Relay destination %s config started.", d->name );
	}
	else if( attIs( "address" ) || attIs( "addr" ) )
	{
		if( !inet_aton( av->val, &(d->data_peer.sin_addr) ) )
		{
			warn( 0x0403, "Invalid IP address '%s' for relay destination %s",
				av->val, ( d->name ) ? d->name : "as yet unnamed" );
			return -1;
		}
		// and copy that for queries
		d->query_peer.sin_addr = d->data_peer.sin_addr;

		debug( 0x0404, "Relay destination %s has address %s",
			( d->name ) ? d->name : "as yet unnamed",
			inet_ntoa( d->data_peer.sin_addr ) );
	}
	else if( attIs( "dport" ) )
	{
		d->data_peer.sin_port = htons( (uint16_t) strtoul( av->val, NULL, 10 ) );

		debug( 0x0405, "Relay destination %s sends data to port %hu",
			( d->name ) ? d->name : "as yet unnamed", ntohs( d->data_peer.sin_port ) );
	}
	else if( attIs( "qport" ) )
	{
		d->query_peer.sin_port = htons( (uint16_t) strtoul( av->val, NULL, 10 ) );

		debug( 0x0406, "Relay destination %s sends queries to port %hu",
			( d->name ) ? d->name : "as yet unnamed", ntohs( d->query_peer.sin_port ) );
	}
	else if( attIs( "type" ) )
	{
		if( !strcasecmp( av->val, "line" ) )
		{
			d->type = NET_COMM_LINE;
			d->rfun = &relay_write_point_line;
		}
		else if( !strcasecmp( av->val, "bin" ) || !strcasecmp( av->val, "binary" ) )
		{
		 	d->type = NET_COMM_BIN;
			d->rfun = &relay_write_point_bin;
		}
		else
		{
			warn( 0x0407, "Unrecognised relay communications type '%s'", av->val );
			return -1;
		}
	}
	else if( attIs( "queues" ) )
	{
		d->qcount = atoi( av->val );
		if( d->qcount < 1 || d->qcount > 32 )
		{
			warn( 0x0408, "Invalid queues (%d) for relay destination %s - allowed values 1-32",
				d->qcount, ( d->name ) ? d->name : "as yet unnamed" );
			return -1;
		}
	}
	else if( attIs( "msec" ) )
	{
		ctl->relay->usdelay = 1000 * atoi( av->val );
	}
	else if( attIs( "done" ) )
	{
		if( !d->name )
		{
			warn( 0x0409, "Cannot process unnamed relay destination block." );
			return -1;
		}

		if( !d->data_peer.sin_addr.s_addr )
		{
			// attempt lookup on name as a hostname
			if( !( he = gethostbyname( d->name ) ) )
			{
				warn( 0x040a, "Failed to look up relay destination '%s' -- %s",
					d->name, Err );
				return -1;
			}
			// grab the first IP address
			inet_aton( he->h_addr, &(d->data_peer.sin_addr) );
			d->query_peer.sin_addr = d->data_peer.sin_addr;

			debug( 0x040b, "Converted relay name %s to IP address %s",
				d->name, inet_ntoa( d->data_peer.sin_addr ) );
		}

		// default to binary type comms for relaying
		if( d->type == NET_COMM_MAX )
			d->type = NET_COMM_BIN;

		// port defaults to 3801 or 3811
		if( !d->data_peer.sin_port )
		{
			if( d->type == NET_COMM_LINE )
				d->data_peer.sin_port = htons( DEFAULT_NET_LINE_DATA_PORT );
			else
				d->data_peer.sin_port = htons( DEFAULT_NET_BIN_DATA_PORT );

			debug( 0x040c, "Relay destination %s defaulting to port %hu for data",
				d->name, ntohs( d->data_peer.sin_port ) );
		}
		if( !d->query_peer.sin_port )
		{
			if( d->type == NET_COMM_LINE )
				d->query_peer.sin_port = htons( DEFAULT_NET_LINE_QUERY_PORT );
			else
				d->query_peer.sin_port = htons( DEFAULT_NET_BIN_QUERY_PORT );

			debug( 0x040d, "Relay destination %s defaulting to port %hu for queries",
				d->name, ntohs( d->query_peer.sin_port ) );
		}

		// winner!
		d->next = ctl->relay->dests;
		ctl->relay->dests = d;
		ctl->relay->dcount++;

		debug( 0x040e, "Relay destination %s created.", d->name );

		// and flatten the static
		rd_cfg_curr = NULL;
	}

	return 0;
}




REL_CTL *relay_config_defaults( void )
{
	REL_CTL *r;

	r = (REL_CTL *) allocz( sizeof( REL_CTL ) );
	r->usdelay = 1000 * RELAY_DEFAULT_MSEC;

	return r;
}

#undef LLFID

