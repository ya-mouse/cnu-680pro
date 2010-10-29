//#include	"../../../drivers/char/envm-s3c2510.h"		// Too much dependencies!!! WHY!!! Grrrr!
#define CFG_ENV_SIZE  0x2000
#define CONFIG_NR_DRAM_BANKS    1
#ifndef ulong
#define ulong unsigned long
#endif
typedef struct environment_s{ulong crc; unsigned char data[CFG_ENV_SIZE - sizeof(ulong)];} env_t;
struct bd_info_ext{int env_crc_valid;};typedef struct bd_info
{int bi_baudrate; unsigned long	bi_ip_addr; unsigned char	bi_enetaddr[6];
env_t *bi_env; ulong bi_arch_number; ulong bi_boot_params; 
struct {ulong start;ulong size;} bi_dram[CONFIG_NR_DRAM_BANKS];
struct bd_info_ext  bi_ext; } bd_t;  
#if 1
 #ifndef	_GENERIC_H_
 #define	_GENERIC_H_

#ifndef	UINT
#define	UINT	unsigned int
#endif

#ifndef	m_print
	#ifdef	__KERNEL__
		#define	m_print	printk
	#else
		#define	m_print	printf
	#endif
#endif

typedef	enum {
	ST_OK,
	ST_FILE_ERROR,
	ST_ENCAP_MODE_ERROR
}	m_STATUS;

#ifndef	BUF_SIZE		//	plz, check any other same name!
#define		BUF_SIZE		128
#endif
  #endif	//	#ifndef	_GENERIC_H_
#endif
