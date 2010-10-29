/*
 *	factory_default.c
 *	
 *	factory default value setting funtions
 *
 *	Jemings Ko
 *	System Solution PJT
 *	Samsung Electronics, Co., Ltd.
 *
 *	Created Date	:	2003.03.04
 *	Last Modified 	: 	2003.03.11
 *
 *	History	:	2003.03.08	First released version
 *				2003.03.11	Completely renewal version
 */

#ifdef	__KERNEL__
#else
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#endif
#ifdef	__KERNEL__
#include	"kludge.h"	//	^_^;
#else
#include	"generic.h"
#endif
#include	"factory_default.h"


extern void	factory_default(void);
m_STATUS	save_environment(PADSL_ENV	env);

#ifdef	__KERNEL__
extern	bd_t	*bd;											//	import from ~/linux/drivers/char/envm-s5N8947.c
extern	int	setenv(bd_t *bd, char *varname, char *varvalue);	//	import from ~/linux/drivers/char/envm-s5n8947.c
extern	int	board_env_save(bd_t *bd, env_t *env, int size);	//	import from ~/linux/drivers/char/envm-s5n8947.c
#else
#endif


void
get_subnet(
	char		*subnet, 
	char		*ip, 
	char		*netmask
)
{
 	int	temp_ip[4];
	int	temp_netmask[4];

	memset(subnet, 0, sizeof(char) * 20);
	
	sscanf(ip,"%d.%d.%d.%d",	&temp_ip[0],
							&temp_ip[1],
							&temp_ip[2],
							&temp_ip[3]	);

	sscanf(netmask,"%d.%d.%d.%d",	&temp_netmask[0],
									&temp_netmask[1],
									&temp_netmask[2],
									&temp_netmask[3]		);

	sprintf(subnet,"%d.%d.%d.%d",		(temp_ip[0]&temp_netmask[0]),
									(temp_ip[1]&temp_netmask[1]),
									(temp_ip[2]&temp_netmask[2]),
									(temp_ip[3]&temp_netmask[3])	);
	return;
}

void
get_subnet_end(
	char	*subnet_to,
	char	*subnet_from,
	char	*netmask
)
{
	int	temp_ip[4];
	int	temp_netmask[4];

	memset(subnet_to, 0, sizeof(char) * 20);

	sscanf(subnet_from, "%d.%d.%d.%d", 	&temp_ip[0],
										&temp_ip[1],
										&temp_ip[2],
										&temp_ip[3]		);

	sscanf(netmask, "%d.%d.%d.%d", 	&temp_netmask[0],
									&temp_netmask[1],
									&temp_netmask[2],
									&temp_netmask[3]	);

	sprintf(subnet_to, "%d.%d.%d.%d",	(temp_ip[0] | (temp_netmask[0] ^ 255)),
										(temp_ip[1] | (temp_netmask[1] ^ 255)),
										(temp_ip[2] | (temp_netmask[2] ^ 255)),
										(temp_ip[3] | (temp_netmask[3] ^ 255))	);
}

//Getting the number of netmask bits
int
get_subnet_bit(
	char* netmask
)
{
	int	i, j, count = 0;
	int	temp_netmask[4];

	sscanf(netmask, "%d.%d.%d.%d",	&temp_netmask[0],
									&temp_netmask[1],
									&temp_netmask[2],
									&temp_netmask[3]		);

	for (i=0; i<4; i++)
	{
		for (j=0; j<8; j++)
		{
			if ( temp_netmask[i] & (1<<j) )
			{
				count++;
			}
		}
	}
	return count;
}


void
sys_env(
	char		*buf
)
{
#ifdef	__KERNEL__
	setenv(bd, buf, "0");
#else
	char		command[BUF_SIZE + 16] = {};

	sprintf((char *)command, "setmodeenv \"%s\" 0", buf);
	system(command);
#endif
}


