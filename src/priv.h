#ifndef COAL_PRIV_H
#define COAL_PRIV_H

struct priv_control
{
	char			*	user;
	char			*	group;
	char			*	basedir;
	int					chroot;

	uid_t				uid;
	gid_t				gid;

	uid_t				orig_uid;
	gid_t				orig_gid;
};

int priv_get_user( void );
int priv_set_user( void );
int priv_set_dirs( void );

PRIV_CTL* priv_config_defaults( void );
int priv_config_line( AVP *av );

#endif
