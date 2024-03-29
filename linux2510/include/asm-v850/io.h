/*
 * include/asm-v850/io.h -- Misc I/O operations
 *
 *  Copyright (C) 2001,02  NEC Corporation
 *  Copyright (C) 2001,02  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#ifndef __V850_IO_H__
#define __V850_IO_H__

#define IO_SPACE_LIMIT 0xFFFFFFFF

#define readb(addr) \
  ({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define readw(addr) \
  ({ unsigned short __v = (*(volatile unsigned short *) (addr)); __v; })
#define readl(addr) \
  ({ unsigned long __v = (*(volatile unsigned long *) (addr)); __v; })

#define writeb(b, addr) \
  (void)((*(volatile unsigned char *) (addr)) = (b))
#define writew(b, addr) \
  (void)((*(volatile unsigned short *) (addr)) = (b))
#define writel(b, addr) \
  (void)((*(volatile unsigned int *) (addr)) = (b))

#define inb(addr)	readb (addr)
#define inw(addr)	readw (addr)
#define inl(addr)	readl (addr)
#define outb(x, addr)	((void) writeb (x, addr))
#define outw(x, addr)	((void) writew (x, addr))
#define outl(x, addr)	((void) writel (x, addr))

#define inb_p(port)		inb((port))
#define outb_p(val, port)	outb((val), (port))
#define inw_p(port)		inw((port))
#define outw_p(val, port)	outw((val), (port))
#define inl_p(port)		inl((port))
#define outl_p(val, port)	outl((val), (port))

static inline void insb (unsigned long port, void *dst, unsigned long count)
{
	unsigned char *p = dst;
	while (count--)
		*p++ = inb (port);
}
static inline void insw (unsigned long port, void *dst, unsigned long count)
{
	unsigned short *p = dst;
	while (count--)
		*p++ = inw (port);
}
static inline void insl (unsigned long port, void *dst, unsigned long count)
{
	unsigned long *p = dst;
	while (count--)
		*p++ = inl (port);
}

static inline void
outsb (unsigned long port, const void *src, unsigned long count)
{
	const unsigned char *p = src;
	while (count--)
		outb (*p++, port);
}
static inline void
outsw (unsigned long port, const void *src, unsigned long count)
{
	const unsigned short *p = src;
	while (count--)
		outw (*p++, port);
}
static inline void
outsl (unsigned long port, const void *src, unsigned long count)
{
	const unsigned long *p = src;
	while (count--)
		outl (*p++, port);
}

#define iounmap(addr)				((void)0)
#define ioremap(physaddr, size)			(physaddr)
#define ioremap_nocache(physaddr, size)		(physaddr)
#define ioremap_writethrough(physaddr, size)	(physaddr)
#define ioremap_fullcache(physaddr, size)	(physaddr)

#define page_to_phys(page)    ((page - mem_map) << PAGE_SHIFT)

#endif /* __V850_IO_H__ */
