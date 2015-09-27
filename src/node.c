#include "coal.h"

#define LLFID LLFNO



int node_path_parse( PATH *p )
{
	// make a copy for strwords to eat
	p->copy = p->str + p->len + 1;
	memcpy( p->copy, p->str, p->len );

	// have to cap it, this might not be virgin memory
	*(p->copy + p->len) = '\0';

	if( strwords( p->w, p->copy, p->len, PATH_SEPARATOR ) < 0 )
	{
		nwarn( 0x0b01, "Could not parse path '%s'", p->str );
		p->copy = NULL;

		return -1;
	}

	return p->w->wc;
}


void node_add_pcache( PATH *p, NODE *n, RDEST *d )
{
	uint32_t hval;
	PCACHE *pc;

	pc       = (PCACHE *) allocz( sizeof( PCACHE ) );
	pc->len  = p->len;
	pc->path = str_dup( p->str, pc->len );
	pc->sum  = node_path_cksum( pc->path, pc->len );

	if( n )
		pc->target.node = n;
	else
	{
		pc->target.dest = d;
		pc->relay       = 1;
	}

	hval = pc->sum % ctl->node->pcache_sz;

	pthread_mutex_lock( &(ctl->locks->cache) );

	pc->next = ctl->node->pcache[hval];
	ctl->node->pcache[hval] = pc;

	if( n )
	{
		ndebug( 0x0e01, "Node %u added to path cache at position %u",
			n->id, hval );
	}

	pthread_mutex_unlock( &(ctl->locks->cache) );
}




void node_add_to_list( NLIST **list, NODE *n )
{
	NLIST *nl;

	if( !list )
		return;

	nl = *list;

	if( !nl || ( nl->used == nl->sz ) )
	{
		nl        = (NLIST *) allocz( sizeof( NLIST ) );
		nl->nodes = (NODE **) allocz( 1024 * sizeof( NODE * ) );
		nl->sz    = 1024;
		nl->used  = 0;

		nl->next = *list;
		*list    = nl;
	}

	nl->nodes[nl->used++] = n;
}


NODE **node_flatten_list( NLIST *list, int *ncount )
{
	int count, j;
	NLIST *nl;
	NODE **nf;

	for( count = 0, nl = list; nl; nl = nl->next )
		count += nl->used;

	nf = (NODE **) allocz( count * sizeof( NODE * ) );
	j  = 0;

	while( list )
	{
		nl   = list;
		list = nl->next;

		// copy the pointers
		memcpy( nf + j, nl->nodes, nl->used * sizeof( NODE * ) );
		j += nl->used;

		free( nl->nodes );
		free( nl );
	}

	if( ncount )
		*ncount = count;

	return nf;
}




NODE_ROUTE *node_policy( char *str )
{
	NODE_ROUTE *nr;
	int i;

	// cycle through the routing blocks
	for( nr = ctl->node->policies; nr; nr = nr->next )
		for( i = 0; i < nr->pat_count; i++ )
			if( !regexec( nr->rgx[i], str, 0, NULL, 0 ) )
				return nr;

	return NULL;
}


NODE *__node_find( PATH *p, int idx, NODE *n )
{
	NODE *cn;
	char *s;
	int l;

	// are we there yet?
	if( idx == p->w->wc )
		return n;

	s = p->w->wd[idx];
	l = p->w->len[idx];

	for( cn = n->children; cn; cn = cn->next )
		if( cn->name_len == l && !memcmp( cn->name, s, l ) )
			return __node_find( p, idx + 1, cn );

	return NULL;
}



static uint32_t node_cksum_primes[8] =
{
	2909, 3001, 3083, 3187, 3259, 3343, 3517, 3581
};


uint32_t node_path_cksum( char *str, int len )
{
#ifdef CKSUM_64BIT
	register uint64_t *p, sum = 0xbeef;
#else
	register uint32_t *p, sum = 0xbeef;
#endif
	int rem;

#ifdef CKSUM_64BIT
	rem = len & 0x7;
	len = len >> 3;

	for( p = (uint64_t *) str; len > 0; len-- )
#else
	rem = len & 0x3;
	len = len >> 2;

	for( p = (uint32_t *) str; len > 0; len-- )
#endif

		sum ^= *p++;

	// and capture the rest
	str = (char *) p;
	while( rem-- > 0 )
		sum += *str++ * node_cksum_primes[rem];

	return sum;
}



