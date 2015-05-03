#include "coal.h"

#define LLFID LLFME

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

		h      = (HOST *) allocz( sizeof( HOST ) );
		h->net = net_make_sock( COAL_NETBUF_SZ, COAL_NETBUF_SZ, NULL, &(h->peer) );
		h->all = (WORDS *) allocz( sizeof( WORDS ) );
		h->val = (WORDS *) allocz( sizeof( WORDS ) );
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

	sh->points        = 0;
	sh->net->sock     = -1;
	sh->net->flags    = 0;
	sh->net->out.len  = 0;
	sh->net->in.len   = 0;
	sh->net->keep.buf = NULL;
	sh->net->keep.len = 0;
	sh->last          = 0.0;
	sh->started       = 0.0;

	if( sh->net->name )
	{
	  	free( sh->net->name );
		sh->net->name = NULL;
	}

	pthread_mutex_lock( &(ctl->locks->host) );

	sh->next = ctl->mem->hosts;
	ctl->mem->hosts = sh;
	++(ctl->mem->free_hosts);

	pthread_mutex_unlock( &(ctl->locks->host) );
}


// this must always be called inside a lock
void __mem_new_points( int count )
{
    POINT *list, *p;
    int i;

    // allocate a block of points
    list = (POINT *) allocz( count * sizeof( POINT ) );

    p = list + 1;

    // link them up
    for( i = 0; i < count; i++ )
        list[i].next = p++;

    // and insert them
    list[count - 1].next = ctl->mem->points;
    ctl->mem->points  = list;
    ctl->mem->free_points += count;
    ctl->mem->mem_points  += count;
}



POINT *mem_new_point( void )
{
	POINT *p;

	pthread_mutex_lock( &(ctl->locks->point) );

    if( !ctl->mem->points )
        __mem_new_points( NEW_POINTS_BLOCK_SZ );

	p = ctl->mem->points;
	ctl->mem->points = p->next;
	--(ctl->mem->free_points);

	pthread_mutex_unlock( &(ctl->locks->point) );

	p->next = NULL;

	return p;
}

POINT *mem_new_points( int count )
{
	POINT *list, *p;
	int i;

	pthread_mutex_lock( &(ctl->locks->point) );

    // make sure we have enough free
    while( ctl->mem->free_points < count )
        __mem_new_points( NEW_POINTS_BLOCK_SZ );

	list = ctl->mem->points;
	for( p = list, i = 1; i < count; i++ )
		p = p->next;

	ctl->mem->points = p->next;
	p->next = NULL;

	ctl->mem->free_points -= count;
	pthread_mutex_unlock( &(ctl->locks->point) );

	return list;
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


// this must always be called from inside a lock
void __mem_new_paths( int count )
{
    WORDS *wlist, *w;
    PATH *plist, *p;
    int i;

    // allocate a block of paths and words
    plist = (PATH *)  allocz( count * sizeof( PATH ) );
    wlist = (WORDS *) allocz( count * sizeof( WORDS ) );

    p = plist + 1;
    w = wlist;

    // link them up
    for( i = 0; i < count; i++ )
    {
        plist[i].next = p++;
        plist[i].w    = w++;
    }

    // and insert them
    plist[count - 1].next = ctl->mem->paths;
    ctl->mem->paths = plist;
    ctl->mem->free_paths += count;
    ctl->mem->mem_paths  += count;
}



PATH *mem_new_path( char *str, int len )
{
	PATH *p;
	int sz;

	pthread_mutex_lock( &(ctl->locks->path) );

	if( !ctl->mem->paths )
        __mem_new_paths( NEW_PATHS_BLOCK_SZ );

	p = ctl->mem->paths;
	ctl->mem->paths = p->next;
	--(ctl->mem->free_paths);

	pthread_mutex_unlock( &(ctl->locks->path) );

	p->next = NULL;

	// we use double the length because we may need two
	// copies, one for strwords to eat
	sz = 2 * ( len + 1 );
	if( p->sz < sz )
	{
		free( p->str );
		p->sz  = sz;
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


QUERY *mem_new_query( void )
{
	QUERY *q;

	pthread_mutex_lock( &(ctl->locks->query) );

	if( ctl->mem->queries )
	{
		q = ctl->mem->queries;
		ctl->mem->queries = q->next;
		--(ctl->mem->free_queries);
		pthread_mutex_unlock( &(ctl->locks->query) );

		q->next = NULL;
	}
	else
	{
		++(ctl->mem->mem_queries);
		pthread_mutex_unlock( &(ctl->locks->query) );

		q = (QUERY *) allocz( sizeof( QUERY ) );
	}

	return q;
}

void mem_free_query( QUERY **q )
{
	QUERY *sq;
	C3RES *rs;
	char *eb;
	int i;

	if( !q || !*q )
		return;

	sq = *q;
	*q = NULL;

	if( sq->results )
	{
		for( rs = sq->results, i = 0; i < sq->rcount; i++, rs++ )
		{
			if( rs->points )
				free( rs->points );
			free( rs );
		}
	}

	if( sq->nodes )
		free( sq->nodes );

	if( sq->path )
		mem_free_path( &(sq->path) );

	// keep the error buffer and zero the rest
	eb = sq->errbuf;
	memset( sq, 0, sizeof( QUERY ) );
	sq->errbuf = eb;

	pthread_mutex_lock( &(ctl->locks->query) );

	sq->next = ctl->mem->queries;
	ctl->mem->queries = sq;
	++(ctl->mem->mem_queries);

	pthread_mutex_unlock( &(ctl->locks->query) );
}





MEM_CTL *mem_config_defaults( void )
{
	// trivial
	return (MEM_CTL *) allocz( sizeof( MEM_CTL ) );
}


#undef LLFID

