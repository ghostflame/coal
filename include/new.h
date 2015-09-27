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

#define BINF_TYPE_REPLY					0x01

#define BINF_TYPE_PING					0x02
#define BINF_TYPE_PONG					(BINF_TYPE_PING|BINF_TYPE_REPLY)
#define BINF_TYPE_DATA					0x04
#define BINF_TYPE_DATA_MULTI			0x06
#define BINF_TYPE_QUERY					0x08
#define BINF_TYPE_QUERY_RET				(BINF_TYPE_QUERY|BINF_TYPE_REPLY)



// default ports
#define COAL_LINE_DATA_PORT				3801
#define COAL_LINE_QUERY_PORT			3802

#define COAL_BIN_DATA_PORT				3811
#define COAL_BIN_QUERY_PORT				3812


#define COAL_ERRBUF_SZ					4096

#define COAL_QUERY_SLEEP_USEC			4000

#ifndef Err
#define Err strerror( errno )
#endif



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
	LCE_RECV_ERROR,
	LCE_BAD_QANS,
	LCE_MAX
};


enum libcoal_query_types
{
	QUERY_TYPE_INVALID = -1,
	QUERY_TYPE_DATA,
	QUERY_TYPE_TREE,
	QUERY_TYPE_SEARCH,
	QUERY_TYPE_MAX
};



typedef struct libcoal_tree_element			COALTREE;
typedef struct libcoal_data					COALDATA;
typedef struct libcoal_search_list			COALSRCL;

typedef struct libcoal_qanswer_base			COALQANS;
typedef struct libcoal_qanswer_invalid		COALQINV;
typedef struct libcoal_qanswer_tree			COALQTR;
typedef struct libcoal_qanswer_data			COALQDAT;
typedef struct libcoal_qanswer_search		COALQSRC;

typedef struct libcoal_query				COALQRY;
typedef struct libcoal_query_list			COALQRYL;



struct libcoal_tree_element
{
	COALTREE	*	next;
	COALTREE	*	children;
	char		*	name;
	char		*	path;
	uint8_t			nlen;
	uint8_t			leaf;
	uint16_t		plen;
};

struct libcoal_data
{
	COALDATA	*	next;
	int				count;
	C3PNT		*	points;
	// one for each to indicate validity
	uint8_t		*	flags;
};



// present in each structure
#define LIBCOAL_QUERY_HEADER	\
	uint8_t			version;\
	uint8_t			type;\
	uint8_t			qtype;\
	uint8_t			id;\
	uint32_t		size


struct libcoal_qanswer_base
{
	LIBCOAL_QUERY_HEADER;
};


struct libcoal_qanswer_invalid
{
	LIBCOAL_QUERY_HEADER;

	char			errmsg[];
};


struct libcoal_qanswer_tree
{
	LIBCOAL_QUERY_HEADER;

	uint8_t			ntype;
	uint8_t			_pad;
	uint16_t		rcount;

	// the data then contains
	// rcount * {
	// ntype (uint8_t, 1 = leaf)
	// padding (uint8_t)
	// path length (uint16_t)
	// }
	// rcount * paths, variable length

	// not a direct map
	COALTREE	*	results;
};

struct libcoal_qanswer_data
{
	LIBCOAL_QUERY_HEADER;

	uint32_t		start;
	uint32_t		end;
	uint8_t			metric;
	uint8_t			_pad;
	uint16_t		pathlen;
	uint32_t		rcount;

	// the data then contains
	// rcount * {
	// time (uint32_t),
	// flags (uint32_t, 0x1: valid)
	// value (float32)
	// }
	// then the path, variable length

	// not a direct map
	char		*	path;
	COALDATA	*	results;
};


struct libcoal_qanswer_search
{
	LIBCOAL_QUERY_HEADER;

	uint32_t		rcount;

	// the data contains then
	// rcount * path lengths (uint16_t)
	// rcount * paths, variable length

	// not a direct map
	COALSRCL	*	results;
};


struct libcoal_query
{
	LIBCOAL_QUERY_HEADER;

	uint32_t		start;
	uint32_t		end;
	uint8_t			metric;
	uint8_t			_pad;
	uint16_t		pathlen;
	char			path[1024];
};



struct libcoal_query_list
{
	COALQRYL	*	next;
	COALQRY		*	query;

	COALQANS	*	answer;

	int8_t			state;
	int8_t			type;
};



enum LIBCOAL_QUERY_STATES
{
	COAL_QUERY_EMPTY = 0,
	COAL_QUERY_PREPD,
	COAL_QUERY_SENT,
	COAL_QUERY_WAITING,
	COAL_QUERY_READ,
	COAL_QUERY_INVALID,
	COAL_QUERY_COMPLETE
};



#endif
