/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 * 
 * (C) Copyright 2000, 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifdef CONFIG_BOARD_SMDK2510
#define CFG_MAX_FLASH_SECT      35     /* max number of sectors on one chip    */
#endif
#ifdef CONFIG_BOARD_2510REF
#define CFG_MAX_FLASH_SECT		71	 // add for AMD320 4M flash
#endif

/*-----------------------------------------------------------------------
 * FLASH Info: contains chip specific data, per FLASH bank
 */

typedef struct {
	ulong	size;			/* total bank size in bytes		*/
	ushort	sector_count;		/* number of erase units		*/
	ulong	flash_id;		/* combined device & manufacturer code	*/
	ulong	start[CFG_MAX_FLASH_SECT];   /* physical sector start addresses	*/
	unchar 	protect[CFG_MAX_FLASH_SECT]; /* sector protection status	*/
} flash_info_t;


/* Prototypes */
struct bd_info;
ulong flash_init	(struct bd_info *bd);
void flash_print_info	(flash_info_t *);
void Delay1Sec(uint);
void Delay1(uint);
void FlashDelay(uint);
int flash_erase	(flash_info_t *, int, int);
int flash_sect_erase	(ulong addr_first, ulong addr_last);
int flash_sect_protect	(int flag, ulong addr_first, ulong addr_last);
int FlashCompareData( uint BaseAddr, uint SrcAddr, uint SrcSize );
/* common/flash.c */
//void flash_protect (int flag, ulong from, ulong to, flash_info_t *info);
//int flash_write (unsinged char *, ulong, ulong);
//flash_info_t *addr2info (ulong);
//int write_buff (flash_info_t *info, unsinged char *src, ulong addr, ulong cnt);
//void flash_perror(int err);

/*-----------------------------------------------------------------------
 * return codes from flash_write(), flash_erase()
 */
#define ERR_OK				0
#define ERR_TIMOUT			-1
#define ERR_NOT_ERASED			-2
#define ERR_PROTECTED			-3
#define ERR_INVAL			-4
#define ERR_ALIGN			-5
#define ERR_UNKNOWN_FLASH_VENDOR	-6
#define ERR_UNKNOWN_FLASH_TYPE		-7
#define ERR_PROG_ERROR			-8
#define ERR_SIZE				-9
#define ERR_DRAM_DOWNLOAD		-10

/*-----------------------------------------------------------------------
 * Protection Flags for flash_protect():
 */
#define FLAG_PROTECT_SET	0x01
#define FLAG_PROTECT_CLEAR	0x02

/*-----------------------------------------------------------------------
 * Device IDs for AMD and Fujitsu FLASH
 */

#define AMD_MANUFACT	0x00010001	/* AMD     manuf. ID in D23..D16, D7..D0 */
#define FUJ_MANUFACT	0x00040004	/* FUJITSU manuf. ID in D23..D16, D7..D0 */
#define STM_MANUFACT	0x00200020	/* STM (Thomson) manuf. ID in D23.. -"-	*/
#define SST_MANUFACT	0x00BF00BF	/* SST     manuf. ID in D23..D16, D7..D0 */
#define MT_MANUFACT	0x00890089	/* MT      manuf. ID in D23..D16, D7..D0 */
#define INTEL_MANUFACT	0x00890089	/* INTEL   manuf. ID in D23..D16, D7..D0 */
#define	INTEL_ALT_MANU	0x00B000B0	/* alternate INTEL namufacturer ID	*/

					/* Micron Technologies (INTEL compat.)	*/
#define MT_ID_28F400_T	0x44704470	/* 28F400B3 ID ( 4 M, top boot sector)	*/
#define MT_ID_28F400_B	0x44714471	/* 28F400B3 ID ( 4 M, bottom boot sect)	*/

#define AMD_ID_LV040B	0x4F		/* 29LV040B ID				*/
					/* 4 Mbit, 512K x 8,			*/
					/* 8 64K x 8 uniform sectors		*/

