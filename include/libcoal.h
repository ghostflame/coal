#ifndef LIBCOAL_H
#define LIBCOAL_H

#define _GNU_SOURCE

#include <poll.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <c3db.h>


#define BINF_TYPE_DATA					0x02
#define BINF_TYPE_QUERY					0x08
#define BINF_TYPE_QUERY_RET				0x09
#define BINF_TYPE_TREE					0x10
#define BINF_TYPE_TREE_RET				0x11


// default ports
#define COAL_LINE_DATA_PORT				3801
#define COAL_LINE_QUERY_PORT			3802

#define COAL_BIN_DATA_PORT				3811
#define COAL_BIN_QUERY_PORT				3812


#define COAL_ERRBUF_SZ					1024


#ifndef Err
#define Err strerror( errno )
#endif


typedef struct libcoal_handle       COALH;
typedef struct libcoal_connection   COALCONN;
typedef struct libcoal_point		COALPT;
typedef struct libcoal_data_answer	COALDANS;
typedef struct libcoal_tree_answer	COALTANS;
typedef struct libcoal_query        COALQRY;


// ERRORS
enum libcoal_errors
{
	LCE_BAD_HOST,
	LCE_MAKE_SOCKET,
	LCE_SET_KEEPALIVE,
	LCE_CONNECT,
	LCE_NOT_CONNECTED,
	LCE_NOT_ENABLED,
	LCE_POLL_ERROR,
	LCE_NO_QRY,
	LCE_EXISTING_QRY,
	LCE_MAX
};


struct libcoal_point
{
	time_t			ts;
	float			val;
	int				valid;
};


struct libcoal_tree_answer
{
	int				count;
	int			*	lengths;
	char		**	children;
};

struct libcoal_data_answer
{
	time_t			start;
	time_t			end;
	int				count;
	COALPT		*	points;
};



struct libcoal_query
{
	COALQRY		*	next;
	char		*	path;
	int				len;

	int				tq;		// tree query
	int				answer;

	time_t			start;
	time_t			end;
	int				metric;

	COALDANS	*	data;
	COALTANS	*	tree;
};




// Functions

// data.c
int libcoal_data_add( COALH *h, time_t ts, float val, char *path, int len );

// net.c
int libcoal_net_flush( COALH *h, COALCONN *c );
int libcoal_connect( COALH *h );

// query.c
int libcoal_data_query( COALH *H, COALQRY **qp, char *path, int len, time_t from, time_t to, int metric );
int libcoal_tree_query( COALH *H, COALQRY **qp, char *path, int len );

#endif
