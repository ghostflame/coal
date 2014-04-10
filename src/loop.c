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

	thread_throw( sync_loop, NULL );
	thread_throw( push_loop, NULL );

	if( ctl->net->line->enabled )
	{
		thread_throw( data_loop,  ctl->net->line );		// data.c
		thread_throw( query_loop, ctl->net->line );		// query.c
	}

	if( ctl->net->bin->enabled )
	{
		thread_throw( data_loop,  ctl->net->bin );		// data.c
		thread_throw( query_loop, ctl->net->bin );		// query.c
	}

	// are we relaying?
	if( ctl->relay->dcount )
		thread_throw( relay_loop, NULL );

	while( ctl->run_flags & RUN_LOOP )
	{
		// get an accurate time
		get_time( );

		// and sleep a bit
		usleep( ctl->sync->tick_usec );
	}
}

