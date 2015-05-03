#ifndef COAL_QUERY_H
#define COAL_QUERY_H


#define QUERY_DEFAULT_ERROR		"Unknown problem - really, how broken was your request to do that?"
#define QUERY_ERRBUF_SZ			4096


enum query_line_fields
{
	QUERY_FIELD_TYPE = 0,
	QUERY_FIELD_ID,
	QUERY_FIELD_FORMAT,
	QUERY_FIELD_PATH,
	QUERY_FIELD_START,
	QUERY_FIELD_END,
	QUERY_FIELD_METRIC,
	QUERY_FIELD_MAX
};


enum query_formats
{
	QUERY_FMT_INVALID = -1,
	QUERY_FMT_FIELDS,
	QUERY_FMT_JSON,
	QUERY_FMT_BIN,
	QUERY_FMT_MAX
};

// make these names available
extern const char *query_type_name_strings[QUERY_TYPE_MAX];

#define query_type_get_name( q )	( ( q->type < QUERY_TYPE_MAX && q->type > QUERY_TYPE_INVALID ) ? query_type_name_strings[q->type] : "invalid" )


struct query_data
{
	QUERY           *   next;
	HOST            *   host;
	PATH            *   path;
	NODE            *   node;
	C3RES           *   results;
	NODE            **  nodes;
	char            *   error;
	char            *   errbuf;

	int8_t              type;
	int8_t              format;
	uint8_t             rtype;
	uint8_t             id;
	uint32_t            rcount;

	regex_t             search;

	time_t              start;
	time_t              end;
	time_t              last;
};



struct query_control
{
	int                 unused;
};


char *query_format_name( int fmt );
int query_format_type( char *str );

int query_config_line( AVP *av );
QRY_CTL *query_config_defaults( void );

query_read_fn query_line_read;
query_read_fn query_bin_read;

throw_fn query_loop;

#endif
