#ifndef COAL_LOG_H
#define COAL_LOG_H

#ifndef Err
#define	Err		strerror( errno )
#endif

#define	DEFAULT_LOG_MAIN	"logs/coal.log"
#define	DEFAULT_LOG_NODE	"logs/coal.node.log"
#define DEFAULT_LOG_QUERY	"logs/coal.query.log"
#define DEFAULT_LOG_RELAY	"logs/coal.relay.log"

#define LOG_LINE_MAX		8192
#define LOG_BUF_SZ			0x100000	// 1M

enum log_levels
{
	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_NOTICE,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_MAX
};

enum log_destinations
{
	LOG_DEST_MAIN = 0,
	LOG_DEST_NODE,
	LOG_DEST_QUERY,
	LOG_DEST_RELAY,
	LOG_DEST_MAX
};

struct log_buffer
{
	LOG_BUF			*	next;
	char			*	buf;
	int					size;
	int					used;
};


struct log_file
{
	char			*	filename;
	int					level;
	int					fd;
	LOG_BUF			*	buffer;
};

struct log_control
{
	LOG_FILE			main;
	LOG_FILE			query;
	LOG_FILE			node;
	LOG_FILE			relay;
	int					copy_stdout;
	int					force_stdout;
};


// functions from log.c that we want to expose
int log_line( int dest, int level, const char *file, const int line, const char *fn, unsigned int id, char *fmt, ... );
void log_buffer( int on_off, LOG_FILE *which );
void log_copy_stdout( int on_off );
int log_close( void );
void log_reopen( int sig );
int log_start( void );

// config
LOG_CTL *log_config_defaults( void );
int log_config_line( AVP *av );

// per-logfile identifiers
// NEVER EVER EDIT THESE
// only append new ones
#define LLMID		0x10000000
#define LLQID		0x20000000
#define LLNID		0x30000000
#define LLRID		0x40000000


// LLFID exists in each .c file, defined at the start and undef'd at the end

#define LLFLF( d, l, i, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l, __FILE__, __LINE__, __FUNCTION__, (LLFID|i), f, ## __VA_ARGS__ )
#define LLNZN( d, l, i, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l,     NULL,        0,         NULL, (LLFID|i), f, ## __VA_ARGS__ )

#define fatal( i, fmt, ... )		LLFLF( MAIN,  FATAL,  (LLMID|i), fmt, ## __VA_ARGS__ )
#define err( i, fmt, ... )			LLNZN( MAIN,  ERR,    (LLMID|i), fmt, ## __VA_ARGS__ )
#define warn( i, fmt, ... )			LLNZN( MAIN,  WARN,   (LLMID|i), fmt, ## __VA_ARGS__ )
#define notice( i, fmt, ... )		LLNZN( MAIN,  NOTICE, (LLMID|i), fmt, ## __VA_ARGS__ )
#define info( i, fmt, ... )			LLNZN( MAIN,  INFO,   (LLMID|i), fmt, ## __VA_ARGS__ )
#define debug( i, fmt, ... )		LLFLF( MAIN,  DEBUG,  (LLMID|i), fmt, ## __VA_ARGS__ )

#define qfatal( i, fmt, ... )		LLFLF( MAIN,  FATAL,  (LLQID|i), fmt, ## __VA_ARGS__ )
#define qerr( i, fmt, ... )			LLNZN( QUERY, ERR,    (LLQID|i), fmt, ## __VA_ARGS__ )
#define qwarn( i, fmt, ... )		LLNZN( QUERY, WARN,   (LLQID|i), fmt, ## __VA_ARGS__ )
#define qnotice( i, fmt, ... )		LLNZN( QUERY, NOTICE, (LLQID|i), fmt, ## __VA_ARGS__ )
#define qinfo( i, fmt, ... )		LLNZN( QUERY, INFO,   (LLQID|i), fmt, ## __VA_ARGS__ )
#define qdebug( i, fmt, ... )		LLFLF( QUERY, DEBUG,  (LLQID|i), fmt, ## __VA_ARGS__ )

#define nfatal( i, fmt, ... )		LLFLF( MAIN,  FATAL,  (LLNID|i), fmt, ## __VA_ARGS__ )
#define nerr( i, fmt, ... )			LLNZN( NODE,  ERR,    (LLNID|i), fmt, ## __VA_ARGS__ )
#define nwarn( i, fmt, ... )		LLNZN( NODE,  WARN,   (LLNID|i), fmt, ## __VA_ARGS__ )
#define nnotice( i, fmt, ... )		LLNZN( NODE,  NOTICE, (LLNID|i), fmt, ## __VA_ARGS__ )
#define ninfo( i, fmt, ... )		LLNZN( NODE,  INFO,   (LLNID|i), fmt, ## __VA_ARGS__ )
#define ndebug( i, fmt, ... )		LLFLF( NODE,  DEBUG,  (LLNID|i), fmt, ## __VA_ARGS__ )

#define rfatal( i, fmt, ... )		LLFLF( MAIN,  FATAL,  (LLRID|i), fmt, ## __VA_ARGS__ )
#define rerr( i, fmt, ... )			LLNZN( RELAY, ERR,    (LLRID|i), fmt, ## __VA_ARGS__ )
#define rwarn( i, fmt, ... )		LLNZN( RELAY, WARN,   (LLRID|i), fmt, ## __VA_ARGS__ )
#define rnotice( i, fmt, ... )		LLNZN( RELAY, NOTICE, (LLRID|i), fmt, ## __VA_ARGS__ )
#define rinfo( i, fmt, ... )		LLNZN( RELAY, INFO,   (LLRID|i), fmt, ## __VA_ARGS__ )
#define rdebug( i, fmt, ... )		LLFLF( RELAY, DEBUG,  (LLRID|i), fmt, ## __VA_ARGS__ )


// file-based LOG strs
// NEVER EVER EDIT THESE
// only append
#define LLFCF	0x00100000
#define LLFDA	0x00200000
#define LLFLG	0x00300000
#define LLFLO	0x00400000
#define LLFMA	0x00500000
#define LLFME	0x00600000
#define LLFNE	0x00700000
#define LLFNO	0x00800000
#define LLFQU	0x00900000
#define LLFRE	0x00a00000
#define LLFST	0x00b00000
#define LLFSY	0x00c00000
#define LLFTH	0x00d00000
#define LLFUT	0x00e00000
#define LLFSE	0x00f00000
#define LLFOB	0x01000000
#define LLFOJ	0x01100000
#define LLFOL	0x01200000
#define LLFPR	0x01300000


#endif