#define AMD_ID_F040B	0xA4		/* 29F040B ID				*/
					/* 4 Mbit, 512K x 8,			*/
					/* 8 64K x 8 uniform sectors		*/

#define AMD_ID_F080B	0xD5		/* 29F080  ID  ( 1 M)			*/
#define AMD_ID_F016D	0xAD		/* 29F016  ID  ( 2 M x 8)		*/

#define AMD_ID_LV400T	0x22B922B9	/* 29LV400T ID ( 4 M, top boot sector)	*/
#define AMD_ID_LV400B	0x22BA22BA	/* 29LV400B ID ( 4 M, bottom boot sect)	*/

#define AMD_ID_LV800T	0x22DA22DA	/* 29LV800T ID ( 8 M, top boot sector)	*/
#define AMD_ID_LV800B	0x225B225B	/* 29LV800B ID ( 8 M, bottom boot sect)	*/

#define AMD_ID_LV160T	0x22C422C4	/* 29LV160T ID (16 M, top boot sector)	*/
#define AMD_ID_LV160B	0x22492249	/* 29LV160B ID (16 M, bottom boot sect)	*/

/* 29LV320 device IDs are not yet available */
#define AMD_ID_LV320T	0xDEADBEEF	/* 29LV320T ID (32 M, top boot sector)	*/
#define AMD_ID_LV320B	0xDEADBEEF	/* 29LV320B ID (32 M, bottom boot sect)	*/

#define AMD_ID_DL322T	0x22552255	/* 29DL322T ID (32 M, top boot sector)	*/
#define AMD_ID_DL322B	0x22562256	/* 29DL322B ID (32 M, bottom boot sect)	*/
#define AMD_ID_DL323T	0x22502250	/* 29DL323T ID (32 M, top boot sector)	*/
#define AMD_ID_DL323B	0x22532253	/* 29DL323B ID (32 M, bottom boot sect)	*/
#define AMD_ID_DL324T	0x225C225C	/* 29DL324T ID (32 M, top boot sector)	*/
#define AMD_ID_DL324B	0x225F225F	/* 29DL324B ID (32 M, bottom boot sect) */

#define AMD_ID_DL640	0x227E227E	/* 29DL640D ID (64 M, dual boot sectors)*/
#define AMD_ID_LV640U	0x22D722D7	/* 29LV640U ID (64 M, uniform sectors)	*/

#define SST_ID_xF200A	0x27892789	/* 39xF200A ID ( 2M = 128K x 16	)	*/
#define SST_ID_xF400A	0x27802780	/* 39xF400A ID ( 4M = 256K x 16	)	*/
#define SST_ID_xF800A	0x27812781	/* 39xF800A ID ( 8M = 512K x 16	)	*/
#define SST_ID_xF160A	0x27822782	/* 39xF800A ID (16M =   1M x 16 )	*/

#define STM_ID_x800AB	0x005B005B	/* M29W800AB ID (8M = 512K x 16	)	*/

#define INTEL_ID_28F016S    0x66a066a0	/* 28F016S[VS] ID (16M = 512k x 16)	*/
#define INTEL_ID_28F800B3T  0x88928892	/*  8M = 512K x 16 top boot sector	*/
#define INTEL_ID_28F800B3B  0x88938893	/*  8M = 512K x 16 bottom boot sector	*/
#define INTEL_ID_28F160B3T  0x88908890	/*  16M = 1M x 16 top boot sector	*/
#define INTEL_ID_28F160B3B  0x88918891	/*  16M = 1M x 16 bottom boot sector	*/
#define INTEL_ID_28F320B3T  0x88968896	/*  32M = 2M x 16 top boot sector	*/
#define INTEL_ID_28F320C3T  0x88C488C4	/*  32M = 2M x 16 top boot sector	*/
#define INTEL_ID_28F320B3B  0x88978897	/*  32M = 2M x 16 bottom boot sector	*/
#define INTEL_ID_28F640B3T  0x88988898	/*  64M = 4M x 16 top boot sector	*/
#define INTEL_ID_28F640B3B  0x88998899	/*  64M = 4M x 16 bottom boot sector	*/
#define INTEL_ID_28F320C3		0x88C588C5	/*	4MB , 8 8KB + 63 64KB bbt*/

