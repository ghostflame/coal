#include "coal.h"

#define LLFID LLFMA

void usage( void )
{
	char *help_str = "\
Usage\tcoal -h\n\
\tcoal [OPTIONS] -c <config file>\n\n\
Options:\n\
 -h             Print this help\n\
 -c <file>      Select config file to use\n\
 -d             Daemonize into the background\n\
 -D             Switch on debug output (overrides config)\n\
 -v             Verbose logging to console\n\
 -t             Just test the config is valid and exit\n\n\
Coal is a graphite-alternative data storage engine.  It runs on very\n\
similar lines, taking data points against dot-delimited paths and\n\
storing it against timestamps.  See the documentation for the details.\n\n";

	printf( "%s", help_str );
	exit( 0 );
}


void shut_down( int exval )
{
	int i = 0;

	get_time( );
	info( 0x0201, "Coal shutting down after %.3fs",
		ctl->curr_time - ctl->start_time );

	net_stop( );
	relay_stop( );

	info( 0x0202, "Waiting for all threads to stop." );

	// wait a maximum of 30 seconds
	i = 0;
	while( ctl->loop_count > 0 && i++ < 300 )
		usleep( 10000 );

	if( ctl->loop_count <= 0 )
		info( 0x0203, "All threads have completed." );
	else
	  	warn( 0x0204, "Shutting down without thread completion!" );

	// shut down all those mutex locks
	lock_shutdown( );

	pidfile_remove( );

	notice( 0x0205, "Coal exiting." );
	log_close( );
	exit( exval );
}


int set_signals( void )
{
	struct sigaction sa;

	memset( &sa, 0, sizeof( struct sigaction ) );
	sa.sa_handler = loop_kill;
	sa.sa_flags   = SA_NOMASK;

	if( sigaction( SIGTERM, &sa, NULL )
	 || sigaction( SIGQUIT, &sa, NULL )
	 || sigaction( SIGINT,  &sa, NULL ) )
	{
		err( 0x0301, "Could not set exit signal handlers -- %s", Err );
		return -1;
	}

	// log bounce
	sa.sa_handler = log_reopen;
	if( sigaction( SIGHUP, &sa, NULL ) )
	{
		err( 0x0302, "Could not set hup signal handler -- %s", Err );
		return -2;
	}

	return 0;
} 




int main( int ac, char **av )
{
	int oc, testConf;

	// just test config?
	testConf = 0;

	// make a control structure
	ctl = create_config( );

	while( ( oc = getopt( ac, av, "hDvdtc:" ) ) != -1 )
		switch( oc )
		{
		  	case 'c':
				free( ctl->cfg_file );
				ctl->cfg_file = strdup( optarg );
				break;
			case 'd':
				ctl->run_flags |= RUN_DAEMON;
				break;
			case 'D':
				ctl->log->main.level  = LOG_LEVEL_DEBUG;
				ctl->log->query.level = LOG_LEVEL_DEBUG;
				ctl->log->node.level  = LOG_LEVEL_DEBUG;
				ctl->run_flags |= RUN_DEBUG;
				break;
			case 'v':
				ctl->log->force_stdout = 1;
				break;
			case 't':
				testConf = 1;
				break;
			case 'h':
			default:
				usage( );
				break;
		}


	if( get_config( ctl->cfg_file ) )
		fatal( 0x0101, "Unable to read config file '%s'\n", ctl->cfg_file );

	// can we chdir to our base dir?
	if( chdir( ctl->basedir ) )
		fatal( 0x0102, "Unable to chdir to base dir '%s' -- %s",
			ctl->basedir, Err );
	else
		debug( 0x0103, "Working directory switched to %s", ctl->basedir );

	// match up relay rules against destinations
	if( config_check_relay( ) )
		fatal( 0x0104, "Unable to validate relay config." );

	// were we just testing config?
	if( testConf )
	{
		printf( "Configuration is valid.\n" );
		return 0;
	}

	pidfile_write( );

	// open our log file and get going
	if( !ctl->log->force_stdout )
		debug( 0x0105, "Starting logging - no more logs to stdout." );

	log_start( );
	notice( 0x0106, "Coal starting up." );

	if( ctl->run_flags & RUN_DAEMON )
	{
		if( daemon( 1, 0 ) < 0 )
		{
			warn( 0x0107, "Unable to daemonize -- %s", Err );
			fprintf( stderr, "Unable to daemonize -- %s", Err );
			ctl->run_flags &= ~RUN_DAEMON;
		}
		else
			info( 0x0108, "Coal running in daemon mode, pid %d.", getpid( ) );
	}

	if( net_start( ) )
		fatal( 0x0109, "Failed to start networking." );

	if( relay_start( ) )
		fatal( 0x010a, "Failed to start relay connections." );

	if( set_signals( ) )
		fatal( 0x010b, "Failed to set signalling." );

	if( node_start_discovery( ) )
		fatal( 0x010c, "Unable to begin looking for existing nodes." );

	info( 0x010d, "Discovered %u branch and %u leaf nodes.",
		ctl->node->branches, ctl->node->leaves,
		( ctl->node->node_id == 1 ) ? "" : "s" );

	get_time( );
	info( 0x010e, "Coal started up in %.3fs.",
		ctl->curr_time - ctl->start_time );

	// run the loop forever
	loop_run( );

	// close down properly
	shut_down( 0 );

	// this never happens but it keeps gcc happy
	return 0;
}

#undef LLFID

