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


struct log_file
{
	char			*	filename;
	int					level;
	int					fd;
};

struct log_control
{
	LOG_FILE			main;
	LOG_FILE			query;
	LOG_FILE			node;
	LOG_FILE			relay;
};


// functions from log.c that we want to expose
int log_line( int dest, int level, const char *file, const int line, const char *fn, char *fmt, ... );
int log_close( void );
void log_reopen( int sig );
int log_start( void );

// config
LOG_CTL *log_config_defaults( void );
int log_config_line( AVP *av );


#define LLFLF( d, l, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l, __FILE__, __LINE__, __FUNCTION__, f, ## __VA_ARGS__ )
#define LLNZN( d, l, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l,     NULL,        0,         NULL, f, ## __VA_ARGS__ )

#define fatal( fmt, ... )		LLFLF( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define err( fmt, ... )			LLNZN( MAIN,  ERR,    fmt, ## __VA_ARGS__ )
#define warn( fmt, ... )		LLNZN( MAIN,  WARN,   fmt, ## __VA_ARGS__ )
#define notice( fmt, ... )		LLNZN( MAIN,  NOTICE, fmt, ## __VA_ARGS__ )
#define info( fmt, ... )		LLNZN( MAIN,  INFO,   fmt, ## __VA_ARGS__ )
#define debug( fmt, ... )		LLFLF( MAIN,  DEBUG,  fmt, ## __VA_ARGS__ )

#define qfatal( fmt, ... )		LLFLF( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define qerr( fmt, ... )		LLNZN( QUERY, ERR,    fmt, ## __VA_ARGS__ )
#define qwarn( fmt, ... )		LLNZN( QUERY, WARN,   fmt, ## __VA_ARGS__ )
#define qnotice( fmt, ... )		LLNZN( QUERY, NOTICE, fmt, ## __VA_ARGS__ )
#define qinfo( fmt, ... )		LLNZN( QUERY, INFO,   fmt, ## __VA_ARGS__ )
#define qdebug( fmt, ... )		LLFLF( QUERY, DEBUG,  fmt, ## __VA_ARGS__ )

#define nfatal( fmt, ... )		LLFLF( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define nerr( fmt, ... )		LLNZN( NODE,  ERR,    fmt, ## __VA_ARGS__ )
#define nwarn( fmt, ... )		LLNZN( NODE,  WARN,   fmt, ## __VA_ARGS__ )
#define nnotice( fmt, ... )		LLNZN( NODE,  NOTICE, fmt, ## __VA_ARGS__ )
#define ninfo( fmt, ... )		LLNZN( NODE,  INFO,   fmt, ## __VA_ARGS__ )
#define ndebug( fmt, ... )		LLFLF( NODE,  DEBUG,  fmt, ## __VA_ARGS__ )

#define rfatal( fmt, ... )		LLFLF( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define rerr( fmt, ... )		LLNZN( RELAY, ERR,    fmt, ## __VA_ARGS__ )
#define rwarn( fmt, ... )		LLNZN( RELAY, WARN,   fmt, ## __VA_ARGS__ )
#define rnotice( fmt, ... )		LLNZN( RELAY, NOTICE, fmt, ## __VA_ARGS__ )
#define rinfo( fmt, ... )		LLNZN( RELAY, INFO,   fmt, ## __VA_ARGS__ )
#define rdebug( fmt, ... )		LLFLF( RELAY, DEBUG,  fmt, ## __VA_ARGS__ )

#endif
