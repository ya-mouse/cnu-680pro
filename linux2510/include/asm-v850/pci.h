/*
 * include/asm-v850/pci.h -- PCI support
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

#ifndef __V850_PCI_H__
#define __V850_PCI_H__

/* Get any platform-dependent definitions.  */
#include <asm/machdep.h>

/* Generic declarations.  */

struct scatterlist;

extern void pcibios_set_master (struct pci_dev *dev);

/* `Grant' to PDEV the memory block at CPU_ADDR, for doing DMA.  The
   32-bit PCI bus mastering address to use is returned.  the device owns
   this memory until either pci_unmap_single or pci_dma_sync_single is
   performed.  */
extern dma_addr_t
pci_map_single (struct pci_dev *pdev, void *cpu_addr, size_t size, int dir);

/* Return to the CPU the PCI DMA memory block previously `granted' to
   PDEV, at DMA_ADDR.  */
extern void
pci_unmap_single (struct pci_dev *pdev, dma_addr_t dma_addr, size_t size,
		  int dir);

/* Make physical memory consistant for a single streaming mode DMA
   translation after a transfer.

   If you perform a pci_map_single() but wish to interrogate the
   buffer using the cpu, yet do not wish to teardown the PCI dma
   mapping, you must call this function before doing so.  At the next
   point you give the PCI dma address back to the card, the device
   again owns the buffer.  */
extern void
pci_dma_sync_single (struct pci_dev *dev, dma_addr_t dma_addr, size_t size,
		     int dir);


/* Do multiple DMA mappings at once.  */
extern int
pci_map_sg (struct pci_dev *pdev, struct scatterlist *sg, int sg_len, int dir);

/* Unmap multiple DMA mappings at once.  */
extern void
pci_unmap_sg (struct pci_dev *pdev, struct scatterlist *sg, int sg_len,
	      int dir);

/* Allocate and map kernel buffer using consistent mode DMA for PCI
   device.  Returns non-NULL cpu-view pointer to the buffer if
   successful and sets *DMA_ADDR to the pci side dma address as well,
   else DMA_ADDR is undefined.  */
extern void *
pci_alloc_consistent (struct pci_dev *pdev, size_t size, dma_addr_t *dma_addr);

/* Free and unmap a consistent DMA buffer.  CPU_ADDR and DMA_ADDR must
   be values that were returned from pci_alloc_consistent.  SIZE must be
   the same as what as passed into pci_alloc_consistent.  References to
   the memory and mappings assosciated with CPU_ADDR or DMA_ADDR past
   this call are illegal.  */
extern void
pci_free_consistent (struct pci_dev *pdev, size_t size, void *cpu_addr,
		     dma_addr_t dma_addr);

#endif /* __V850_PCI_H__ */
