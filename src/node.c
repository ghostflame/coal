#include "coal.h"


NODE_RET *node_policy( char *str )
{
	NODE_RET *nr;
	int i;

	// cycle through the retention blocks
	for( nr = ctl->node->policies; nr; nr = nr->next )
		for( i = 0; i < nr->pat_count; i++ )
			if( !regexec( nr->rgx[i], str, 0, NULL, 0 ) )
				return nr;

	return NULL;
}




NODE *node_create( char *name, int len, NODE *parent, PATH *p, int leaf )
{
	NODE *n, *pn;
	double at;
	int l;

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

	// databases get the extension
	if( leaf )
		n->dpath_len += 1 + C3DB_FILE_EXTN_LEN;

	// make a new string space for the dir path
	n->dir_path  = perm_str( n->dpath_len + 1 );
	l = 0;

	// prepend the parent path if there is one
	if( parent )
		l = snprintf( n->dir_path, n->dpath_len + 1, "%s/", parent->dir_path );

	if( leaf )
	{
		snprintf( n->dir_path + l, n->dpath_len + 1 - l, "%s.%s", name, C3DB_FILE_EXTN );

		n->flags |= NODE_FLAG_LEAF;
		ctl->node->count++;

		if( !( n->policy = node_policy( n->dir_path + ctl->node->root_len + 1 ) ) )
		{
			nerr( "Cannot pick a policy for node '%s'", n->dir_path );
			n->flags |= NODE_FLAG_ERROR;
		}
		else
			ndebug( "Node '%s' uses policy %s", n->dir_path, n->policy->name );

		// and create that
		data_add_path_cache( n, p );

		ninfo( "New leaf node %u '%s', child of node %u",
			n->id, n->dir_path, parent->id );
	}
	else
	{
	 	snprintf( n->dir_path + l, n->dpath_len + 1 - l, "%s", name );

		if( parent )
			ninfo( "New branch node %u '%s', child of node %u",
				n->id, n->dir_path, parent->id );
		else
			ninfo( "Created root node '%s'", n->dir_path );
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
		nerr( "Could not create database '%s' -- %s",
			n->dir_path, c3db_error( h ) );
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
		nerr( "Could not open database '%s' -- %s",
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
		nerr( "Failed to create directory '%s' -- %s",
			n->dir_path, Err );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}

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
			nerr( "Path %s exists but is not a directory!",
				n->dir_path );
			n->flags |= NODE_FLAG_ERROR;
			return -1;
		}

		ninfo( "Found directory %s", n->dir_path );
		node_add_flag( n, NODE_FLAG_CREATED );
		return 0;
	}

	// file check
	if( !S_ISREG( sb->st_mode ) )
	{
		nerr( "Path %s exists but is not a regular file!",
			n->dir_path );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}

	node_add_flag( n, NODE_FLAG_CREATED );

	h = c3db_open( n->dir_path, C3DB_RO );
	if( c3db_status( h ) != 0 )
	{
		nerr( "Cannot open node %s as a C3DB -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		n->flags |= NODE_FLAG_ERROR;
		return -1;
	}
	ninfo( "Found database %s.", n->dir_path );
	c3db_close( h );

	return 0;
}



int node_write_single( NODE *n )
{
	struct stat sb;
	int leaf, ret;

	// we saw no created flag, so test properly this time
	node_lock( n );
	if( n->flags & (NODE_FLAG_CREATED|NODE_FLAG_CREATING) )
	{
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
		nerr( "Cannot stat path '%s' -- %s",
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

	if( n->parent
	 && !( n->parent->flags & NODE_FLAG_CREATED ) )
		// can't do this one yet
		// stops a new thread running 'ahead' of a
		// working thread
		return;

	// is this one done?
	if( !( n->flags & NODE_FLAG_CREATED ) )
		node_write_single( n );

	// move on
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
		nerr( "Failed to open dir %s -- %s",
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




int node_start_discovery( void )
{
	char buf[16], ebuf[128];
	regex_t c3r;
	int rv;

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
		err( "Could not compile c3db file matching regex -- %s", ebuf );
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
		n->root     = strdup( av->val );
		n->root_len = av->vlen;
	}
	else
		return -1;

	return 0;
}



static NODE_RET *nr_cfg_curr = NULL;
static int nr_cfg_count = 0;

int node_retain_line( AVP *av )
{
  	NODE_RET *r;

	if( !nr_cfg_curr )
	{
		nr_cfg_curr = (NODE_RET *) allocz( sizeof( NODE_RET ) );
		nr_cfg_curr->id = nr_cfg_count++;
	}

	r = nr_cfg_curr;

	if( attIs( "name" ) )
	{
		if( r->name )
		{
			warn( "Node retention group '%s' also contains name '%s' - is a 'done' missing?",
				r->name, av->val );
			return -1;
		}

		r->name = str_dup( av->val, av->vlen );

		debug( "Retention policy %s config started.", r->name );
	}
	else if( attIs( "pattern" ) )
	{
		if( r->pat_count == NODE_RET_MAX_PATTERNS )
		{
			warn( "Node retention group '%s' already has the max %d regex patterns.",
				r->name, NODE_RET_MAX_PATTERNS );
			return -1;
		}

		r->patterns[r->pat_count] = str_dup( av->val, av->vlen );
		r->rgx[r->pat_count]      = (regex_t *) allocz( sizeof( regex_t ) );

		if( regcomp( r->rgx[r->pat_count], r->patterns[r->pat_count],
				REG_NOSUB|REG_ICASE|REG_EXTENDED ) )
		{
			warn( "Invalid regex '%s'", r->patterns[r->pat_count] );
			regfree( r->rgx[r->pat_count] );
			// yes we leak the str_dup - I don't care, we're about to bomb
			// out on dodgy config
			return -1;
		}

		debug( "Retention policy %s gains regex %s",
			( r->name ) ? r->name : "as yet unnamed", av->val );

		// and note that we have another one
		r->pat_count++;
	}
	else if( attIs( "retain" ) )
	{
		if( r->retain )
		{
			warn( "Node retention group '%s' also contains retention '%s' - is a 'done' missing?",
				r->name, av->val );
			return -1;
		}

		// TODO
		// sanity checking on retention strings

		r->retain  = str_dup( av->val, av->vlen );
		r->ret_len = av->vlen;

		debug( "Retention policy %s has retains: %s",
			( r->name ) ? r->name : "as yet unnamed", r->retain );
	}
	else if( attIs( "done" ) )
	{
		if( !r->name )
		{
			warn( "Cannot process unnamed node data retention block." );
			return -1;
		}

		if( !r->retain
		 || !r->pat_count )
		{
			warn( "Incomplete node data retention block '%s'", r->name );
			return -1;
		}

		// winner!
		r->next = ctl->node->policies;
		ctl->node->policies = r;

		debug( "Retention policy %s created.", r->name );

		// and stamp on our static
		nr_cfg_curr = NULL;
	}

	return 0;
}


// they are backwards
void node_sort_retentions( void )
{
	NODE_RET *list, *nr, *nr_next;

	for( list = NULL, nr = ctl->node->policies; nr; nr = nr_next )
	{
		nr_next  = nr->next;
		nr->next = list;
		list     = nr;
	}

	ctl->node->policies = list;
}