ENCAP_MODE
check_encap_mode(
	char *str 
)
{
	ENCAP_MODE		encap;
	
	if (!strncmp((const char *)str, "PPPOE", 5))
	{
		encap = ENCAP_PPPOE;	
	}
	else if (!strncmp((const char *)str, "PPPOA", 5))
	{
		encap = ENCAP_PPPOA;
	}
	else if (!strncmp((const char *)str, "RFC1483br", 9))
	{
		encap = ENCAP_RFC1483BR;
	}
	else if (!strncmp((const char *)str, "RFC1483rt", 9))
	{
		encap = ENCAP_RFC1483RT;
	}
	else
	{
		encap = ENCAP_NOT_SUPPORTED;
	}
	
	return	encap;
}


#ifdef	__KERNEL__
m_STATUS
flash_setting_pppoe(
	PADSL_ENV	p_env,
	char			*flash_buf
)
#else
m_STATUS
flash_setting_pppoe(
	PADSL_ENV	p_env,
	char			*flash_buf,
	FILE			*fw
)
#endif
{
	sprintf(flash_buf, "br2684ctl -c %d -a %d.%d &",	p_env->if_index,
												p_env->vpi,
												p_env->vci		);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	if (p_env->mtu)
	{
		sprintf(flash_buf, "ifconfig nas%d mtu %d",	p_env->if_index,
												p_env->mtu			);	//	I'll not touch nas MTU at this time.
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	else
	{
	}

#if	0	//	Why did you run nas up twice times?
	sprintf(flash_buf,"ifconfig nas%d up",				p_env->if_index);
	sys_env(flash_buf);
#ifdef	__KERNEL__		
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
		
	sprintf(flash_buf, "ifconfig eth0 %s netmask %s",	p_env->ip_addr,
												p_env->netmask	);	//	I'll not touch eth index at this time.
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	{
	char		subnet[20];
	get_subnet(subnet, p_env->ip_addr, p_env->netmask);
	sprintf(flash_buf, "iptables -t nat -A POSTROUTING -s %s/%d -j MASQUERADE",
												(char *)subnet,
												get_subnet_bit(p_env->netmask)	);	//	Should be imported these functions
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}

	sprintf(flash_buf, "ifconfig nas%d up",				p_env->if_index);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"start_pppoe&");
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	if (p_env->dhcp_use)
	{
		sprintf(flash_buf, "udhcpd&");
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}

	return	ST_OK;
}
	

#ifdef	__KERNEL__
m_STATUS
flash_setting_rfc1483br(
	PADSL_ENV	p_env,
	char			*flash_buf
)
#else
m_STATUS
flash_setting_rfc1483br(
	PADSL_ENV	p_env,
	char			*flash_buf,
	FILE			*fw
)
#endif
{
	sprintf(flash_buf, "br2684ctl -c %d -a %d.%d &",	p_env->if_index,
												p_env->vpi,
												p_env->vci		);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "ifconfig nas%d 0.0.0.0",	p_env->if_index	);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "ifconfig eth0 0.0.0.0");			//	I'll not touch eth index at this time.
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "brctl addbr sbridge");
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "brctl stp sbridge off");	
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "brctl addif sbridge eth0");		//	I'll not touch eth index at this time.
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "brctl addif sbridge nas%d",	p_env->if_index	);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "ifconfig nas%d up",		p_env->if_index	);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "ifconfig sbridge %s netmask %s",	p_env->ip_addr,
													p_env->netmask	);
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	return	ST_OK;
}


