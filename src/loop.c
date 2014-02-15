#include "coal.h"


void loop_end( char *reason )
{
  	info( "Shutting down polling: %s", reason );
	ctl->run_flags |= RUN_SHUTDOWN;
	ctl->run_flags &= ~RUN_LOOP;
}


void loop_kill( int sig )
{
	notice( "Received signal %d", sig );
	ctl->run_flags |= RUN_SHUTDOWN;
	ctl->run_flags &= ~RUN_LOOP;
}

void loop_run( void )
{
	ctl->run_flags |= RUN_LOOP;

	throw_thread( sync_loop, NULL );
	throw_thread( push_loop, NULL );

	if( ctl->net->line->enabled )
	{
		throw_thread( data_loop,  ctl->net->line );		// data.c
		throw_thread( query_loop, ctl->net->line );		// query.c
	}

	if( ctl->net->bin->enabled )
	{
		throw_thread( data_loop,  ctl->net->bin );		// data.c
		throw_thread( query_loop, ctl->net->bin );		// query.c
	}

	while( ctl->run_flags & RUN_LOOP )
	{
		// get an accurate time
		get_time( );

		// and sleep a bit
		usleep( ctl->sync->tick_usec );
	}
}

