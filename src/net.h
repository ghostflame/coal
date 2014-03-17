#ifndef COAL_NETWORK_H
#define COAL_NETWORK_H


#define DEFAULT_NET_LINE_DATA_PORT		3801
#define DEFAULT_NET_LINE_QUERY_PORT		3802
#define DEFAULT_NET_LINE_ENABLED		1

#define DEFAULT_NET_BIN_DATA_PORT		3811
#define DEFAULT_NET_BIN_QUERY_PORT		3812
#define DEFAULT_NET_BIN_ENABLED			0

#define POLL_EVENTS						(POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND)

#define	MAX_PKT_IN						8192
#define MAX_PKT_OUT						1450	// tcp max seg, ish


#define HOST_CLOSE						0x01
#define HOST_NEW						0x02


enum net_comm_types
{
	NET_COMM_LINE = 0,
	NET_COMM_BIN,
	NET_COMM_MAX
};



struct port_control
{
	unsigned short			port;
	int						sock;
	unsigned long			conns;
	char				*	label;
};



struct net_type_control
{
	PORT_CTL			*	data;
	PORT_CTL			*	query;

	int						type;
	int						enabled;
	char				*	label;
};


struct network_control
{
	NET_TYPE_CTL		*	line;
	NET_TYPE_CTL		*	bin;

	int						status;
};


struct host_data
{
	HOST				*	next;

	struct sockaddr_in		peer;
	char				*	name;
	int						fd;
	int						flags;  // close, new

	char				*	inbuf;
	char				*	outbuf;

	char				*	keep;	// data held over
	int						keepLen;

	int						type;	// line or bin

	unsigned short			inlen;	// size of incoming
	unsigned short			outlen;	// size of outgoing

	unsigned long			points;	// counter

	WORDS				*	all;	// each line
	WORDS				*	val;	// per line
};


HOST *net_get_host( int sock, int type );
int net_port_sock( PORT_CTL *pc, uint32_t ip, int backlog );

// r/w
int net_write_data( HOST *h );
int net_read_data( HOST *h );
int net_read_lines( HOST *h );

// init/shutdown
int net_start( void );
void net_stop( void );

// config
NET_CTL *net_config_defaults( void );
int net_config_line( AVP *av );

#endif
