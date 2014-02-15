#include "coal.h"


void *query_connection( void *arg )
{
  	THRD *t = (THRD *) arg;





	free( t );
	return NULL;
}



void *query_loop( void *arg )
{
	NET_TYPE_CTL *ntc;
	struct pollfd p;
	PORT_CTL *pc;
	HOST *h;
	THRD *t;
	int rv;

	t   = (THRD *) arg;
	ntc = (NET_TYPE_CTL *) t->arg;
	pc  = ntc->query;

	p.fd     = pc->sock;
	p.events = POLL_EVENTS;

	while( ctl->run_flags & RUN_LOOP )
	{
	  	// this value is all about responsiveness to shutdown
		if( ( rv = poll( &p, 1, 100 ) ) < 0 )
		{
		  	if( errno != EINTR )
			{
				err( "Poll error on query socket -- %s", Err );
				loop_end( "polling error on query hosts" );
				break;
			}

			continue;
		}
		else if( !rv )
			continue;

		if( p.revents & POLL_EVENTS )
		{
			if( ( h = net_get_host( p.fd, ntc->type ) ) )
				throw_thread( query_connection, h );
		}
	}

	// TODO
	// say we are shut down


	free( t );
	return NULL;
}


