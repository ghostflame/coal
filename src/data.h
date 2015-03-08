#ifndef COAL_DATA_H
#define COAL_DATA_H




#define	LINE_SEPARATOR					'\n'
#define	FIELD_SEPARATOR					' '
#define	PATH_SEPARATOR					'.'



struct data_point
{
	POINT			*	next;
	PATH			*	path;
	C3PNT				data;
	int					handled;
};


enum data_line_fields
{
	DATA_FIELD_PATH = 0,
	DATA_FIELD_VALUE,
	DATA_FIELD_TS,
	DATA_FIELD_COUNT
};


char *data_bin_type_names( int type );
int data_path_parse( PATH *path );
uint32_t data_path_cksum( char *str, int len );
void data_add_path_cache( PATH *p, NODE *n, RDEST *d );

throw_fn push_loop;
throw_fn data_loop;

#endif
