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
#if 0 /* coreyliu 20040202, not use the envm */
extern	bd_t	*bd; //	import from ~/linux/drivers/char/envm-s5N8947.c
extern	int	setenv(bd_t *bd, char *varname, char *varvalue);	//	import from ~/linux/drivers/char/envm-s5n8947.c
extern	int	board_env_save(bd_t *bd, env_t *env, int size);	//	import from ~/linux/drivers/char/envm-s5n8947.c
#else
bd_t *bd;
int setenv(bd_t *bd, char *varname, char *varvalue)
{
}
int board_env_save(bd_t *bd, env_t *env, int size)
{

}	
#endif
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
	sprintf(flash_buf, "br2684ctl -c %d -m 2 -a %d.%d &",	p_env->if_index,
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

// nancho 08.20.
// atm_qos start
    sprintf(flash_buf,"adsldmt a q %d",p_env->if_index);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
// atm_qos end


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
	
// shs  2003.08.08
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
	char setting_command[34][60];
	int i;

	strcpy(setting_command[ 0],"adsldmt r w");
	strcpy(setting_command[ 1],"ifconfig eth0 down");
	strcpy(setting_command[ 2],"ifconfig eth0 0.0.0.0");
	strcpy(setting_command[ 3],"br2684ctl -c 0 -a 0.32 &");
	strcpy(setting_command[ 4],"brctl addbr sbridge");
	strcpy(setting_command[ 5],"brctl stp sbridge off");
	strcpy(setting_command[ 6],"brctl addif sbridge eth0");
	strcpy(setting_command[ 7],"samsung_sleep ");
	strcpy(setting_command[ 8],"adsldmt a q 0");				// nancho 08.20.
	strcpy(setting_command[ 9],"brctl addif sbridge nas0");
	strcpy(setting_command[10],"ifconfig nas0 up");
	strcpy(setting_command[11],"ifconfig sbridge 192.168.1.1 netmask 255.255.255.0");
	strcpy(setting_command[12],"samsung_sleep         ");
	strcpy(setting_command[13],"br2684ctl -c 1 -a 0.35 &");
	strcpy(setting_command[14],"samsung_sleep          ");
	strcpy(setting_command[15],"brctl addif sbridge nas1");
	strcpy(setting_command[16],"adsldmt a q 1");				// nancho 08.20.
	strcpy(setting_command[17],"samsung_sleep           ");
	strcpy(setting_command[18],"ifconfig nas1 up");
	strcpy(setting_command[19],"samsung_sleep                 ");
	strcpy(setting_command[20],"br2684ctl -c 2 -a 8.35 &");
	strcpy(setting_command[21],"samsung_sleep                  ");
	strcpy(setting_command[22],"brctl addif sbridge nas2");
	strcpy(setting_command[23],"adsldmt a q 2");				// nancho 08.20.
	strcpy(setting_command[24],"samsung_sleep                   ");
	strcpy(setting_command[25],"ifconfig nas2 up");
	strcpy(setting_command[26],"samsung_sleep                         ");
	strcpy(setting_command[27],"br2684ctl -c 3 -a 8.81 &");
	strcpy(setting_command[28],"samsung_sleep                          ");
	strcpy(setting_command[29],"brctl addif sbridge nas3");
	strcpy(setting_command[30],"adsldmt a q 3");				// nancho 08.20.
	strcpy(setting_command[31],"samsung_sleep                           ");
	strcpy(setting_command[32],"ifconfig nas3 up");
	strcpy(setting_command[33],"udhcpd&");

	
	for(i=0;i<34;i++)
	{
		sprintf(flash_buf, "%s", setting_command[i]);
		sys_env(flash_buf);
#ifdef	__KERNEL__
#else
		fprintf(fw, "%s\n", flash_buf);
#endif
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	}
	
	sprintf(flash_buf, "iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -j MASQUERADE");
	sys_env(flash_buf);
#ifdef	__KERNEL__
#else
	fprintf(fw, "%s\n", flash_buf);
#endif
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	return	ST_OK;
}
// shs end