PCACHE *node_find_pcache( PATH *p )
{
	register PCACHE *pc;
	uint32_t sum, hval;

	sum  = node_path_cksum( p->str, p->len );
	hval = sum % ctl->node->pcache_sz;

	for( pc = ctl->node->pcache[hval]; pc; pc = pc->next )
		if( pc->sum == sum
		 && pc->len == p->len
		 && !memcmp( pc->path, p->str, p->len ) )
			return pc;

	return NULL;
}



// fnid b
NODE *node_find( PATH *p )
{
	PCACHE *pc;

	if( !p )
	{
		nwarn( 0x0b01, "No path passed into node_find." );
		return NULL;
	}

	// special name for the root
	if( p->len == 1 && *(p->str) == '.' )
		return ctl->node->nodes;

	// look in the cache
	if( ( pc = node_find_pcache( p ) ) )
	{
		if( pc->relay )
			return NULL;
		return pc->target.node;
	}

	// make sure we've broken it up
	if( p->w->wc <= 0
	 && node_path_parse( p ) <= 0 )
		return NULL;

	// recurse down the tree
	return __node_find( p, 0, ctl->node->nodes );
}




NODE *node_create( char *name, int len, NODE *parent, PATH *p, int leaf )
{
	NODE *n, *pn;
	double at;

	n            = (NODE *) allocz( sizeof( NODE ) );
	n->id        = ctl->node->node_id++;
	n->name      = str_dup( name, len );
	n->name_len  = len;
	n->dpath_len = len;
	n->parent    = parent;

	if( parent )
	{
		n->next          = parent->children;
		parent->children = n;
		n->dpath_len    += 1 + parent->dpath_len;
	}

	// may need a policy
	if( leaf )
	{
	  	// these are checked against the original string
		if( !( n->policy = node_policy( p->str ) ) )
		{
			nerr( 0x0101, "Cannot pick a policy for node '%s'", p->str );
			n->flags |= NODE_FLAG_ERROR;
		}
		else
		  	ndebug( 0x0102, "Node '%s' uses policy %s", p->str, n->policy->name );

		// is this a relay node?
		if( n->policy->relay )
		{
		 	n->dir_path = perm_str( n->dpath_len + 1 );

			if( parent )
			  	snprintf( n->dir_path, n->dpath_len + 1, "%s/%s", parent->dir_path, name );
			else
				snprintf( n->dir_path, n->dpath_len + 1, "%s", name );

			n->flags |= NODE_FLAG_RELAY;

			// create that as a relay cache
			node_add_pcache( p, NULL, n->policy->dest );

			ninfo( 0x0103, "New relay node %u '%s', child of node %u",
				n->id, n->dir_path, parent->id );
		}
		else
		{
			// databases get the extension
		  	n->dpath_len += 1 + C3DB_FILE_EXTN_LEN;
			n->dir_path   = perm_str( n->dpath_len + 1 );

			if( parent )
				snprintf( n->dir_path, n->dpath_len + 1, "%s/%s.%s", parent->dir_path, name, C3DB_FILE_EXTN );
			else
				snprintf( n->dir_path, n->dpath_len + 1, "%s.%s", name, C3DB_FILE_EXTN );

			// create that as a node cache
			node_add_pcache( p, n, NULL );

			ninfo( 0x0104, "New leaf node %u '%s', child of node %u",
				n->id, n->dir_path, parent->id );
		}

		n->flags |= NODE_FLAG_LEAF;
		ctl->node->count++;
	}
	else
	{
	 	n->dir_path = perm_str( n->dpath_len + 1 );
		if( parent )
		{
		  	snprintf( n->dir_path, n->dpath_len + 1, "%s/%s", parent->dir_path, name );
			ninfo( 0x0105, "New branch node %u '%s', child of node %u",
				n->id, n->dir_path, parent->id );
		}
		else
		{
	 		snprintf( n->dir_path, n->dpath_len + 1, "%s", name );
			ninfo( 0x0106, "Created root node '%s'", n->dir_path );
		}
	}

	// update the timestamps
	at = ctl->curr_time;
	for( pn = n; pn; pn = pn->parent )
		pn->updated = at;

	// and the counts
	if( leaf )
		++(ctl->node->leaves);
	else
		++(ctl->node->branches);

	return n;
}




void node_add_point( NODE *n, POINT *p )
{
  	// don't need this any more
	mem_free_path( &(p->path) );

	node_lock( n );

	p->next = n->incoming;
	n->incoming = p;
	p->handled = 1;
	++(n->waiting);

	node_unlock( n );
}



