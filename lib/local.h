#ifndef LIBCOAL_LOCAL_H
#define LIBCOAL_LOCAL_H

#include <libcoal.h>

#define OUTBUF_SZ			0x10000
#define INBUF_SZ			0x10000




#define herr( e, f, ... )	h->errnum = LCE_##e; \
							snprintf( h->errbuf, COAL_ERRBUF_SZ, f, ## __VA_ARGS__ )


typedef struct libcoal_data_point	COALDAT;


struct libcoal_data_point
{
	unsigned char			vers;
	unsigned char			type;
	unsigned short			size;
	time_t					ts;
	float					val;
	char					path[];
};


struct libcoal_connection
{
	struct sockaddr_in		to;
	int						sock;
	int						enabled;
	int						connected;

	unsigned char		*	outbuf;
	unsigned char		*	wrptr;
	unsigned char		*	hwmk;

	unsigned char		*	inbuf;
	unsigned char		*	rdptr;
	unsigned char		*	keep;
	int						inlen;
	int						keepLen;
};



struct libcoal_handle
{
	COALCONN			*	data;
	COALCONN			*	query;

	char				*	host;
	struct in_addr			svr;
	unsigned short			dport;
	unsigned short			qport;

	char				*	errbuf;
	int						errnum;
};

// INTERNAL FUNCTIONS

// init.c
void *allocz( int size );



#endif
