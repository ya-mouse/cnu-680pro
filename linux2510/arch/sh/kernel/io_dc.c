/*
 *	$Id: io_dc.c,v 1.1.1.1 2003/11/17 02:33:22 jipark Exp $
 *	I/O routines for SEGA Dreamcast
 */

#include <asm/io.h>
#include <asm/machvec.h>

unsigned long dreamcast_isa_port2addr(unsigned long offset)
{
	return offset + 0xa0000000;
}
