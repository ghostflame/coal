#ifndef COAL_LOG_H
#define COAL_LOG_H

#ifndef Err
#define	Err		strerror( errno )
#endif

#define	DEFAULT_LOG_MAIN	"logs/coal.log"
#define	DEFAULT_LOG_NODE	"logs/coal.node.log"
#define DEFAULT_LOG_QUERY	"logs/coal.query.log"

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
};


// functions from log.c that we want to expose
int log_line( int dest, int level, const char *file, const int line, const char *fn, char *fmt, ... );
int log_close( void );
void log_reopen( int sig );
int log_start( void );

// config
LOG_CTL *log_config_defaults( void );
int log_config_line( AVP *av );


#define LLBAD( d, l, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l, __FILE__, __LINE__, __FUNCTION__, f, ## __VA_ARGS__ )
#define LLOK( d, l, f, ... )	log_line( LOG_DEST_##d, LOG_LEVEL_##l,     NULL,        0,         NULL, f, ## __VA_ARGS__ )


#define fatal( fmt, ... )		LLBAD( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define err( fmt, ... )			LLOK(  MAIN,  ERR,    fmt, ## __VA_ARGS__ )
#define warn( fmt, ... )		LLOK(  MAIN,  WARN,   fmt, ## __VA_ARGS__ )
#define notice( fmt, ... )		LLOK(  MAIN,  NOTICE, fmt, ## __VA_ARGS__ )
#define info( fmt, ... )		LLOK(  MAIN,  INFO,   fmt, ## __VA_ARGS__ )
#define debug( fmt, ... )		LLBAD( MAIN,  DEBUG,  fmt, ## __VA_ARGS__ )

#define qfatal( fmt, ... )		LLBAD( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define qerr( fmt, ... )		LLOK(  QUERY, ERR,    fmt, ## __VA_ARGS__ )
#define qwarn( fmt, ... )		LLOK(  QUERY, WARN,   fmt, ## __VA_ARGS__ )
#define qnotice( fmt, ... )		LLOK(  QUERY, NOTICE, fmt, ## __VA_ARGS__ )
#define qinfo( fmt, ... )		LLOK(  QUERY, INFO,   fmt, ## __VA_ARGS__ )
#define qdebug( fmt, ... )		LLBAD( QUERY, DEBUG,  fmt, ## __VA_ARGS__ )

#define nfatal( fmt, ... )		LLBAD( MAIN,  FATAL,  fmt, ## __VA_ARGS__ )
#define nerr( fmt, ... )		LLOK(  NODE,  ERR,    fmt, ## __VA_ARGS__ )
#define nwarn( fmt, ... )		LLOK(  NODE,  WARN,   fmt, ## __VA_ARGS__ )
#define nnotice( fmt, ... )		LLOK(  NODE,  NOTICE, fmt, ## __VA_ARGS__ )
#define ninfo( fmt, ... )		LLOK(  NODE,  INFO,   fmt, ## __VA_ARGS__ )
#define ndebug( fmt, ... )		LLBAD( NODE,  DEBUG,  fmt, ## __VA_ARGS__ )


#endif
