/*
 *	factory_default.h
 *	
 *	This header file defines factory default value for ADSL solution
 *
 *	Jemings Ko
 *	System Solution PJT
 *	Samsung Electronics, Co., Ltd.
 *
 *	Created Date	:	2003.03.04
 *	Last Modified 	: 	2003.03.05
 *
 *	History	:
 *	
 *	WARNING:	Watch out when you change these configurations.
 * 				These default values cannot be changed in running time as factory default setting.
 *				Don't misunderstand my words. Surely you can change most environment
 *				variables and use them until you try to reset environment to factory default settings.
 *				Refer to the user's guide to know how to reset values as factory default.
 */

#ifndef	_FACTORY_DEFAULT_H_
#define	_FACTORY_DEFAULT_H_

typedef	enum {
	ENCAP_NOT_SUPPORTED,
	ENCAP_PPPOE,
	ENCAP_PPPOA,
	ENCAP_RFC1483BR,
	ENCAP_RFC1483RT
}	ENCAP_MODE;

typedef	struct	{
	int			dmt_reset;

	UINT		if_index;	
	
	char			encap[20];
	UINT		vpi;
	UINT		vci;
	UINT		mtu;
	char			ip_addr[20];
	char			netmask[20];
	
	char			root_id[64];
	char			root_pw[32];
	char			user_id[64];
	char			user_pw[32];
	
	UINT			dhcp_use;
	char			dhcp_ip_from[20];
	char			dhcp_ip_to[20];
	char			dhcp_interface[20];
	char			dhcp_netmask[20];
	char			dhcp_gateway[20];
	UINT		dhcp_lease_time;

	char			dns_primary[20];
	char			dns_secondary[20];

	char			ppp_id[64];
	char			ppp_pw[32];
	int			ppp_default_route;
	UINT		ppp_mru;
	UINT		ppp_mtu;	
}	ADSL_ENV, *PADSL_ENV;

/*
 *	Basic Configurations (FD means "Factory Default". I hope it will avoid conflicting with other defines.)
 */

//////////////////////////////////////////////////
//	Special parameters
#define	FD_DMT_RESET			0				//	Initialize DMT chip after boot	(Boolean: 0: OFF, 1: ON)
//
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	Interface Settings
//	(Factory default sets only one NAS interface.)
#define	FD_INTERFACE_INDEX	0				//	Default NAS Interface index	(Integer: 0~7(?): anyway, 8 pvc supported)

#define	FD_ENCAP				"PPPOE"			//	Encapsulation mode (Should be one of these! - Case sensitive)
												//				(String:		"RFC1483br"
												//							"RFC1483rt"
												//							"PPPOE"
												//							"PPPOA"		)
												//	Caution: At this time, only "PPPOE" can be used because of lack of time.

#define	FD_VPI					0				//	VPI index					(Integer)
#define	FD_VCI					32				//	VCI index					(Integer)

#define	FD_MTU					1480			//	NAS interface's MTU			(Integer: If 0, it'll be ignored)
												//	Notice: It's different from FD_PPP_MTU.
#define	FD_IP_ADDR				"192.168.1.1"		//	IP address					(String: "xxx.xxx.xxx.xxx")
#define	FD_NETMASK			"255.255.255.0"	//	Netmask					(String: "xxx.xxx.xxx.xxx")
//
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	User Access Information
#define	FD_ROOT_ID				"admin"			//	Supervisor ID				(String)
#define	FD_ROOT_PW			"admin"			//	Supervisor password			(String)

#define	FD_USER_ID				"user"			//	User ID					(String)
#define	FD_USER_PW			"user"			//	User password				(String)
//
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	DHCP Server Information
#define FD_DHCP_USE			1				//	DHCP enable						(Boolean: 0: OFF, 1:ON)
#define	FD_DHCP_IP_FROM		"192.168.1.111"	//	DHCP server IP pool start		(String: "xxx.xxx.xxx.xxx")
#define	FD_DHCP_IP_TO			"192.168.1.211"	//	DHCP server IP pool end		(String: "xxx.xxx.xxx.xxx")
#define	FD_DHCP_INTERFACE		"eth0"			//	DHCP server default interface	(String)
#define	FD_DHCP_NETMASK		"255.255.255.0"	//	DHCP server subnet mask		(String: "xxx.xxx.xxx.xxx")
#define	FD_DHCP_GATEWAY		"192.168.1.1"		//	DHCP server default gateway	(String: "xxx.xxx.xxx.xxx")
#define	FD_DHCP_LEASE_TIME	60				//	DHCP IP lease time			(Integer)
//
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	DNS Information
#define	FD_DNS_PRIMARY		"168.95.1.1"		//	Primary Domain Name Server	(String: "xxx.xxx.xxx.xxx")
#define	FD_DNS_SECONDARY		"168.126.63.1"	//	Secondary Domain Name Server	(String" "xxx.xxx.xxx.xxx")
//
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	PPP
#define	FD_PPP_ID				"test0"			//	PPP authentication ID			(String)
#define	FD_PPP_PW				"test0"			//	PPP authentication password	(String)

#define	FD_PPP_DEFAULT_ROUTE		1			//	PPP default route setting		(Boolean: 0: OFF, 1: ON)
#define	FD_PPP_MRU			1492			//	PPP Max Receive Unit value	(Integer: If 0, it'll be ignored)
#define	FD_PPP_MTU			1492			//	PPP Max Transfer Unit value	(Integer: If 0, it'll be ignored)
//
//////////////////////////////////////////////////


#endif	//	#ifndef	_FACTORY_DEFAULT_H_
