#ifndef COAL_DATA_H
#define COAL_DATA_H


#define	LINE_SEPARATOR					'\n'
#define	FIELD_SEPARATOR					' '
#define	PATH_SEPARATOR					'.'



struct data_point
{
	POINT			*	next;
	C3PNT				data;
	PATH			*	path;
	int					handled;
};


enum data_line_fields
{
	DATA_FIELD_PATH = 0,
	DATA_FIELD_VALUE,
	DATA_FIELD_TS,
	DATA_FIELD_COUNT
};



void data_add_path_cache( NODE *n, PATH *p );

throw_fn push_loop;
throw_fn data_loop;

#endif
