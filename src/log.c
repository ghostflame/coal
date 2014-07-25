#include "coal.h"

char *log_level_strings[LOG_LEVEL_MAX] =
{
	"FATAL",
	"ERR",
	"WARN",
	"NOTICE",
	"INFO",
	"DEBUG"
};


int log_get_level( char *str )
{
  	int i;

	if( !str || !*str )
	{
	  	warn( "Empty/null log level string." );
		// clearly, you need the help
		return LOG_LEVEL_DEBUG;
	}

	if( isdigit( *str ) )
	{
		i = atoi( str );
		if( i >= 0 && i < LOG_LEVEL_MAX )
		  	return i;

		warn( "Invalid log level string '%s'", str );
		return LOG_LEVEL_DEBUG;
	}

	for( i = LOG_LEVEL_FATAL; i < LOG_LEVEL_MAX; i++ )
		if( !strcasecmp( str, log_level_strings[i] ) )
		  	return i;


	warn( "Unrecognised log level string '%s'", str );
	return LOG_LEVEL_DEBUG;
}



int log_write_ts( char *to, int len )
{
	char tbuf[64];
	struct tm t;
	int usec;
	time_t sec;
#ifdef LOG_CURR_TIME
	double ct, ts, us;

	us   = modf( ctl->curr_time, &ts );
	usec = (int) ( 1000000.0 * us );
	sec  = (time_t) ts;
#else
	struct timeval tv;

	gettimeofday( &tv, NULL );
	sec  = tv.tv_sec;
	usec = tv.tv_usec;
#endif

	gmtime_r( &sec, &t );
	strftime( tbuf, 64, "%F %T", &t );

	return snprintf( to, len, "[%s.%06d]", tbuf, usec );
}


int log_line( int dest, int level, const char *file, const int line,
				const char *fn, char *fmt, ... )
{
	int fd = 2, l = 0, tofile = 0;
  	char buf[LOG_LINE_MAX];
	LOG_FILE *lf = NULL;
	va_list args;

	// stderr if we aren't set up yet
	// we don't daemonize until after setup
	if( ctl && ctl->log )
	{
		switch( dest )
		{
			case LOG_DEST_QUERY:
				lf = &(ctl->log->query);
				break;
			case LOG_DEST_NODE:
				lf = &(ctl->log->node);
				break;
			case LOG_DEST_RELAY:
				lf = &(ctl->log->relay);
				break;
			default:
				lf = &(ctl->log->main);
				break;
		}

		// are we logging this deeply?
		if( level > lf->level )
			return 0;

		fd = lf->fd;
		tofile = 1;
	}

	// can we write?
	if( fd < 0 )
		return -1;

	// write the predictable parts
	l  = log_write_ts( buf, LOG_LINE_MAX );

	l += snprintf( buf + l, LOG_LINE_MAX - l, " [%s] ",
			log_level_strings[level] );


	// fatals, errs and debugs get details
	switch( level )
	{
		case LOG_LEVEL_FATAL:
		case LOG_LEVEL_DEBUG:
			l += snprintf( buf + l, LOG_LINE_MAX - l,
					"[%s:%d:%s] ", file, line, fn );
			break;
	}

	// and we're off
	va_start( args, fmt );
	l += vsnprintf( buf + l, LOG_LINE_MAX - l, fmt, args );
	va_end( args );

	// add a newline
	if( buf[l - 1] != '\n' )
	{
		buf[l++] = '\n';
		buf[l]   = '\0';
	}


	if( lf )
	{
		// maybe always log to stdout?
	  	if( ctl->log->force_stdout )
			printf( "%s", buf );

		// but write to log file
		if( tofile > 0 )
			l = write( fd, buf, l );
	}
	else
	  	// nowhere else to go...
	  	l = printf( "%s", buf );

	// FATAL never returns
	if( level == LOG_LEVEL_FATAL )
		shut_down( 1 );

	return l;
}


int __log_open( LOG_FILE *lf )
{
	// require a valid fd, and ! stderr
	if( lf->fd >= 0
	 && lf->fd != 2 )
		close( lf->fd );

	if( ( lf->fd = open( lf->filename, O_WRONLY|O_APPEND|O_CREAT, 0644 ) ) < 0 )
	{
		fprintf( stderr, "Unable to open log file '%s' -- %s\n",
			lf->filename, Err );
		return -1;
	}

	return 0;
}

