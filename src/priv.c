#include "coal.h"

#define LLFID LLFPR


int priv_set_dirs( void )
{
	PRIV_CTL *p = ctl->priv;

	// are we chrooting?
	if( p->chroot )
	{
		if( chroot( p->basedir ) )
		{
			err( 0x0101, "Unable to chroot to base dir '%s' -- %s",
				p->basedir, Err );
			return -1;
		}
		else
			debug( 0x0102, "Chrooted to base dir %s", p->basedir );
	}

	// can we chdir to our base dir?
	if( chdir( p->basedir ) )
	{
		err( 0x0103, "Unable to chdir to base dir '%s' -- %s",
			p->basedir, Err );
		return -1;
	}
	else
		debug( 0x0104, "Working directory switched to %s", p->basedir );

	return 0;
}




int priv_get_user( void )
{
	PRIV_CTL *p = ctl->priv;
	struct passwd *pwe;
	struct group *gre;

	if( !p->user )
		return 0;

	if( !( pwe = getpwnam( p->user ) ) )
	{
		err( 0x0201, "Unable to find user '%s' on system -- %s.", p->user, Err );
		return -1;
	}

	debug( 0x0202, "Found user '%s' as uid %u.", p->user, pwe->pw_uid );

	p->uid = pwe->pw_uid;
	p->gid = pwe->pw_gid;

	if( p->group )
	{
		if( !( gre = getgrnam( p->group ) ) )
		{
			err( 0x0203, "Unable to find group '%s' on system -- %s.", p->group, Err );
			return -1;
		}

		debug( 0x0204, "Found group '%s' as gid %u.", p->group, gre->gr_gid );

		p->gid = gre->gr_gid;
	}

	return 0;
}


int priv_set_user( void )
{
	PRIV_CTL *p = ctl->priv;

	// has anything changed?

	if( p->gid != p->orig_gid )
	{
		if( setgid( p->gid ) )
		{
			err( 0x0205, "Unable to setgid to %u -- %s", p->gid, Err );
			return -1;
		}

		debug( 0x0206, "Set gid to %u.", p->gid );
	}

	if( p->uid != p->orig_uid )
	{
		if( setuid( p->uid ) )
		{
			err( 0x0207, "Unable to setuid to %u -- %s", p->uid, Err );
			return -1;
		}

		debug( 0x0208, "Set uid to %u.", p->uid );
	}

	return 0;
}



int priv_config_line( AVP *av )
{
	if( attIs( "basedir" ) )
	{
		free( ctl->priv->basedir );
		ctl->priv->basedir = strdup( av->val );
	}
	else if( attIs( "chroot" ) )
	{
		if( valIs( "yes" ) || valIs( "y" ) || atoi( av->val ) )
			ctl->priv->chroot = 1;
		else
			ctl->priv->chroot = 0;
	}
	else if( attIs( "user" ) )
	{
		if( ctl->priv->user )
			free( ctl->priv->user );
		ctl->priv->user = strdup( av->val );
	}
	else if( attIs( "group" ) )
	{
		if( ctl->priv->group )
			free( ctl->priv->group );
		ctl->priv->group = strdup( av->val );
	}
	else
		return -1;

	return 0;


}



PRIV_CTL *priv_config_defaults( void )
{
	PRIV_CTL *p;

	p = (PRIV_CTL *) allocz( sizeof( PRIV_CTL ) );

	p->uid = getuid( );
	p->gid = getgid( );

	p->orig_uid = p->uid;
	p->orig_gid = p->gid;

	p->basedir = strdup( DEFAULT_BASE_DIR );

	return p;
}


#undef LLFID