#define INTEL_ID_28F128J3   0x89189818  /*  16M = 8M x 16 x 128	*/

#define INTEL_ID_28F320JA3  0x00160016	/*  32M = 128K x  32			*/
#define INTEL_ID_28F640JA3  0x00170017	/*  64M = 128K x  64			*/
#define INTEL_ID_28F128JA3  0x00180018	/* 128M = 128K x 128			*/

#define INTEL_ID_28F160S3   0x00D000D0	/*  16M = 512K x  32 (64kB x 32)	*/
#define INTEL_ID_28F320S3   0x00D400D4	/*  32M = 512K x  64 (64kB x 64)	*/

#define INTEL_ID_28F800F3T  0x88F188F1	/*  8M = 512K x 16 top boot sector	*/
#define INTEL_ID_28F800F3B  0x88F288F2	/*  8M = 512K x 16 bottom boot sector	*/
#define INTEL_ID_28F160F3T  0x88F388F3	/*  16M = 1M x 16 top boot sector	*/
#define INTEL_ID_28F160F3B  0x88F488F4	/*  16M = 1M x 16 bottom boot sector	*/

/*-----------------------------------------------------------------------
 * Internal FLASH identification codes
 *
 * Be careful when adding new type! Odd numbers are "bottom boot sector" types!
 */

#define FLASH_AM040	0x0001		/* AMD Am29F040B, Am29LV040B
					 * Bright Micro BM29F040
					 * Fujitsu MBM29F040A
					 * 8 64K x 8 uniform sectors
					 */
#define FLASH_AM400T	0x0002		/* AMD AM29LV400			*/
#define FLASH_AM400B	0x0003
#define FLASH_AM800T	0x0004		/* AMD AM29LV800			*/
#define FLASH_AM800B	0x0005
#define FLASH_AM160T	0x0006		/* AMD AM29LV160			*/
#define FLASH_AM160B	0x0007
#define FLASH_AM320T	0x0008		/* AMD AM29LV320			*/
#define FLASH_AM320B	0x0009

#define FLASH_AMDL322T	0x0010		/* AMD AM29DL322			*/
#define FLASH_AMDL322B	0x0011
#define FLASH_AMDL323T	0x0012		/* AMD AM29DL323			*/
#define FLASH_AMDL323B	0x0013
#define FLASH_AMDL324T	0x0014		/* AMD AM29DL324			*/
#define FLASH_AMDL324B	0x0015

#define FLASH_AMDL640	0x0016		/* AMD AM29DL640D			*/

#define FLASH_SST200A	0x0040		/* SST 39xF200A ID (  2M = 128K x 16 )	*/
#define FLASH_SST400A	0x0042		/* SST 39xF400A ID (  4M = 256K x 16 )	*/
#define FLASH_SST800A	0x0044		/* SST 39xF800A ID (  8M = 512K x 16 )	*/
#define FLASH_SST160A	0x0046		/* SST 39xF160A ID ( 16M =   1M x 16 )	*/

#define FLASH_STM800AB	0x0051		/* STM M29WF800AB  (  8M = 512K x 16 )	*/

#define FLASH_28F400_T	0x0062		/* MT  28F400B3 ID (  4M = 256K x 16 )	*/
#define FLASH_28F400_B	0x0063		/* MT  28F400B3 ID (  4M = 256K x 16 )	*/

