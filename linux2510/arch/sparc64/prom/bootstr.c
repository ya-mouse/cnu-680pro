/* $Id: bootstr.c,v 1.1.1.1 2003/11/17 02:33:26 jipark Exp $
 * bootstr.c:  Boot string/argument acquisition from the PROM.
 *
 * Copyright(C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright(C) 1996,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/string.h>
#include <linux/init.h>
#include <asm/oplib.h>

/* WARNING: The boot loader knows that these next three variables come one right
 *          after another in the .data section.  Do not move this stuff into
 *          the .bss section or it will break things.
 */

#define BARG_LEN  256
int bootstr_len = BARG_LEN;
static int bootstr_valid = 0;
static char bootstr_buf[BARG_LEN] = { 0 };

char * __init
prom_getbootargs(void)
{
	/* This check saves us from a panic when bootfd patches args. */
	if (bootstr_valid) return bootstr_buf;
	prom_getstring(prom_chosen_node, "bootargs", bootstr_buf, BARG_LEN);
	bootstr_valid = 1;
	return bootstr_buf;
}
