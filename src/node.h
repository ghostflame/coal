#ifndef COAL_NODE_H
#define COAL_NODE_H

#define NODE_PCACHE_SZ_SMALL		4001
#define NODE_PCACHE_SZ_MEDIUM		40009
#define NODE_PCACHE_SZ_LARGE		400009
#define NODE_PCACHE_SZ_HUGE			4000037

#define DEFAULT_NODE_PCACHE_SZ		NODE_PCACHE_SZ_MEDIUM
#define DEFAULT_NODE_ROOT			"nodes"

#define NODE_RET_MAX_PATTERNS		32

#define NODE_FLAG_LEAF				0x01
#define NODE_FLAG_WRITING			0x02
#define NODE_FLAG_ERROR				0x04
#define NODE_FLAG_CREATING			0x10
#define NODE_FLAG_CREATED			0x20






struct path_cache
{
	PCACHE			*	next;
	char			*	path;		// path string, pre-parsing
	NODE			*	node;		// relevant node
#ifdef CKSUM_64BIT
	uint64_t			sum;		// hash value, un-modulo'd
#else
	uint32_t			sum;		// hash value, un-modulo'd
#endif
	int					len;		// string length
};


struct path_data
{
	PATH			*	next;		// used for recycling
	char			*	str;
	char			*	copy;		// eaten by strwords

	short				len;
	short				sz;
	int					curr;

	WORDS			*	w;
};


struct node_retain
{
	NODE_RET		*	next;

	// the name of this retain string
	char			*	name;

	// what we match
	char			*	patterns[NODE_RET_MAX_PATTERNS];
	regex_t			*	rgx[NODE_RET_MAX_PATTERNS];

	// retain strings are handled by libc3db
	char			*	retain;

	int					pat_count;
	int					ret_len;

	int					id;
};



struct node_data
{
	NODE			*	next;
	NODE			*	parent;
	NODE			*	children;

	char			*	dir_path;
	char			*	name;

	unsigned short		dpath_len;
	unsigned char		name_len;
	unsigned short		flags;

	unsigned int		id;

	// timestamp
	double				updated;

	NODE_RET		*	policy;

	// incoming data
	POINT			*	incoming;		// waiting to be written
	int					waiting;		// how many
};




struct node_control
{
	// the nodes
	NODE			*	nodes;
	unsigned int		count;

	char			*	root;
	unsigned int		root_len;

	// path cache
	PCACHE			**	pcache;
	unsigned int		pcache_sz;

	// incrementor
	unsigned int		node_id;
	unsigned int		leaves;
	unsigned int		branches;

	// retention policies
	NODE_RET		*	policies;
};






// node locking is based on id to spread the locks out
#define node_lock( _n )			pthread_mutex_lock(   &(ctl->locks->node[(_n)->id % NODE_MUTEX_MASK]) )
#define node_unlock( _n )		pthread_mutex_unlock( &(ctl->locks->node[(_n)->id % NODE_MUTEX_MASK]) )



#define node_add_flag( _n, _f )	node_lock( _n ); n->flags |=  _f; node_unlock( _n )
#define node_rmv_flag( _n, _f )	node_lock( _n ); n->flags &= ~_f; node_unlock( _n )



NODE *node_create( char *name, int len, NODE *parent, PATH *p, int leaf );


// called to actually sync nodes to disk
throw_fn node_maker;


void node_add_point( NODE *n, POINT *p );
int node_start_discovery( void );


NODE_CTL *node_config_defaults( void );
int node_config_line( AVP *av );
int node_retain_line( AVP *av );

void node_sort_retentions( void );

#endif
