#ifndef COAL_CONFIG_H
#define COAL_CONFIG_H

#define DEFAULT_BASE_DIR		"/opt/coal"

#define DEFAULT_CONFIG_FILE		"conf/coal.conf"


// used all over - so all the config line fns have an AVP called 'av'
#define attIs( s )		!strcasecmp( av->att, s )
#define valIs( s )		!strcasecmp( av->val, s )



// main config structure
struct coal_control
{
	LOG_CTL			*	log;
	NET_CTL			*	net;
	NODE_CTL		*	node;
	MEM_CTL			*	mem;
	LOCK_CTL		*	locks;
	SYNC_CTL		*	sync;

	// where as-yet-unclassified data goes
	POINT			*	data_in;

	unsigned int		run_flags;

	// timing
	double				start_time;
	double				curr_time;
	int					main_tick_usec;

	char			*	pidfile;
	char			*	basedir;
	char			*	cfg_file;
};




struct config_context
{
  	CCTXT				*	next;
	CCTXT				*	children;
	CCTXT				*	parent;

	char					file[512];
	char					section[512];
	int						lineno;
};


// FUNCTIONS

int get_config( char *path );
COAL_CTL *create_config( void );


#endif
