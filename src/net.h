#ifndef COAL_NETWORK_H
#define COAL_NETWORK_H

// some of these live in libcoal.h
#define DEFAULT_NET_LINE_DATA_PORT		COAL_LINE_DATA_PORT
#define DEFAULT_NET_LINE_QUERY_PORT		COAL_LINE_QUERY_PORT
#define DEFAULT_NET_LINE_ENABLED		1

#define DEFAULT_NET_BIN_DATA_PORT		COAL_BIN_DATA_PORT
#define DEFAULT_NET_BIN_QUERY_PORT		COAL_BIN_QUERY_PORT
#define DEFAULT_NET_BIN_ENABLED			0

#define POLL_EVENTS						(POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND|POLLHUP)

#define COAL_NETBUF_SZ					0x10000	// 64k

#define HOST_CLOSE						0x01
#define HOST_NEW						0x02


#define NET_DEAD_CONN_TIMER				3600.0	// 1 hr


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
	double					dead_time;
};


struct net_buffer
{
	unsigned char		*	buf;
	unsigned char		*	hwmk;
	int						len;
	int						sz;
};


// socket for talking to a host
struct net_socket
{
	NBUF					out;
	NBUF					in;
	// this one has no buffer allocation
	NBUF					keep;

	int						sock;
	int						flags;

	struct sockaddr_in	*	peer;
	char				*	name;
};



struct host_data
{
	HOST				*	next;
	NSOCK				*	net;

	struct sockaddr_in		peer;

	double					started;	// accept time
	double					last;		// last communication

	int						type;		// line or bin
	unsigned long			points;		// counter
	unsigned long			queries;	// counter

	WORDS				*	all;		// each line
	WORDS				*	val;		// per line

	query_read_fn		*	qrf;
	data_read_fn		*	drf;
};

// thread control
void *net_watched_socket( void *arg );

// client connections
HOST *net_get_host( int sock, int type );
void net_close_host( HOST *h );

NSOCK *net_make_sock( int insz, int outsz, char *name, struct sockaddr_in *peer );
int net_port_sock( PORT_CTL *pc, uint32_t ip, int backlog );
void net_disconnect( int *sock, char *name );
int net_connect( NSOCK *s );

// r/w
int net_write_data( NSOCK *s );
int net_read_data( NSOCK *s );
int net_read_bin( HOST *h );
int net_read_lines( HOST *h );

// init/shutdown
int net_start( void );
void net_stop( void );

// config
NET_CTL *net_config_defaults( void );
int net_config_line( AVP *av );

#endif
