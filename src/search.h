#ifndef COAL_SEARCH_H
#define COAL_SEARCH_H

// query a database
int search_data( QUERY *q );

// match nodes against a regex
int search_nodes( QUERY *q );

// search the tree for children
int search_tree( QUERY *q );


#endif
