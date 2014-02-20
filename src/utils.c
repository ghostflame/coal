#include "coal.h"


double timedbl( double *dp )
{
	struct timeval tv;
	double d;

	gettimeofday( &tv, NULL );

	d  = (double) tv.tv_usec;
	d /= 1000000.0;
	d += (double) tv.tv_sec;

	if( dp )
		*dp = d;

	return d;
}

void get_time( void )
{
	if( ctl )
		timedbl( &(ctl->curr_time) );
}



// zero'd memory
void *allocz( size_t size )
{
	return memset( malloc( size ), 0, size );
}



char *perm_space_ptr  = NULL;
char *perm_space_curr = NULL;
int   perm_space_left = 0;


char *perm_str( int len )
{
	char *p;

	// ensure we are 4-byte aligned
	if( len & 0x3 )
		len += 4 - ( len % 4 );

	// just malloc big blocks
	if( len >= ( PERM_SPACE_BLOCK >> 8 ) )
		return (char *) allocz( len );

	if( len > perm_space_left )
	{
		perm_space_ptr  = (char *) allocz( PERM_SPACE_BLOCK );
		perm_space_curr = perm_space_ptr;
		perm_space_left = PERM_SPACE_BLOCK;
	}

	p = perm_space_curr;
	perm_space_curr += len;
	perm_space_left -= len;

	return p;
}

char *str_dup( char *src, int len )
{
	char *p;

	if( !len )
		len = strlen( src );

	p = perm_str( len + 1 );
	memcpy( p, src, len );

	return p;
}


int strwords( WORDS *w, char *src, int len, char sep )
{
	register char *p = src;
	register char *q = NULL;
	register int   l;
	int i = 0;

	if( !w || !src || !sep )
		return -1;

	if( !len )
		len = strlen( src );

	memset( w, 0, sizeof( WORDS ) );

	w->wc = 0;

	if( !len )
		return 0;

	l = len;
	p = src;

	w->in_len = l;

	// step over leading separators
	while( *p == sep )
	{
		p++;
		l--;
	}

	// anything left?
	if( !*p )
		return 0;

	while( l )
	{
		w->wd[i] = p;

		if( i == STRWORDS_HWM
		 || !( q = memchr( p, sep, l ) ) )
		{
			w->len[i++] = l;
			break;
		}

		w->len[i++] = q - p;
		*q++ = '\0';
		l -= q - p;
		p = q;
	}

	// done
	return ( w->wc = i );
}


#define	VV_NO_CHECKS	0x07
int var_val( char *line, int len, AVP *av, int flags )
{
	char *p, *q, *r, *s;

	if( !line || !av )
		return 1;

	if( ( flags & VV_NO_CHECKS ) == VV_NO_CHECKS )
		flags |= VV_NO_VALS;

	/* blat it */
	memset( av, 0, sizeof( AVP ) );

	p  = line;     // start of line
	r  = line;     // needs init
	s  = p + len;  // end of line
	*s = '\0';     // stamp on the newline

	while( len && isspace( *p ) )
	{
		p++;
		len--;
	}
	if( !( flags & VV_VAL_WHITESPACE )
	 || ( flags & VV_NO_VALS ) )
	{
		while( s > p && isspace( *(s-1) ) )
		{
			*--s = '\0';
			len--;
		}
	}

	/* blank line... */
	if( len < 1 )
	{
		av->status = VV_LINE_BLANK;
		return 0;
	}

	/* comments are successfully read */
	if( *p == '#' )
	{
		av->status = VV_LINE_COMMENT;
		return 0;
	}

	// if we are ignoring values, what we have is an attribute
	if( ( flags & VV_NO_VALS ) )
	{
		av->att = p;
		av->alen = len;

		// if we're automatically setting vals, set 1
		if( flags & VV_AUTO_VAL )
		{
			av->val  = "1";
			av->vlen = 1;
		}
		else
			av->val = "";
	}
	else
	{
		/* search order is =, tab, space */
		q = NULL;
		if( !( flags & VV_NO_EQUAL ) )
			q = memchr( p, '=', len );
		if( !q && !( flags & VV_NO_TAB ) )
			q = memchr( p, '\t', len );
		if( !q && !( flags & VV_NO_SPACE ) )
			q = memchr( p, ' ', len );

		if( !q )
		{
			// are we accepting flag values - no value
			if( flags & VV_AUTO_VAL )
			{
				// then we have an attribute and an auto value
				av->att  = p;
				av->alen = s - p;
				av->val  = "1";
				av->vlen = 1;
				goto vv_finish;
			}

			av->status = VV_LINE_BROKEN;
			return 1;
		}

		// blat the separator
		*q = '\0';
		r  = q + 1; 

		if( !( flags & VV_VAL_WHITESPACE ) )
		{
			// start off from here with the value
			while( r < s && isspace( *r ) )
				r++;
		}

		while( q > p && isspace( *(q-1) ) )
			*--q = '\0';

		// OK, let's record those
		av->att = p;
		av->alen = q - p;
		av->val = r;
		av->vlen = s - r;
	}

	/* got an attribute?  No value might be legal but this ain't */
	if( !av->alen )
	{
		av->status = VV_LINE_NOATT;
		return 1;
	}

vv_finish:

	// are were lower-casing the attributes?
	if( flags && VV_LOWER_ATT )
		for( p = av->att, q = p + av->alen; p < q; p++ )
			*p = tolower( *p );

	/* looks good */
	av->status = VV_LINE_ATTVAL;
	return 0;
}


void pidfile_write( void )
{
	FILE *fh;

	if( !( fh = fopen( ctl->pidfile, "w" ) ) )
	{	
		warn( "Unable to write to pidfile %s -- %s",
			ctl->pidfile, Err );
		return;
	}
	fprintf( fh, "%d\n", getpid( ) );
	fclose( fh );
}

void pidfile_remove( void )
{
	if( unlink( ctl->pidfile ) && errno != ENOENT )
		warn( "Unable to remove pidfile %s -- %s",
			ctl->pidfile, Err );
}