void __log_close( LOG_FILE *lf )
{
	close( lf->fd );
	lf->fd = 2;
}


int log_close( void )
{
	if( ctl && ctl->log )
	{
		__log_close( &(ctl->log->main) );
		__log_close( &(ctl->log->node) );
		__log_close( &(ctl->log->query) );
		__log_close( &(ctl->log->relay) );
	}

	return 0;
}

// in response to a signal
void log_reopen( int sig )
{
  	int ret = 0;

  	if( ctl && ctl->log )
	{
		ret += __log_open( &(ctl->log->main) );
		ret += __log_open( &(ctl->log->node) );
		ret += __log_open( &(ctl->log->query) );
		ret += __log_open( &(ctl->log->relay) );
	}

	if( ret )
		warn( "Failed to reopen at least one of our logs." );
}



int log_start( void )
{
	int ret = 0;

	if( !ctl || !ctl->log )
		return -1;

	ret += __log_open( &(ctl->log->main) );
	notice( "Coal logging started." );

	ret += __log_open( &(ctl->log->query) );
	qnotice( "Coal query logging started." );

	ret += __log_open( &(ctl->log->node) );
	nnotice( "Coal node logging started." );

	ret += __log_open( &(ctl->log->relay) );
	rnotice( "Coal relay logging started." );

	return ret;
}

// default log config
LOG_CTL *log_config_defaults( void )
{
	LOG_CTL *l;

	l = (LOG_CTL *) allocz( sizeof( LOG_CTL ) );

	l->main.filename  = strdup( DEFAULT_LOG_MAIN );
	l->main.level     = LOG_LEVEL_INFO;
	l->main.fd        = fileno( stderr );

	l->node.filename  = strdup( DEFAULT_LOG_NODE );
	l->node.level     = LOG_LEVEL_INFO;
	l->node.fd        = fileno( stderr );

	l->query.filename = strdup( DEFAULT_LOG_QUERY );
	l->query.level    = LOG_LEVEL_NOTICE;
	l->query.fd       = fileno( stderr );

	l->relay.filename = strdup( DEFAULT_LOG_RELAY );
	l->relay.level    = LOG_LEVEL_NOTICE;
	l->relay.fd       = fileno( stderr );
	
	l->force_stdout   = 0;

	return l;
}


int log_config_line( AVP *av )
{
	LOG_FILE *list[8];
	int llen, i, lvl;
	char *d;

	if( !( d = memchr( av->att, '.', av->alen ) ) )
		return -1;
	d++;

	switch( av->att[0] )
	{
		case 'm':
			if( strncasecmp( av->att, "main.", 5 ) )
				return -1;
			list[0] = &(ctl->log->main);
			llen = 1;
			break;
		case 'q':
			if( strncasecmp( av->att, "query.", 6 ) )
				return -1;
			list[0] = &(ctl->log->query);
			llen = 1;
			break;
		case 'n':
			if( strncasecmp( av->att, "node.", 5 ) )
				return -1;
			list[0] = &(ctl->log->node);
			llen = 1;
			break;
		case 'r':
			if( strncasecmp( av->att, "relay.", 6 ) )
				return -1;
			list[0] = &(ctl->log->relay);
			llen = 1;
			break;
		case 'a':
			if( strncasecmp( av->att, "all.", 4 ) )
				return -1;
			list[0] = &(ctl->log->main);
			list[1] = &(ctl->log->query);
			list[2] = &(ctl->log->node);
			list[3] = &(ctl->log->relay);
			llen = 4;
			break;
		default:
			return -1;
	}

	if( !strcasecmp( d, "filename" ) )
	{
		if( llen != 1 )
		{
			warn( "Cannot set filename of all log types." );
			return -1;
		}
		free( list[0]->filename );
		list[0]->filename = strdup( av->val );
	}
	else if( !strcasecmp( d, "level" ) )
	{
		// get the log level
		lvl = log_get_level( av->val );
		if( ctl->run_flags & RUN_DEBUG )
			lvl = LOG_LEVEL_DEBUG;

		// and set it
		for( i = 0 ; i < llen; i++ )
			list[i]->level = lvl;
	}
	else if( !strcasecmp( d, "disabled" ) )
	{
		lvl = ( ctl->run_flags & RUN_DEBUG ) ? LOG_LEVEL_DEBUG : -1;
		for( i = 0; i < llen; i++ )
			list[i]->level = lvl;
	}
	else
		return -1;

	return 0;
}


