#ifndef COAL_NET_H
#define COAL_NET_H


struct memory_control
{
	HOST		*	hosts;
	PATH		*	paths;
	POINT		*	points;

	int				free_hosts;
	int				free_paths;
	int				free_points;

	int				mem_hosts;
	int				mem_paths;
	int				mem_points;
};



HOST *mem_new_host( void );
void mem_free_host( HOST **h );

POINT *mem_new_point( void );
void mem_free_point( POINT **p );
void mem_free_point_list( POINT *list );

PATH *mem_new_path( char *str, int len );
void mem_free_path( PATH **p );
void mem_free_path_list( PATH *list );

MEM_CTL *mem_config_defaults( void );

#endif
