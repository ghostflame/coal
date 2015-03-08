#ifndef COAL_SYNC_H
#define COAL_SYNC_H


#define DEFAULT_SYNC_INTERVAL			5.0
#define DEFAULT_MAKE_INTERVAL			20.0
#define DEFAULT_MAX_SYNC_THREADS		3
#define DEFAULT_MAIN_TICK_USEC			10000

#define SYNC_NODES_BRANCH               1
#define SYNC_NODES_LEAF                 2


struct sync_control
{
	double				sync_sec;
	double				make_sec;
	unsigned long		tick_usec;
	unsigned long		max_threads;
	unsigned long		curr_threads;
	unsigned long		sync_id;
};


void sync_single_node( NODE *n );

int sync_config_line( AVP *av );
SYNC_CTL *sync_config_defaults( void );

throw_fn sync_loop;

#endif
