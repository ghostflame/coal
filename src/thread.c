#include "coal.h"

pthread_attr_t *tt_attr = NULL;

void thread_throw_init_attr( void )
{
	tt_attr = (pthread_attr_t *) allocz( sizeof( pthread_attr_t ) );
	pthread_attr_init( tt_attr );

	if( pthread_attr_setdetachstate( tt_attr, PTHREAD_CREATE_DETACHED ) )
		err( "Cannot set default attr state to detached -- %s", Err );
}


void thread_throw( void *(*fp) (void *), void *arg )
{
	THRD *t;

	if( !tt_attr )
		thread_throw_init_attr( );

	t = (THRD *) allocz( sizeof( THRD ) );
	t->arg = arg;
	pthread_create( &(t->id), tt_attr, fp, t );
}


LOCK_CTL *lock_config_defaults( void )
{
	LOCK_CTL *l;
	int i;

	l = (LOCK_CTL *) allocz( sizeof( LOCK_CTL ) );

	// and init all the mutexes

	// used in mem.c
	pthread_mutex_init( &(l->host), NULL );
	pthread_mutex_init( &(l->path), NULL );
	pthread_mutex_init( &(l->point), NULL );

	// used in data.c
	pthread_mutex_init( &(l->data), NULL );

	// used in query.c
	pthread_mutex_init( &(l->query), NULL );

	// used in sync.c
	pthread_mutex_init( &(l->field), NULL );

	// used in loop control
	pthread_mutex_init( &(l->loop), NULL );

	// used to lock nodes
	for( i = 0; i < NODE_MUTEX_COUNT; i++ )
		pthread_mutex_init( l->node + i, NULL );

	l->init_done = 1;
	return l;
}

void lock_shutdown( void )
{
	LOCK_CTL *l = ctl->locks;
	int i;

	if( !l || !l->init_done )
		return;

	// used in mem.c
	pthread_mutex_destroy( &(l->host) );
	pthread_mutex_destroy( &(l->path) );
	pthread_mutex_destroy( &(l->point) );

	// used in data.c
	pthread_mutex_destroy( &(l->data) );

	// used in query.c
	pthread_mutex_destroy( &(l->query) );

	// used in sync.c
	pthread_mutex_destroy( &(l->field) );

	// used in loop control
	pthread_mutex_destroy( &(l->loop) );

	// used to lock nodes
	for( i = 0; i < NODE_MUTEX_COUNT; i++ )
		pthread_mutex_destroy( l->node + i );

	l->init_done = 0;
}