int create_database( NODE *n )
{
	C3HDL *h;

	// always create on latest version
	h = c3db_create( n->dir_path, 0, n->policy->retain );

	if( c3db_status( h ) != C3E_SUCCESS )
	{
		nerr( 0x0201, "Could not create database '%s' -- %s",
			n->dir_path, c3db_error( h ) );
		ndebug( 0x0202, "Path '%s', retention '%s'", n->dir_path, n->policy->retain );
		n->flags |= NODE_FLAG_ERROR;
		c3db_close( h );
		return -1;
	}

	c3db_close( h );

	node_add_flag( n, NODE_FLAG_CREATED );

	return 0;
}



int check_database( NODE *n )
{
	C3HDL *h;

	h = c3db_open( n->dir_path, C3DB_RO );

	if( c3db_status( h ) != C3E_SUCCESS )
	{
		nerr( 0x0301, "Could not open database '%s' -- %s",
			n->dir_path, c3db_error( h ) );
		n->flags |= NODE_FLAG_ERROR;
		c3db_close( h );
		return -1;
	}

	c3db_close( h );

	node_add_flag( n, NODE_FLAG_CREATED );

	return 0;
}



int create_directory( NODE *n )
{
	if( mkdir( n->dir_path, 0755 )
	 && errno != EEXIST )
	{
		nerr( 0x0401, "Failed to create directory '%s' -- %s",
			n->dir_path, Err );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}

    ndebug( 0x0402, "Created node directory '%s'", n->dir_path );
	node_add_flag( n, NODE_FLAG_CREATED );

	return 0;
}


