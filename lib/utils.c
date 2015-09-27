#include "local.h"

double libcoal_tmdbl( double *td )
{
	struct timeval tv;
	double d;

	gettimeofday( &tv, NULL );

	d  = (double) tv.tv_usec;
	d /= 1000000.0;
	d += (double) tv.tv_sec;

	if( td )
		*td = d;

	return d;
}

