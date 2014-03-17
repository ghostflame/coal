#include "coal.h"





void relay_add_point( RDEST *d, POINT *p )
{
	return;
}



static RDEST *rd_cfg_curr = NULL;

int relay_config_line( AVP *av )
{
	struct hostent *he;
	RDEST *d;

	if( !rd_cfg_curr )
	{
		rd_cfg_curr = (RDEST *) allocz( sizeof( RDEST ) );
		rd_cfg_curr->type = NET_COMM_MAX;
	}

	d = rd_cfg_curr;

	if( attIs( "name" ) )
	{
		if( d->name )
		{
			warn( "Relay destination '%s' also contains name '%s' - is a 'done' missing?",
				d->name, av->val );
			return -1;
		}

		d->name = str_dup( av->val, av->vlen );

		debug( "Relay destination %s config started.", d->name );
	}
	else if( attIs( "address" ) || attIs( "addr" ) )
	{
		if( !inet_aton( av->val, &(d->data.sin_addr) ) )
		{
			warn( "Invalid IP address '%s' for relay destination %s",
				av->val, ( d->name ) ? d->name : "as yet unnamed" );
			return -1;
		}
		// and copy that for queries
		d->query.sin_addr = d->data.sin_addr;

		debug( "Relay destination %s has address %s",
			( d->name ) ? d->name : "as yet unnamed",
			inet_ntoa( d->data.sin_addr ) );
	}
	else if( attIs( "dport" ) )
	{
		d->data.sin_port = htons( (uint16_t) strtoul( av->val, NULL, 10 ) );

		debug( "Relay destination %s sends data to port %hu",
			( d->name ) ? d->name : "as yet unnamed", ntohs( d->data.sin_port ) );
	}
	else if( attIs( "qport" ) )
	{
		d->data.sin_port = htons( (uint16_t) strtoul( av->val, NULL, 10 ) );

		debug( "Relay destination %s sends queries to port %hu",
			( d->name ) ? d->name : "as yet unnamed", ntohs( d->query.sin_port ) );
	}
	else if( attIs( "type" ) )
	{
		if( !strcasecmp( av->val, "line" ) )
			d->type = NET_COMM_LINE;
		else if( !strcasecmp( av->val, "bin" ) || !strcasecmp( av->val, "binary" ) )
		 	d->type = NET_COMM_BIN;
		else
		{
			warn( "Unrecognised relay communications type '%s'", av->val );
			return -1;
		}
	}
	else if( attIs( "done" ) )
	{
		if( !d->name )
		{
			warn( "Cannot process unnamed relay destination block." );
			return -1;
		}

		if( !d->data.sin_addr.s_addr ) {
			// attempt lookup on name as a hostname
			if( !( he = gethostbyname( d->name ) ) )
			{
				warn( "Failed to look up relay destination '%s' -- %s",
					d->name, Err );
				return -1;
			}
			// grab the first IP address
			inet_aton( he->h_addr, &(d->data.sin_addr) );
			d->query.sin_addr = d->data.sin_addr;

			debug( "Converted relay name %s to IP address %s",
				d->name, inet_ntoa( d->data.sin_addr ) );
		}

		// default to binary type comms for relaying
		if( d->type == NET_COMM_MAX )
			d->type = NET_COMM_BIN;

		// port defaults to 3801 or 3811
		if( !d->data.sin_port )
		{
			if( d->type == NET_COMM_LINE )
				d->data.sin_port = htons( DEFAULT_NET_LINE_DATA_PORT );
			else
				d->data.sin_port = htons( DEFAULT_NET_BIN_DATA_PORT );

			debug( "Relay destination %s defaulting to port %hu for data",
				d->name, ntohs( d->data.sin_port ) );
		}
		if( !d->query.sin_port )
		{
			if( d->type == NET_COMM_LINE )
				d->query.sin_port = htons( DEFAULT_NET_LINE_QUERY_PORT );
			else
				d->query.sin_port = htons( DEFAULT_NET_BIN_QUERY_PORT );

			debug( "Relay destination %s defaulting to port %hu for queries",
				d->name, ntohs( d->query.sin_port ) );
		}

		// winner!
		d->next = ctl->relay->dests;
		ctl->relay->dests = d;
		ctl->relay->dcount++;

		debug( "Relay destination %s created.", d->name );

		// and flatten the static
		rd_cfg_curr = NULL;
	}

	return 0;
}




REL_CTL *relay_config_defaults( void )
{
	REL_CTL *r;

	r = (REL_CTL *) allocz( sizeof( REL_CTL ) );

	return r;
}