int node_path_check( NODE *n, int leaf, struct stat *sb )
{
	C3HDL *h;

	if( !leaf )
	{
		// dir check
		if( !S_ISDIR( sb->st_mode ) )
		{
			nerr( 0x0501, "Path %s exists but is not a directory!",
				n->dir_path );
			n->flags |= NODE_FLAG_ERROR;
			return -1;
		}

		ninfo( 0x0502, "Found directory %s", n->dir_path );
		node_add_flag( n, NODE_FLAG_CREATED );
		return 0;
	}

	// file check
	if( !S_ISREG( sb->st_mode ) )
	{
		nerr( 0x0503, "Path %s exists but is not a regular file!",
			n->dir_path );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}

	node_add_flag( n, NODE_FLAG_CREATED );

	h = c3db_open( n->dir_path, C3DB_RO );
	if( c3db_status( h ) != 0 )
	{
		nerr( 0x0504, "Cannot open node %s as a C3DB -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}
	ninfo( 0x0505, "Found database %s.", n->dir_path );
	c3db_close( h );

	return 0;
}



int node_write_single( NODE *n )
{
	struct stat sb;
	int leaf, ret;

    ndebug( 0x0601, "Attempting node_write_single on '%s'", n->dir_path );

	// we saw no created flag, so test properly this time
	node_lock( n );
	if( n->flags & (NODE_FLAG_CREATED|NODE_FLAG_CREATING|NODE_FLAG_ERROR) )
	{
        ndebug( 0x0602, "Aborting node_write_single on '%s' because of flags.",
            n->dir_path );
		node_unlock( n );
		return 0;
	}

	// mark this as ours
	n->flags |= NODE_FLAG_CREATING;
	node_unlock( n );

	leaf = ( n->flags & NODE_FLAG_LEAF ) ? 1 : 0;
	ret = 0;

	if( stat( n->dir_path, &sb ) == 0 )
	{
		if( node_path_check( n, leaf, &sb ) != 0 )
			ret = -1;
	}
	else if( errno != ENOENT )
	{
		nerr( 0x0603, "Cannot stat path '%s' -- %s",
			n->dir_path, Err );
		n->flags |= NODE_FLAG_ERROR;
		ret = -2;
	}
	else
	{
		// create it then!
		if( leaf )
		{
			if( create_database( n ) != 0 )
				ret = -3;
		}
		else
		{
			if( create_directory( n ) != 0 )
				ret = -4;
		}
	}

	node_rmv_flag( n, NODE_FLAG_CREATING );
	return ret;
}



void node_write( NODE *n )
{
	NODE *ch;

	// relay flag lives on relay leaf nodes - we're done here
	if( n->flags & NODE_FLAG_RELAY )
		return;

	if( n->parent
	 && !( n->parent->flags & NODE_FLAG_CREATED ) )
		// can't do this one yet
		// stops a new thread running 'ahead' of a
		// working thread
		return;

	// is this one done?
	if( !( n->flags & NODE_FLAG_CREATED ) )
		node_write_single( n );

	// move on - depth first
	for( ch = n->children; ch; ch = ch->next )
		node_write( ch );
}


void *node_maker( void *arg )
{
	THRD *t = (THRD *) arg;

	node_write( ctl->node->nodes );

	free( t );
	return NULL;
}



int node_read_dir( NODE *dn, regex_t *c3chk )
{
	int leaf, ret, len;
	struct dirent *d;
	char buf[2048];
	PATH path;
	NODE *n;
	DIR *dh;

	if( !( dh = opendir( dn->dir_path ) ) )
	{
		nerr( 0x0701, "Failed to open dir %s -- %s",
			dn->dir_path, Err );
		return -1;
	}

	memset( &path, 0, sizeof( PATH ) );
	path.str = buf;
	path.len = snprintf( buf, 2048, "%s/", dn->dir_path );
	ret = 0;

	while( ( d = readdir( dh ) ) )
	{
		// avoid static dirs and hidden files
		if( d->d_name[0] == '.' )
			continue;

		// tie ourselves to linux dirent.d_type for now

		switch( d->d_type )
		{
			case DT_DIR:
				len  = strlen( d->d_name );
				leaf = 0;
				break;

			case DT_REG:
				// we only care about c3db files here
				if( regexec( c3chk, d->d_name, 0, NULL, 0 ) )
					continue;

				// adjust the length - we don't want .c3db in the node name
				len  = strlen( d->d_name ) - 1 - C3DB_FILE_EXTN_LEN;
				leaf = 1;

				// stamp on the . in the name
				d->d_name[len] = '\0';
				break;

			default:
				// never mind other types
				continue;
		}

		// set up the path and make a node
		path.len += snprintf( buf + path.len, 2048 - path.len, "%s", d->d_name );
		n = node_create( d->d_name, len, dn, &path, leaf );

		// recurse in on directories
		if( !leaf )
			ret += node_read_dir( n, c3chk );
	}

	closedir( dh );

	return ret;
}


// they are backwards
void node_sort_routings( void )
{
	NODE_ROUTE *list, *nr, *nr_next;

	for( list = NULL, nr = ctl->node->policies; nr; nr = nr_next )
	{
		nr_next  = nr->next;
		nr->next = list;
		list     = nr;
	}

	ctl->node->policies = list;

	for( nr = list; nr; nr = nr->next )
		debug( 0x0801, "Routing policy order: %s", nr->name );
}


int node_start_discovery( void )
{
	char buf[16], ebuf[128];
	regex_t c3r;
	int rv;

	// nodes are backwards
	node_sort_routings( );

	ctl->node->pcache = (PCACHE **) allocz( ctl->node->pcache_sz * sizeof( PCACHE * ) );

	// read the directory
	// create node structures
	// do not return until finished

	ctl->node->nodes = node_create( ctl->node->root, ctl->node->root_len,
			NULL, NULL, 0 );
	// and make it, right now
	node_write( ctl->node->nodes );

	snprintf( buf, 16, "\\.%s$", C3DB_FILE_EXTN );
	if( ( rv = regcomp( &c3r, buf, REG_NOSUB ) ) )
	{
		regerror( rv, &c3r, ebuf, 128 );
		err( 0x0901, "Could not compile c3db file matching regex -- %s", ebuf );
		return -1;
	}

	// and recurse down the tree
	return node_read_dir( ctl->node->nodes, &c3r );
}


NODE_CTL *node_config_defaults( void )
{
	NODE_CTL *n;

	n = (NODE_CTL *) allocz( sizeof( NODE_CTL ) );
	n->pcache_sz = DEFAULT_NODE_PCACHE_SZ;

	n->root      = strdup( DEFAULT_NODE_ROOT );
	n->root_len  = strlen( n->root );

	return n;
}


int node_config_line( AVP *av )
{
	NODE_CTL *n = ctl->node;

  	if( attIs( "cache_size" ) )
		n->pcache_sz = (unsigned int) strtoul( av->val, NULL, 10 );
	else if( attIs( "cache_hint" ) )
	{
		if( !strcasecmp( av->val, "small" ) )
			n->pcache_sz = NODE_PCACHE_SZ_SMALL;
		else if( !strcasecmp( av->val, "medium" ) )
		  	n->pcache_sz = NODE_PCACHE_SZ_MEDIUM;
		else if( !strcasecmp( av->val, "large" ) )
		  	n->pcache_sz = NODE_PCACHE_SZ_LARGE;
		else if( !strcasecmp( av->val, "huge" ) )
		  	n->pcache_sz = NODE_PCACHE_SZ_HUGE;
		else
			return -1;
	}
	else if( attIs( "rootdir" ) )
	{
		free( n->root );
		n->root     = config_relative_path( av->val );
		n->root_len = strlen( n->root );
	}
	else
		return -1;

	return 0;
}



static NODE_ROUTE *nr_cfg_curr = NULL;
static int nr_cfg_count = 0;

int node_routing_line( AVP *av )
{
  	NODE_ROUTE *r;
    regex_t retrgx;

	if( !nr_cfg_curr )
	{
		nr_cfg_curr = (NODE_ROUTE *) allocz( sizeof( NODE_ROUTE ) );
		nr_cfg_curr->id = nr_cfg_count++;
	}

	r = nr_cfg_curr;

	if( attIs( "name" ) )
	{
		if( r->name )
		{
			warn( 0x0a01, "Node routing group '%s' also contains name '%s' - is a 'done' missing?",
				r->name, av->val );
			return -1;
		}

		r->name = str_dup( av->val, av->vlen );

		debug( 0x0a02, "Routing policy '%s' config started.", r->name );
	}
	else if( attIs( "pattern" ) )
	{
		if( r->pat_count == NODE_ROUTE_MAX_PATTERNS )
		{
			warn( 0x0a03, "Node routing group '%s' already has the max %d regex patterns.",
				r->name, NODE_ROUTE_MAX_PATTERNS );
			return -1;
		}

		r->patterns[r->pat_count] = str_dup( av->val, av->vlen );
		r->rgx[r->pat_count]      = (regex_t *) allocz( sizeof( regex_t ) );

		if( regcomp( r->rgx[r->pat_count], r->patterns[r->pat_count],
				REG_NOSUB|REG_ICASE|REG_EXTENDED ) )
		{
			warn( 0x0a04, "Invalid regex '%s'", r->patterns[r->pat_count] );
			regfree( r->rgx[r->pat_count] );
			// yes we leak the str_dup - I don't care, we're about to bomb
			// out on dodgy config
			return -1;
		}

		debug( 0x0a05, "Routing policy %s gains regex %s",
			( r->name ) ? r->name : "as yet unnamed", av->val );

		// and note that we have another one
		r->pat_count++;
	}
	else if( attIs( "retain" ) )
	{
		if( r->retain )
		{
			warn( 0x0a06, "Node routing group '%s' also contains retention '%s' - is a 'done' missing?",
				r->name, r->retain );
			return -1;
		}

		if( r->relay )
		{
			warn( 0x0a07, "Node routing group '%s' also contains relay '%s' - cannot retain AND relay!",
				r->name, r->relay );
			return -1;
		}

        // should match all retention strings
        if( regcomp( &retrgx, "^([0-9]+:[0-9]+[DdWwMmYy]?)(;[0-9]+:[0-9]+[DdWwMmYy]?)*$",
                REG_EXTENDED|REG_NOSUB ) )
        {
            err( 0x0a08, "Could not create retention regex." );
            return -1;
        }
        if( regexec( &retrgx, av->val, 0, NULL, 0 ) )
        {
            err( 0x0a09, "Invalid retention string '%s'", av->val );
            regfree( &retrgx );
            return -1;
        }
        regfree( &retrgx );

		r->retain  = str_dup( av->val, av->vlen );
		r->ret_len = av->vlen;

		debug( 0x0a0a, "Routing policy %s has retain: %s",
			( r->name ) ? r->name : "as yet unnamed", r->retain );
	}
	else if( attIs( "relay" ) )
	{
		if( r->relay )
		{
			warn( 0x0a0b, "Node routing group '%s' also contains relay '%s' - is a 'done' missing?",
				r->name, r->relay );
			return -1;
		}

		if( r->retain )
		{
			warn( 0x0a0c, "Node routing group '%s' also contains retention '%s' - cannot retain AND relay!",
				r->name, r->relay );
			return -1;
		}

		r->relay   = str_dup( av->val, av->vlen );
		r->rel_len = av->vlen;

		debug( 0x0a0d, "Routing policy %s has relay: %s",
			( r->name ) ? r->name : "as yet unnamed", r->relay );
	}
	else if( attIs( "done" ) )
	{
		if( !r->name )
		{
			warn( 0x0a0e, "Cannot process unnamed node data routing block." );
			return -1;
		}

		if( ( !r->retain && !r->relay ) || !r->pat_count )
		{
			warn( 0x0a0f, "Incomplete node data routing block '%s'", r->name );
			return -1;
		}

		// winner!
		r->next = ctl->node->policies;
		ctl->node->policies = r;

		debug( 0x0a10, "Routing policy %s created.", r->name );

		// and stamp on our static
		nr_cfg_curr = NULL;
	}

	return 0;
}

#undef LLFID

