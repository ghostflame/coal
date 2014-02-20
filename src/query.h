#ifndef COAL_QUERY_H
#define COAL_QUERY_H


enum query_line_fields
{
	QUERY_FIELD_PATH = 0,
	QUERY_FIELD_START,
	QUERY_FIELD_END,
	QUERY_FIELD_METRIC,
	QUERY_FIELD_FORMAT,
	QUERY_FIELD_COUNT
};

enum
{
	QUERY_FMT_INVALID = -1,
	QUERY_FMT_FIELDS,
	QUERY_FMT_JSON,
	QUERY_FMT_MAX
};


struct query_data
{
	QUERY			*	next;
	PATH			*	path;
	NODE			*	node;

	time_t				start;
	time_t				end;
	int					rtype;
	int					format;
	int					tree;

	C3RES				res;
};


char *query_format_name( int fmt );
int query_format_type( char *str );

throw_fn query_loop;

#endif
