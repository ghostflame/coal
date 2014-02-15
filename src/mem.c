#include "coal.h"



HOST *mem_new_host( void )
{
	HOST *h;

	pthread_mutex_lock( &(ctl->locks->host) );
	if( ctl->mem->hosts )
	{
		h = ctl->mem->hosts;
		ctl->mem->hosts = h->next;
		--(ctl->mem->free_hosts);
		pthread_mutex_unlock( &(ctl->locks->host) );

		h->next = NULL;
	}
	else
	{
		++(ctl->mem->mem_hosts);
		pthread_mutex_unlock( &(ctl->locks->host) );

		h = (HOST *) allocz( sizeof( HOST ) );
		// we need more than the max packet, in case we have
		// some data held over between packets
		// we also never free hosts, so we never give
		// this memory back
		h->inbuf  = perm_str( 2 * MAX_PKT_IN );
		h->outbuf = perm_str( MAX_PKT_OUT );
		h->all    = (WORDS *) allocz( sizeof( WORDS ) );
		h->val    = (WORDS *) allocz( sizeof( WORDS ) );
	}

	return h;
}


void mem_free_host( HOST **h )
{
	HOST *sh;

	if( !h || !*h )
		return;

	sh = *h;
	*h = NULL;

	sh->fd     = 0;
	sh->flags  = 0;
	sh->points = 0;

	sh->inlen  = 0;
	sh->outlen = 0;

	if( sh->name )
	{
	  	free( sh->name );
		sh->name = NULL;
	}

	pthread_mutex_lock( &(ctl->locks->host) );

	sh->next = ctl->mem->hosts;
	ctl->mem->hosts = sh;
	++(ctl->mem->free_hosts);

	pthread_mutex_unlock( &(ctl->locks->host) );
}


POINT *mem_new_point( void )
{
	POINT *p;

	pthread_mutex_lock( &(ctl->locks->point) );
	if( ctl->mem->points )
	{
		p = ctl->mem->points;
		ctl->mem->points = p->next;
		--(ctl->mem->free_points);
		pthread_mutex_unlock( &(ctl->locks->point) );

		p->next = NULL;
	}
	else
	{
		++(ctl->mem->mem_points);
		pthread_mutex_unlock( &(ctl->locks->point) );

		p = (POINT *) allocz( sizeof( POINT ) );
	}

	return p;
}

void mem_free_point( POINT **p )
{
	POINT *sp;

	if( !p || !*p )
		return;

	sp = *p;
	*p = NULL;

	if( sp->path )
		mem_free_path( &(sp->path) );

	memset( sp, 0, sizeof( POINT ) );

	pthread_mutex_lock( &(ctl->locks->point) );

	sp->next = ctl->mem->points;
	ctl->mem->points = sp;
	++(ctl->mem->free_points);

	pthread_mutex_unlock( &(ctl->locks->point) );
}

void mem_free_point_list( POINT *list )
{
	POINT *freed, *p, *end;
	PATH *plist;
	int j = 0;

	freed = end = NULL;
	plist = NULL;

	while( list )
	{
		p    = list;
		list = p->next;

		if( p->path )
		{
			p->path->next = plist;
			plist         = p->path;
		}

		memset( p, 0, sizeof( POINT ) );
		p->next = freed;
		freed   = p;

		if( !end )
			end = p;

		j++;
	}

	pthread_mutex_lock( &(ctl->locks->point) );

	end->next = ctl->mem->points;
	ctl->mem->points = freed;
	ctl->mem->free_points += j;

	pthread_mutex_unlock( &(ctl->locks->point) );

	if( plist )
		mem_free_path_list( plist );
}


PATH *mem_new_path( char *str, int len )
{
	PATH *p;

	pthread_mutex_lock( &(ctl->locks->path) );
	if( ctl->mem->paths )
	{
		p = ctl->mem->paths;
		ctl->mem->paths = p->next;
		--(ctl->mem->free_paths);
		pthread_mutex_unlock( &(ctl->locks->path) );

		p->next = NULL;
	}
	else
	{
	  	++(ctl->mem->mem_paths);
		pthread_mutex_unlock( &(ctl->locks->path) );

		p    = (PATH *) allocz( sizeof( PATH ) );
		p->w = (WORDS *) allocz( sizeof( WORDS ) );
	}

	// we use double the length because we may need two
	// copies, one for strwords to eat
	if( p->sz < ( 2 * len ) )
	{
		free( p->str );
		p->sz  = 2 * len;
		p->str = (char *) allocz( p->sz );
	}

	// copy the string into place
	p->len = len;
	memcpy( p->str, str, len );
	p->str[len] = '\0';

	return p;
}


void mem_free_path( PATH **p )
{
  	PATH *sp;

	if( !p || !*p )
		return;

	sp = *p;
	*p = NULL;

	*(sp->str) = '\0';
	sp->len    = 0;
	sp->curr   = 0;

	pthread_mutex_lock( &(ctl->locks->path) );

	sp->next = ctl->mem->paths;
	ctl->mem->paths = sp;
	++(ctl->mem->free_paths);

	pthread_mutex_unlock( &(ctl->locks->path) );
}


void mem_free_path_list( PATH *list )
{
	PATH *p, *end = list;
	int j;

	if( !list )
		return;

	for( j = 0, p = list; p; p = p->next, j++ )
	{
		*(p->str) = '\0';
		p->len    = 0;
		p->curr   = 0;

		end = p;
	}

	pthread_mutex_lock( &(ctl->locks->path) );

	end->next = ctl->mem->paths;
	ctl->mem->paths = list;
	ctl->mem->free_paths += j;

	pthread_mutex_unlock( &(ctl->locks->path) );
}



MEM_CTL *mem_config_defaults( void )
{
	// trivial
	return (MEM_CTL *) allocz( sizeof( MEM_CTL ) );
}