// shs   2003.08.08
#if 0
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
#endif
// shs end

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
//// ALG
#if 0
	if(p_env->dhcp_use)
	{ 
        	sprintf(flash_buf, "iptables -t nat -A PREROUTING -i %s -p tcp --dport 1503 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 1720 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 1731 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
        //====  
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 1723 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 161 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
		
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 162 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 389 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

        	sprintf(flash_buf,"iptables -t nat -A PREROUTING -i %s -p tcp --dport 522 -j DNAT --to %s-%s","ppp0",p_env->dhcp_ip_from,p_env->dhcp_ip_to);
		sys_env(flash_buf);
		memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
               
    	}
#endif
//

// shs   2003.08.28
	sprintf(flash_buf, "/var/EXfilter");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	sprintf(flash_buf, "/var/RouteTb.conf");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
// end shs

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

// shs 2003.08.08
/////////////////////////////////////////////////
//	
//	aal5_x_start
	sprintf(flash_buf, "aal5_0_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "RFC1483br 0 32 0 1 1 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#if 0	
	sprintf(flash_buf, "1 0 0 0 0 0");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
	sprintf(flash_buf, "aal5_0_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_1_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "RFC1483br 0 35 0 1 1 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#if 0	
	sprintf(flash_buf, "1 0 0 0 0 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
	sprintf(flash_buf, "aal5_1_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "aal5_2_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "RFC1483br 8 35 0 1 1 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#if 0	
	sprintf(flash_buf, "1 0 0 0 0 2");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
	sprintf(flash_buf, "aal5_2_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "aal5_3_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "RFC1483br 8 81 0 1 1 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#if 0	
	sprintf(flash_buf, "1 0 0 0 0 3");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
#endif
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
// shs end
	
// shs   2003.08.08
#if 0 
/////////////////////////////////////////////////
//	
//	aal5_x_start
	sprintf(flash_buf, "aal5_0_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	// 8 parameter -> 6 parameter by shs 03/04/15
	sprintf(flash_buf, "%s %d %d %s %s %s",	p_env->encap,
													p_env->vpi,
													p_env->vci,
													p_env->ip_addr,
													p_env->netmask,
													p_env->ppp_id);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "%s %d %d %d %d %d",	p_env->ppp_pw,
													p_env->ppp_default_route,	
													p_env->dhcp_use,
													1,1,1);
			
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
#endif
// shs end

// nancho 08.20
/////////////////////////////////////////////////
//
//	qos_start

	sprintf(flash_buf, "qos_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"0 0 32 0 0 0 0 0 0");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"1 0 35 0 0 0 0 0 0");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"2 8 35 0 0 0 0 0 0");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"3 8 81 0 0 0 0 0 0");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
	
	sprintf(flash_buf, "qos_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	qos_end
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
	// nancho 03.04.01.
	// change some nat rule entries from NAPT to NAPT(MASQ) and
	//                              from (char *)subnet_to to p_env->netmask
    sprintf(flash_buf, "%s %s %s %s %s %s %s %s %s %s %s %s",	"NAPT(MASQ)",
																"1",
																"-",
																"-",
																(char *)subnet_from,	//"192.168.1.0",
																p_env->netmask,
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

	/*
	 *  added by cecece 03/04/07
	 */
	
	sprintf(flash_buf,"lcp-echo-interval %d",	p_env->ppp_lcp_echo_interval);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf,"lcp-echo-failure %d",	p_env->ppp_lcp_echo_failure);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	/* end */
	
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

	sprintf(flash_buf, "MANAGENAME=%s",			p_env->manage_id	);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	

	sprintf(flash_buf, "MANAGEPW=%s",			p_env->manage_pw	);
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
//	cur_dhcp_start
	sprintf(flash_buf, "cur_dhcp_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "%s %s %s %s %s %s %d",		"dhcp_server ",
											p_env->dhcp_ip_from,
											p_env->dhcp_ip_to,
											"--",
											p_env->ip_addr,
											p_env->dhcp_netmask,
											p_env->dhcp_lease_time		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);

	sprintf(flash_buf, "cur_dhcp_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);
//
//	cur_dhcp_end
/////////////////////////////////////////////////
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

// shs   2003.08.14   in case of bridge, no default gw.
#if 0   
	sprintf(flash_buf, "option\t router %s",		p_env->dhcp_gateway		);
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);	
#endif
	
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

/* KTH-- 031027, Useless code */
// shs   2003.07.18
//	dslconf_start
/*
	sprintf(flash_buf, "dslconf_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "6 0 1 0 1");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "dslconf_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
*/
//	dslconf_end
// shs end
/* end of KTH-- */


// shs   2003.08.28
	sprintf(flash_buf, "netfilter_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
	sprintf(flash_buf, "netfilter_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
	sprintf(flash_buf, "route_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
	sprintf(flash_buf, "route_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
// end shs
//
// 10.14 Xavi Buglist #9
{
	sprintf(flash_buf, "pppoe_idle_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "60 ");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
	
	sprintf(flash_buf, "pppoe_idle_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
                    
	sprintf(flash_buf, "pppoa_idle_start");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "60");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		

	sprintf(flash_buf, "pppoa_idle_end");
	sys_env(flash_buf);
	memset(flash_buf, 0, sizeof(char) * BUF_SIZE);		
}

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
	strcpy(env.manage_id,			FD_MANAGE_ID);
	strcpy(env.manage_pw,		FD_MANAGE_PW);
	
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

	env.ppp_lcp_echo_interval	=	FD_PPP_LCP_ECHO_INTERVAL;		// cecece (03.04.07)
	env.ppp_lcp_echo_failure 	=	FD_PPP_LCP_ECHO_FAILURE; 		// cecece (03.04.07)

	status = save_environment(&env);

	if (status != ST_OK)
	{
		m_print("Factory default saving failed! error code=%d\n", status);
	}
}

