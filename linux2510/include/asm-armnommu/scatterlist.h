#ifndef _ASMARM_SCATTERLIST_H
#define _ASMARM_SCATTERLIST_H

#include <asm/types.h>

struct scatterlist {
	char		*address;	/* virtual address		 */
	char		*alt_address;	/* indirect dma address, or NULL */
	struct page 	*page;          /* coreyliu 20040202, for making success compile and according old 2.4.17 version */
	unsigned int    offset;         /* coreyliu 20040202, for making success compile and according 2.4.20 kernel */
	dma_addr_t	dma_address;	/* dma address			 */
	unsigned int	length;		/* length			 */
};

/*
 * These macros should be used after a pci_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns, or alternatively stop on the first sg_dma_len(sg) which
 * is 0.
 */
#define sg_dma_address(sg)      ((sg)->dma_address)
#define sg_dma_len(sg)          ((sg)->length)

#define ISA_DMA_THRESHOLD (0xffffffff)

#endif /* _ASMARM_SCATTERLIST_H */
