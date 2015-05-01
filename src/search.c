#include "coal.h"

#define LLFID LLFSE

int search_data( QUERY *q )
{
	C3HDL *h;
	NODE *n;

	n = q->node;

	// force flush the node
	if( n->waiting )
		sync_single_node( n );

	// then open and read
	h = c3db_open( n->dir_path, C3DB_RO );
	if( c3db_status( h ) )
	{
		qwarn( 0x0801, "Could not open node %s -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		return -1;
	}

	// impose our config
	if( c3db_read( h, q->start, q->end, q->rtype, q->results ) )
	{
		qwarn( 0x0802, "Unable to read node %s -- %s",
			n->dir_path, c3db_error( h ) );
		c3db_close( h );
		return -2;
	}

	// and done
	c3db_close( h );
	return 0;
}

// how to do this efficiently?
// currently it's a search across nodes
int search_nodes( QUERY *q )
{
	NLIST *nl = NULL;
	PCACHE *pc;
	int i;

	// safe without locks
	for( i = 0; i < ctl->node->pcache_sz; i++ )
		for( pc = ctl->node->pcache[i]; pc; pc = pc->next )
		{
			// match?
			if( regexec( &(q->search), pc->path, 0, NULL, 0 ) )
				continue;

			// we need to do a relay search
			if( pc->relay )
			{
				// come back to this
				continue;
			}

			// capture the node in a list
			node_add_to_list( &nl, pc->target.node );
		}

	// and extract that list
	q->nodes = node_flatten_list( nl, (int *) &(q->rcount) );

	return 0;
}


// just nip through the children
int search_tree( QUERY *q )
{
	NLIST *nl = NULL;
	NODE *nc;

	// can't do this in two passes - not thread safe
	for( nc = q->node; nc; nc = nc->next )
		node_add_to_list( &nl, nc );

	q->nodes = node_flatten_list( nl, (int *) &(q->rcount) );

	return 0;
}




#undef LLFID

