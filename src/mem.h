#ifndef COAL_NET_H
#define COAL_NET_H


struct memory_control
{
	HOST		*	hosts;
	PATH		*	paths;
	POINT		*	points;
	QUERY		*	queries;

	int				free_hosts;
	int				free_paths;
	int				free_points;
	int				free_queries;

	int				mem_hosts;
	int				mem_paths;
	int				mem_points;
	int				mem_queries;
};



HOST *mem_new_host( void );
void mem_free_host( HOST **h );

POINT *mem_new_point( void );
POINT *mem_new_points( int count );
void mem_free_point( POINT **p );
void mem_free_point_list( POINT *list );

PATH *mem_new_path( char *str, int len );
void mem_free_path( PATH **p );
void mem_free_path_list( PATH *list );

QUERY *mem_new_query( void );
void mem_free_query( QUERY **q );

MEM_CTL *mem_config_defaults( void );

#endif
