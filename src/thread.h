#ifndef COAL_THREAD_H
#define COAL_THREAD_H


#define NODE_MUTEX_COUNT	0x40	// 64
#define NODE_MUTEX_MASK		0x3f



struct lock_control
{
	pthread_mutex_t			host;					// mem hosts
	pthread_mutex_t			auth;					// mem auth
	pthread_mutex_t			path;
	pthread_mutex_t			cache;					// pcache
	pthread_mutex_t			query;					// query backlog
	pthread_mutex_t			data;					// data backlog
	pthread_mutex_t			field;
	pthread_mutex_t			point;
	pthread_mutex_t			config;
	pthread_mutex_t			sync;					// sync control
	pthread_mutex_t			node[NODE_MUTEX_COUNT];	// node incoming

	int						init_done;
};



struct thread_data
{
	THRD				*	next;
	pthread_t				id;
	void				*	arg;
};



void thread_throw( void *(*fp) (void *), void *arg );

LOCK_CTL *lock_config_defaults( void );
void lock_shutdown( void );


#endif