#define FLASH_INTEL800T 0x0074		/* INTEL 28F800B3T (  8M = 512K x 16 )	*/
#define FLASH_INTEL800B 0x0075		/* INTEL 28F800B3B (  8M = 512K x 16 )	*/
#define FLASH_INTEL160T 0x0076		/* INTEL 28F160B3T ( 16M =  1 M x 16 )	*/
#define FLASH_INTEL160B 0x0077		/* INTEL 28F160B3B ( 16M =  1 M x 16 )	*/
#define FLASH_INTEL320T 0x0078		/* INTEL 28F320B3T ( 32M =  2 M x 16 )	*/
#define FLASH_INTEL320B 0x0079		/* INTEL 28F320B3B ( 32M =  2 M x 16 )	*/
#define FLASH_INTEL640T 0x007A		/* INTEL 28F320B3T ( 64M =  4 M x 16 )	*/
#define FLASH_INTEL640B 0x007B		/* INTEL 28F320B3B ( 64M =  4 M x 16 )	*/

#define FLASH_28F320JA3 0x007C		/* INTEL 28F320JA3 ( 32M = 128K x  32)	*/
#define FLASH_28F640JA3 0x007D		/* INTEL 28F640JA3 ( 64M = 128K x  64)	*/
#define FLASH_28F128JA3 0x007E		/* INTEL 28F128JA3 (128M = 128K x 128)	*/

#define FLASH_28F008S5	0x0080		/* Intel 28F008S5  (  1M =  64K x 16 )	*/
#define FLASH_28F016SV	0x0081		/* Intel 28F016SV  ( 16M = 512k x 32 )	*/
#define FLASH_28F800_B	0x0083		/* Intel E28F800B  (  1M = ? )		*/
#define FLASH_AM29F800B	0x0084		/* AMD Am29F800BB  (  1M = ? )		*/
#define FLASH_28F320J5	0x0085		/* Intel 28F320J5  (  4M = 128K x 32 )	*/
#define FLASH_28F160S3	0x0086		/* Intel 28F160S3  ( 16M = 512K x 32 )	*/
#define FLASH_28F320S3	0x0088		/* Intel 28F320S3  ( 32M = 512K x 64 )	*/
#define FLASH_28F320C3	0x0089		/* Intel 28F320C3  ( 32M = 64Kx8 + 512x64)*/
#define FLASH_AM640U	0x0090		/* AMD Am29LV640U  ( 64M = 4M x 16 )	*/

#define FLASH_UNKNOWN	0xFFFF		/* unknown flash type			*/


/* manufacturer offsets
 */
#define FLASH_MAN_AMD	0x00000000	/* AMD					*/
#define FLASH_MAN_FUJ	0x00010000	/* Fujitsu				*/
#define FLASH_MAN_BM	0x00020000	/* Bright Microelectronics		*/
#define FLASH_MAN_SST	0x00100000
#define FLASH_MAN_STM	0x00200000
#define FLASH_MAN_INTEL	0x00300000
#define FLASH_MAN_MT	0x00400000


#define FLASH_TYPEMASK	0x0000FFFF	/* extract FLASH type   information	*/
#define FLASH_VENDMASK	0xFFFF0000	/* extract FLASH vendor information	*/

#define FLASH_AMD_COMP	0x000FFFFF	/* Up to this ID, FLASH is compatible	*/
					/* with AMD, Fujitsu and SST		*/
					/* (JEDEC standard commands ?)		*/

#define FLASH_BTYPE	0x0001		/* mask for bottom boot sector type	*/

/*-----------------------------------------------------------------------
 * Timeout constants:
 *
 * We can't find any specifications for maximum chip erase times,
 * so these values are guestimates.
 */
#define FLASH_ERASE_TIMEOUT	120000	/* timeout for erasing in ms		*/
#define FLASH_WRITE_TIMEOUT	500	/* timeout for writes  in ms		*/

/* 20020613 by drsohn */
#define SAMSUNG_MANUFACT	0x00EC00EC	/* SAMSUNG */
#define SAMSUNG_ID_K8B1616UBA		0x22452245
#define FLASH_K8B1616		0x0091


/* drsohn end */