m_STATUS
save_environment(
	PADSL_ENV	p_env
)
{
#ifdef	__KERNEL__
#else
	FILE		*fw = NULL;
#endif
	char				flash_buf[BUF_SIZE] = {};
	ENCAP_MODE		encap = ENCAP_NOT_SUPPORTED;
	m_STATUS		status = ST_OK;

#if 1	//	I need more time!
	encap = check_encap_mode(p_env->encap);

	if (encap == ENCAP_NOT_SUPPORTED)
	{
	#ifdef	DBG
		m_print("Not supported encapsulation mode...%s\n", p_env->encap);
	#endif
		return	ST_ENCAP_MODE_ERROR;		
	}
#else
	if (!strncmp((const char *)(p_env->encap), "PPPOE", 5))
	{
		encap = ENCAP_PPPOE;	
	}
	else if (!strncmp((const char *)(p_env->encap), "PPPOA", 5))
	{
		encap = ENCAP_PPPOA;
	}
	else if (!strncmp((const char *)(p_env->encap), "RFC1483br", 9))
	{
		encap = ENCAP_RFC1483BR;
	}
	else if (!strncmp((const char *)(p_env->encap), "RFC1483rt", 9))
	{
		encap = ENCAP_RFC1483RT;
	}
	else
	{
		encap = ENCAP_NOT_SUPPORTED;
	#ifdef	DBG
		m_print("Not supported encapsulation mode...%s\n", p_env->encap);
	#endif
		return	ST_ENCAP_MODE_ERROR;
	}
#endif

/////////////////////////////////////////////////
//	samsung_adsl
//	adsl_start
#ifdef	__KERNEL__
#else
	if ((fw = fopen("/var/samsung_adsl", "w")) == NULL)
	{
	#ifdef	DBG
		m_print("Cannot make /var/samsung_adsl file!\n");
	#endif
		return	ST_FILE_ERROR;
	}
#endif

	sprintf(flash_buf, "adsl_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	if (p_env->dmt_reset)
	{
		sprintf(flash_buf, "adsldmt r m 0");
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		
		sprintf(flash_buf, "adsldmt r y");
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	
#if 1
	switch(encap)
	{
		case	ENCAP_PPPOE:	//	Fall through...
		case	ENCAP_PPPOA:	//	I don't know how to set PPPoA case..., but I'll suppose both will be same at this time.
	#ifdef	__KERNEL__
			status = flash_setting_pppoe(p_env, flash_buf);
	#else
			status = flash_setting_pppoe(p_env, flash_buf, fw);
	#endif
			break;

		case	ENCAP_RFC1483BR:	//	Fall through...
		case	ENCAP_RFC1483RT:	//	I don't know how to set RFC1483rt case..., but I'll suppose both will be same at this time.
	#ifdef	__KERNEL__
			status = flash_setting_rfc1483br(p_env, flash_buf);
	#else
			status = flash_setting_rfc1483br(p_env, flash_buf, fw);
	#endif
			break;

		case	ENCAP_NOT_SUPPORTED:	//	Fall through...
		default:
			status = ST_ENCAP_MODE_ERROR;
			break;
	}
#else
	if (!strncmp((const char *)(p_env->encap), "PPP", 3))	//	If truth, that's PPP mode.
	{
		sprintf(flash_buf, "br2684ctl -c %d -a %d.%d &",	p_env->if_index,
													p_env->vpi,
													p_env->vci		);
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

		if (p_env->mtu)
		{
			sprintf(flash_buf, "ifconfig nas%d mtu %d",		p_env->if_index,
													p_env->mtu			);	//	I'll not touch nas MTU at this time.
			sys_env(flash_buf);
#ifdef	__KERNEL__
#else
			fprintf(fw, "%s\n", flash_buf);
#endif
			memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		}
		else
		{
		}

#if	0	//	Why did you run nas up twice times?
		sprintf(flash_buf,"ifconfig nas%d up",				p_env->if_index);
		sys_env(flash_buf);
#ifdef	__KERNEL__		
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
		
		sprintf(flash_buf, "ifconfig eth0 %s netmask %s",	p_env->ip_addr,
													p_env->netmask	);	//	I'll not touch eth index at this time.
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	{
		char		subnet[20];
		get_subnet(subnet, p_env->ip_addr, p_env->netmask);
		sprintf(flash_buf, "iptables -t nat -A POSTROUTING -s %s/%d -j MASQUERADE",
													(char *)subnet,
													get_subnet_bit(p_env->netmask)	);	//	Should be imported these functions
	}
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

		sprintf(flash_buf, "ifconfig nas%d up",				p_env->if_index);
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

		sprintf(flash_buf,"start_pppoe&");
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	else			//	I'll not touch not_ppp case at this time
	{
	}

	sprintf(flash_buf, "udhcpd&");
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif

	sprintf(flash_buf, "adsl_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

#ifdef	__KERNEL__
#else
	if (fw)
	{
		fclose(fw);
		fw = NULL;
	}
#endif

#ifdef	__KERNEL__
#else
	system("chmod 755 /var/samsung_adsl");
#endif
//
//	adsl_end
/////////////////////////////////////////////////

/////////////////////////////////////////////////
//	
//	aal5_x_start
	sprintf(flash_buf, "aal5_0_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "%s %d %d %s %s %s %s %d",	p_env->encap,
													p_env->vpi,
													p_env->vci,
													p_env->ip_addr,
													p_env->netmask,
													p_env->ppp_id,
													p_env->ppp_pw,
													p_env->ppp_default_route);	
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "%d %d %d %d %d %d %d %d",	p_env->dhcp_use,
													1,1,1,1,1,1,1);
			
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_0_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_1_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_1_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_2_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_2_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "aal5_3_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_3_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_4_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_4_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_5_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_5_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_6_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_6_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_7_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "aal5_7_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	aal5_x_end
/////////////////////////////////////////////////	

/////////////////////////////////////////////////
//
//	nat_rule_start

	sprintf(flash_buf, "nat_rule_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	{
	char subnet_from[20], subnet_to[20];
	get_subnet(subnet_from, p_env->ip_addr, p_env->netmask);
	get_subnet_end(subnet_to, subnet_from, p_env->netmask);
    sprintf(flash_buf, "%s %s %s %s %s %s %s %s %s %s %s %s",	"NAPT",
																"1",
																"-",
																"-",
																(char *)subnet_from,	//"192.168.1.0",
																(char *)subnet_to,		//"192.168.1.255",
																"-",
                                                        		"anywhere",
																"-",
																"-",
																"-",
																"-"			);
	}
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "nat_rule_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	nat_rule_end
/////////////////////////////////////////////////	

/////////////////////////////////////////////////
//
//	chap_start & pap_start
	sprintf(flash_buf, "chap_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf,"%s  *   %s",			p_env->ppp_id,
										p_env->ppp_pw	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
	sprintf(flash_buf, "chap_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "pap_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf,"%s  *   %s ",			p_env->ppp_id,
										p_env->ppp_pw	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
	sprintf(flash_buf, "pap_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	chap_end & pap_end
/////////////////////////////////////////////////

/////////////////////////////////////////////////
//
//	ppp_option_start
	sprintf(flash_buf, "ppp_option_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);


	if (p_env->ppp_default_route)
	{
		sprintf(flash_buf, "defaultroute");
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	else
	{
	}

	sprintf(flash_buf, "asyncmap 0");					//	I'll not touch asyncmap at this time.
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	if (p_env->ppp_mru)
	{
		sprintf(flash_buf, "mru %d",			p_env->ppp_mru	);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	else
	{
	}

	if (p_env->ppp_mtu)
	{
		sprintf(flash_buf, "mtu %d",			p_env->ppp_mtu	);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	else
	{
	}

	sprintf(flash_buf,"user %s",				p_env->ppp_id	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"plugin pppoe");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"nas%d",				p_env->if_index	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "usepeerdns");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "ppp_option_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
//
//	ppp_option_end
/////////////////////////////////////////////////

/////////////////////////////////////////////////
//
//	webpassword_start
	sprintf(flash_buf, "webpassword_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "USERNAME=%s",			p_env->user_id	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "PASSWORD=%s",			p_env->user_pw	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "ADMINNAME=%s",			p_env->root_id	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "PASSWORD1=%s",			p_env->root_pw	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "webpassword_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
//
//	webpassword_end
/////////////////////////////////////////////////

	if (p_env->dhcp_use)
	{
/////////////////////////////////////////////////
//
//	dhcp_start
	sprintf(flash_buf, "dhcp_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "%s %s %s %s %s %s",		"dhcp_server",
											p_env->dhcp_ip_from,
											p_env->dhcp_ip_to,
											"--",
											p_env->ip_addr,
											p_env->dhcp_netmask		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "dhcp_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	dhcp_end
/////////////////////////////////////////////////
#if 1
/////////////////////////////////////////////////
//
//	udhcp_start
	sprintf(flash_buf, "udhcp_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "start\t %s",			p_env->dhcp_ip_from		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "end\t %s",				p_env->dhcp_ip_to		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
	
	sprintf(flash_buf, "interface %s",			p_env->dhcp_interface	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "option\t subnet %s",		p_env->dhcp_netmask		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "option\t router %s",		p_env->dhcp_gateway		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "option\t lease %d", 		p_env->dhcp_lease_time	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "udhcp_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
//
//	udhcp_end
/////////////////////////////////////////////////
#endif
	}	//	if (p_env->dhcp_use)
	else
	{
	}

/////////////////////////////////////////////////
//
//	dns_start
	sprintf(flash_buf, "dns_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "nameserver %s",		p_env->dns_primary		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "nameserver %s ",		p_env->dns_secondary	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "dns_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
//
//	dns_end
/////////////////////////////////////////////////

#ifdef	__KERNEL__
	board_env_save(bd, bd->bi_env, sizeof(env_t));
#else
	system("saveenvm");
#endif

	return	status;
}
	

void
factory_default(
	void
)
{
	ADSL_ENV	env;
	m_STATUS	status;

	memset(&env, 0, sizeof(ADSL_ENV));

	env.dmt_reset			=	FD_DMT_RESET;
	
	env.if_index			=	FD_INTERFACE_INDEX;
	
	strcpy(env.encap,			FD_ENCAP);
	env.vpi				=	FD_VPI;
	env.vci				=	FD_VCI;
	env.mtu				=	FD_MTU;
	strcpy(env.ip_addr,		FD_IP_ADDR);
	strcpy(env.netmask,		FD_NETMASK);
	
	strcpy(env.root_id,			FD_ROOT_ID);
	strcpy(env.root_pw,		FD_ROOT_PW);
	strcpy(env.user_id,			FD_USER_ID);
	strcpy(env.user_pw,		FD_USER_PW);
	
	env.dhcp_use		 =	FD_DHCP_USE;
	strcpy(env.dhcp_ip_from,	FD_DHCP_IP_FROM);
	strcpy(env.dhcp_ip_to,		FD_DHCP_IP_TO);
	strcpy(env.dhcp_interface,	FD_DHCP_INTERFACE);
	strcpy(env.dhcp_netmask,	FD_DHCP_NETMASK);
	strcpy(env.dhcp_gateway,	FD_DHCP_GATEWAY);
	env.dhcp_lease_time	=	FD_DHCP_LEASE_TIME;
	
	strcpy(env.dns_primary,	FD_DNS_PRIMARY);
	strcpy(env.dns_secondary,	FD_DNS_SECONDARY);
	
	strcpy(env.ppp_id,			FD_PPP_ID);
	strcpy(env.ppp_pw,		FD_PPP_PW);
	env.ppp_default_route	=	FD_PPP_DEFAULT_ROUTE;
	env.ppp_mru			=	FD_PPP_MRU;
	env.ppp_mtu			=	FD_PPP_MTU;

	status = save_environment(&env);

	if (status != ST_OK)
	{
		m_print("Factory default saving failed! error code=%d\n", status);
	}
}

