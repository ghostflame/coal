#ifndef COAL_RELAY_H
#define COAL_RELAY_H

#define RELAY_DEFAULT_MSEC			200
#define RELAY_DEFAULT_QUEUES		5
#define RELAY_OUTBUF_SZ				0x10000	// 64k


// created by a query connection
struct relay_query_conn
{
	RDEST			*	dest;

	int					sock;

	unsigned char	*	inbuf;
	unsigned char	*	outbuf;

	int					inlen;
	int					outlen;
};




// destination target
struct relay_destination
{
	RDEST			*	next;
	char			*	name;		// name of the destination

	struct sockaddr_in	data_peer;	// data destination
	struct sockaddr_in	query_peer;

	int					type;		// line or binary
	int					errors;		// current error count

	// data connection
	NSOCK			*	data;

	// query conns are not held here

	// defaults to 5
	int					qcount;

	pthread_mutex_t		qr_ctl;
	pthread_mutex_t		pt_ctl;

	pthread_mutex_t	*	locks;

	// point from data thread collect here
	// under locking
	POINT			**	incoming;

	// then are moved here under lock to be
	// sent out
	POINT			*	outgoing;
};


struct relay_control
{
	RDEST			*	dests;
	int					dcount;
	int					usdelay;
};

throw_fn relay_flush;
throw_fn relay_loop;

void relay_add_point( RDEST *d, POINT *p );

int relay_start( void );
void relay_stop( void );

int relay_config_line( AVP *av );
REL_CTL *relay_config_defaults( void );


#endif
