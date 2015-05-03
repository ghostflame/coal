#ifndef COAL_NODE_H
#define COAL_NODE_H

#define NODE_PCACHE_SZ_SMALL		4001
#define NODE_PCACHE_SZ_MEDIUM		40009
#define NODE_PCACHE_SZ_LARGE		400009
#define NODE_PCACHE_SZ_HUGE			4000037

#define DEFAULT_NODE_PCACHE_SZ		NODE_PCACHE_SZ_MEDIUM
#define DEFAULT_NODE_ROOT			"nodes"

#define NODE_ROUTE_MAX_PATTERNS		32

#define NODE_FLAG_LEAF				0x01
#define NODE_FLAG_WRITING			0x02
#define NODE_FLAG_ERROR				0x04
#define NODE_FLAG_CREATING			0x10
#define NODE_FLAG_CREATED			0x20
#define NODE_FLAG_RELAY				0x40





struct path_cache
{
	PCACHE			*	next;
	char			*	path;		// path string, pre-parsing
	union {
		NODE			*	node;		// relevant node
		RDEST			*	dest;		// relevant destination
	}					target;
#ifdef CKSUM_64BIT
	uint64_t			sum;		// hash value, un-modulo'd
#else
	uint32_t			sum;		// hash value, un-modulo'd
#endif
	short				len;		// string length
	short				relay;		// is this a relay path?
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


struct node_routing
{
	NODE_ROUTE		*	next;

	// the name of this retain string
	char			*	name;

	// what we match
	char			*	patterns[NODE_ROUTE_MAX_PATTERNS];
	regex_t			*	rgx[NODE_ROUTE_MAX_PATTERNS];

	// retain strings are handled by libc3db
	char			*	retain;

	// relay target
	char			*	relay;
	RDEST			*	dest;

	int					pat_count;
	int					ret_len;
	int					rel_len;

	int					id;
};


struct node_list
{
	NLIST			*	next;
	NODE			**	nodes;
	int					sz;
	int					used;
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

	NODE_ROUTE		*	policy;

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

	// routing policies
	NODE_ROUTE		*	policies;
};






// node locking is based on id to spread the locks out
#define node_lock( _n )			pthread_mutex_lock(   &(ctl->locks->node[(_n)->id % NODE_MUTEX_MASK]) )
#define node_unlock( _n )		pthread_mutex_unlock( &(ctl->locks->node[(_n)->id % NODE_MUTEX_MASK]) )



#define node_add_flag( _n, _f )	node_lock( _n ); n->flags |=  _f; node_unlock( _n )
#define node_rmv_flag( _n, _f )	node_lock( _n ); n->flags &= ~_f; node_unlock( _n )


#define node_leaf_int( _n )		( ( (_n)->flags & NODE_FLAG_LEAF ) ? 1 : 0 )
#define node_leaf_str( _n )		( ( (_n)->flags & NODE_FLAG_LEAF ) ? "leaf" : "branch" )


// for making lists of nodes, used by query
void node_add_to_list( NLIST **list, NODE *n );
NODE **node_flatten_list( NLIST *list, int *ncount );

// calculate the hash value for a path
uint32_t node_path_cksum( char *str, int len );

// find something in the pcache
PCACHE *node_find_pcache( PATH *p );

// add something to the path cache
void node_add_pcache( PATH *p, NODE *n, RDEST *d );

// break up a path into a words structure
int node_path_parse( PATH *p );

// track down a node
NODE *node_find( PATH *p );

// create a node
NODE *node_create( char *name, int len, NODE *parent, PATH *p, int leaf );


// called to actually sync nodes to disk
throw_fn node_maker;


void node_add_point( NODE *n, POINT *p );
int node_start_discovery( void );


NODE_CTL *node_config_defaults( void );
int node_config_line( AVP *av );
int node_routing_line( AVP *av );

void node_sort_retentions( void );

#endif
