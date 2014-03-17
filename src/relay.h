#ifndef COAL_RELAY_H
#define COAL_RELAY_H



// destination connection
struct relay_connection
{
	RCONN			*	next;
	RDEST			*	dest;

	int					sock;
	short				type;
	short				query;

	POINT			*	incoming;
	POINT			*	outgoing;

	QUERY			*	pending;
	QUERY			*	inflight;
};


// destination target
struct relay_destination
{
	RDEST			*	next;
	char			*	name;		// name of the destination
	struct sockaddr_in	data;
	struct sockaddr_in	query;
	int					type;		// line or binary
	int					errors;		// current error count
};


struct relay_control
{
	RDEST			*	dests;
	int					dcount;
	RCONN			*	conns;
};



void relay_add_point( RDEST *d, POINT *p );


int relay_config_line( AVP *av );
REL_CTL *relay_config_defaults( void );


#endif
