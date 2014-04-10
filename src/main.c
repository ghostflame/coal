#include "coal.h"

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
 -t             Just test the config is valid and exit\n\n\
Coal is a graphite-alternative data storage engine.  It runs on very\n\
similar lines, taking data points against dot-delimited paths and\n\
storing it against timestamps.  See the documentation for the details.\n\n";

	printf( "%s", help_str );
	exit( 0 );
}

int shut_down_is_complete( void )
{
	return 1;
}


void shut_down( int exval )
{
	int i = 0;

	get_time( );
	info( "Coal shutting down after %.3fs",
		ctl->curr_time - ctl->start_time );

	net_stop( );
	relay_stop( );

	info( "Waiting for all threads to stop." );


	// wait a maximum of 30 seconds
	i = 0;
	while( !shut_down_is_complete( ) && i++ < 300 )
		usleep( 10000 );

	if( shut_down_is_complete( ) )
		info( "All threads have completed." );
	else
	  	warn( "Shutting down without thread completion!" );

	// shut down all those mutex locks
	lock_shutdown( );

	pidfile_remove( );

	notice( "Coal exiting." );
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
		err( "Could not set exit signal handlers -- %s", Err );
		return -1;
	}

	// log bounce
	sa.sa_handler = log_reopen;
	if( sigaction( SIGHUP, &sa, NULL ) )
	{
		err( "Could not set hup signal handler -- %s", Err );
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

	while( ( oc = getopt( ac, av, "hDdtc:" ) ) != -1 )
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
			case 't':
				testConf = 1;
				break;
			case 'h':
			default:
				usage( );
				break;
		}


	if( get_config( ctl->cfg_file ) ) {
		fatal( "Unable to read config file '%s'\n", ctl->cfg_file );
	}

	// can we chdir to our base dir?
	if( chdir( ctl->basedir ) )
		fatal( "Unable to chdir to base dir '%s' -- %s",
			ctl->basedir, Err );

	// match up relay rules against destinations
	if( config_check_relay( ) ) {
		fatal( "Unable to validate relay config." );
	}

	// were we just testing config?
	if( testConf )
	{
		printf( "Configuration is valid.\n" );
		return 0;
	}

	pidfile_write( );

	// open our log file and get going
	debug( "Starting logging - no more logs to stderr." );
	log_start( );
	notice( "Coal starting up." );

	if( ctl->run_flags & RUN_DAEMON )
	{
		if( daemon( 0, 0 ) < 0 )
		{
			warn( "Unable to daemonize -- %s", Err );
			fprintf( stderr, "Unable to daemonize -- %s", Err );
			ctl->run_flags &= ~RUN_DAEMON;
		}
		else
			info( "Coal running in daemon mode, pid %d.", getpid( ) );
	}

	if( net_start( ) )
		fatal( "Failed to start networking." );

	if( relay_start( ) )
		fatal( "Failed to start relay connections." );

	if( set_signals( ) )
		fatal( "Failed to set signalling." );

	if( node_start_discovery( ) )
		fatal( "Unable to begin looking for existing nodes." );
	info( "Discovered %u branch and %u leaf nodes.",
		ctl->node->branches, ctl->node->leaves,
		( ctl->node->node_id == 1 ) ? "" : "s" );

	get_time( );
	info( "Coal started up in %.3fs.",
		ctl->curr_time - ctl->start_time );

	// run the loop forever
	loop_run( );

	// close down properly
	shut_down( 0 );

	// this never happens but it keeps gcc happy
	return 0;
}


