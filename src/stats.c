#include "coal.h"

#define LLFID LLFST

void stats_run( void )
{
	// grab the memory stats
	memcpy( &(ctl->stats->data->mem), ctl->mem, sizeof( MEM_CTL ) );


}



void *stats_loop( void *arg )
{
	THRD *t = (THRD *) arg;
	double next;

	next = ctl->curr_time + ctl->stats->interval;

	while( ctl->run_flags & RUN_LOOP )
	{
		if( ctl->curr_time > next )
		{
			next += ctl->stats->interval;
			stats_run( );
		}
	}

	free( t );
	return NULL;
}


int stats_config_line( AVP *av )
{
	if( attIs( "enable" ) )
	{
		ctl->stats->enabled = atoi( av->val );
	}
	else
		return -1;

	return 0;
}



STAT_CTL *stats_config_defaults( void )
{
	STAT_CTL *s;

	s           = (STAT_CTL *) allocz( sizeof( STAT_CTL ) );
	s->data     = (STATS *) allocz( sizeof( STATS ) );
	s->interval = DEFAULT_STATS_INTERVAL;
	// off by default
	s->enabled  = 0;

	return s;
}

#undef LLFID

