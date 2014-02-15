#ifndef COAL_UTILS_H
#define COAL_UTILS_H

#define DEFAULT_PID_FILE	"/var/run/coal"


// for fast string allocation, free not possible
#define PERM_SPACE_BLOCK	0x1000000	// 1M

// for breaking up strings on a delimiter
#define STRWORDS_MAX		255
#define STRWORDS_HWM		254

// var_val status
#define VV_LINE_UNKNOWN		0
#define VV_LINE_BROKEN		1
#define VV_LINE_COMMENT		2
#define VV_LINE_BLANK		3
#define VV_LINE_NOATT		4
#define VV_LINE_ATTVAL		5

// var_val flags
#define	VV_NO_EQUAL			0x01	// don't check for equal
#define	VV_NO_SPACE			0x02	// don't check for space
#define	VV_NO_TAB			0x04	// don't check for tabs
#define	VV_NO_VALS			0x08	// there's just attributes
#define	VV_VAL_WHITESPACE	0x10	// don't clean value whitespace
#define	VV_AUTO_VAL			0x20	// put "1" as value where none present
#define VV_LOWER_ATT		0x40	// lowercase the attribute




struct words_data
{
	char				*	wd[STRWORDS_MAX];
	int						len[STRWORDS_MAX];
	int						in_len;
	int						wc;
};


struct av_pair
{
	char				*	att;
	char				*	val;
	int						alen;
	int						vlen;
	int						status;
};


// FUNCTIONS

// zero'd memory
void *allocz( size_t size );

// report time to nearest usec
double timedbl( double *dp );
void get_time( void );

// allocation of strings that can't be freed
char *perm_str( int len );
char *str_dup( char *src, int len );

// processing a config line in variable/value
int var_val( char *line, int len, AVP *av, int flags );

// break up a string by delimiter
int strwords( WORDS *w, char *src, int len, char sep );


void pidfile_write( void );
void pidfile_remove( void );


#endif
