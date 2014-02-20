#ifndef COAL_STATS_H
#define COAL_STATS_H

#define DEFAULT_STATS_INTERVAL		10.0

struct data_stats_data
{
	unsigned long			conns;
	unsigned long			points;
};

struct query_stats_data
{
	unsigned long			conns;
	unsigned long			queries;
};


struct net_type_stats
{
	DSTATS					data;
	QSTATS					query;
};


struct stats_data
{
	NTSTATS					line;
	NTSTATS					bin;
	MEM_CTL					mem;
};

struct stat_control
{
	STATS				*	data;
	double					interval;
	int						enabled;
};


int stats_config_line( AVP *av );
STAT_CTL *stats_config_defaults( void );
throw_fn stats_loop;

#endif
