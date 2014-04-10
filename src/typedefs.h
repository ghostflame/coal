#ifndef COAL_TYPEDEFS_H
#define COAL_TYPEDEFS_H

// all typedefs go here to prevent crazy ordering problems

// control structures
typedef struct coal_control			COAL_CTL;
typedef struct log_control			LOG_CTL;
typedef struct network_control		NET_CTL;
typedef struct port_control			PORT_CTL;
typedef struct net_type_control		NET_TYPE_CTL;
typedef struct memory_control 		MEM_CTL;
typedef struct node_control			NODE_CTL;
typedef struct lock_control			LOCK_CTL;
typedef struct sync_control			SYNC_CTL;
typedef struct stat_control			STAT_CTL;
typedef struct relay_control		REL_CTL;

// other structures
typedef struct config_context 		CCTXT;
typedef struct data_point 			POINT;
typedef struct log_file    			LOG_FILE;
typedef struct query_data			QUERY;
typedef struct net_buffer			NBUF;
typedef struct net_socket			NSOCK;
typedef struct host_data			HOST;
typedef struct node_routing			NODE_ROUTE;
typedef struct node_data			NODE;
typedef struct path_cache			PCACHE;
typedef struct path_data			PATH;
typedef struct thread_data			THRD;
typedef struct words_data			WORDS;
typedef struct av_pair				AVP;
typedef struct relay_destination	RDEST;
typedef struct relay_data_conn		RDCONN;
typedef struct relay_query_conn		RQCONN;

// stats structures
typedef struct data_stats_data		DSTATS;
typedef struct query_stats_data		QSTATS;
typedef struct net_type_stats		NTSTATS;
typedef struct stats_data			STATS;


// and function types
typedef void * throw_fn ( void * );

#endif
