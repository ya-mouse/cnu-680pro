/****************************************************************************/
/*
 *  linux/mmnommu/page_alloc2.c
 *
 *  	Copyright (C) 2001, 2002 David McCullough <davidm@snapgear.com>
 *
 *	A page allocator that attempts to be better than the
 *	standard power of 2 allocator.
 *
 *	Based on page_alloc.c, see credits in that file.
 *
 *	01/2003 Oleksandr Zhadan <Oleks@ArcturusNetworks.com>
 *	  - Added support for a specific region on top of the memory.
 *          Now it is set to use this region as uncached memory and it is
 *	    ARM940 specific things(ARM940 has no functions to invalidate/clean
 *	    part of the cache, only whole one), but it could be use for any
 *	    user proposal.
 *	    To back to original code just remove everything between
 *	    "#ifdef CONFIG_UNCACHED_MEM" and "#endif"
 *
 */  
/****************************************************************************/

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/module.h>

/****************************************************************************/
/*
 *	do we want nasty stuff checking enabled
 */

#if 0
#define SADISTIC_PAGE_ALLOC 1
#endif

/*
 *	Some accounting stuff
 */
extern unsigned long askedalloc, realalloc;

int nr_swap_pages;
int nr_active_pages;
int nr_inactive_pages;
struct list_head inactive_list;
struct list_head active_list;
pg_data_t *pgdat_list;

#define memlist_init(x) INIT_LIST_HEAD(x)
#define memlist_add_head list_add
#define memlist_add_tail list_add_tail
#define memlist_del list_del
#define memlist_entry list_entry
#define memlist_next(x) ((x)->next)
#define memlist_prev(x) ((x)->prev)

zone_t *zone_table[MAX_NR_ZONES*MAX_NR_NODES];
EXPORT_SYMBOL(zone_table);

static char *zone_names[MAX_NR_ZONES] = { "DMA", "Normal", "HighMem" };

/*
 *	A simple method to save us searching all the reserved kernel
 *	pages every time is to remember where the first free page is
 */

static char *bit_map = NULL;
static int	 bit_map_size = 0;
static int	 first_usable_page = 0;
static int   _nr_free_pages = 0;

unsigned int nr_free_pages() { return _nr_free_pages; }

extern struct wait_queue *buffer_wait;

#define ADDRESS(x) (PAGE_OFFSET + ((x) << PAGE_SHIFT))

#if 1
#define DBG_ALLOC(fmt...)
#else
#define DBG_ALLOC(fmt...) printk(##fmt)
#endif

extern unsigned long __get_contiguous_pages(unsigned int gfp_mask,
			unsigned long num_adjpages, unsigned int align_order);
static void find_some_memory(int n);

#ifdef CONFIG_MEM_MAP
static int mem_map_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data);
#endif

#ifdef __mc68000__
#define ALIGN_ORDER(x)	0
#else
#define ALIGN_ORDER(x)	((x) == 1 ? 1 : 0)
#endif

/****************************************************************************/

#ifdef	CONFIG_UNCACHED_MEM
#include <asm/arch/memory.h>

#define PAGES_IN_A_REGION		(ARM940_REGION_SIZE/0x1000)
#define NUM_REGION			6	// Total number of regions
#define START_OF_SEARCH_REGION		3	// Start of assignable area

#define NOT_ALLOCATED_REGION	0
#define ALLOCATION_DMA		1
#define ALLOCATION_CACHEABLE	2

#define START_LINK_CACHEABLE	0
#define START_LINK_DMA		1

#define SMALL_ALLOC_PAGES 2

struct fixed_arm940_region {
	int               used_pages;
	unsigned int link;	// 0xff when there is no link.
	unsigned int in_use;	// 0: "NOT_ALLOCATED_REGION" for not-in-use,
				// 1: "ALLOCATION_DMA" for dma area,
				// 2: "ALLOCATION_CACHEABLE" for cacheable area,
} fixed_region[NUM_REGION];

struct fixed_arm940_range {
	unsigned int startOfAllocBitMap;
	unsigned int endOfAllocBitMap;
} const fixed_range[NUM_REGION] = {
	{ MAP_NR(REGION0_BEGIN_ADDR), MAP_NR(REGION0_END_ADDR) },	// cacheable area
	{ MAP_NR(REGION1_BEGIN_ADDR), MAP_NR(REGION1_END_ADDR) },	// DMA area
	{ MAP_NR(REGION2_BEGIN_ADDR), MAP_NR(REGION2_END_ADDR) },
	{ MAP_NR(REGION3_BEGIN_ADDR), MAP_NR(REGION3_END_ADDR) },
	{ MAP_NR(REGION4_BEGIN_ADDR), MAP_NR(REGION4_END_ADDR) },
	{ MAP_NR(REGION5_BEGIN_ADDR), MAP_NR(REGION5_END_ADDR) },
};

unsigned int new_allocation_on = 0;
extern unsigned int arm940_change_region(unsigned int region_num, unsigned int region_data);

#ifdef MEM_ALLOC2_DEBUG
void check_data_cache_region()
{
    unsigned long value;

    __asm__ __volatile__(
        "mrc     p15, 0, %0, c1, c0, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 system register : %08lx\n", value);

    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c0, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region0 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c1, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region1 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c2, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region2 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c3, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region3 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c4, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region4 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c5, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region5 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c6, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region6 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c6, c7, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 region7 register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c2, c0, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 cacheable register : %08lx\n", value);
    __asm__ __volatile__(
        "mrc     p15, 0, %0, c3, c0, 0   @ read control reg\n"
        : "=r" (value)
        :
        : "memory");
    printk("cp15 bufferable register : %08lx\n", value);

}

static int
pu15_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	check_data_cache_region();
	return 0;
}
#endif	// MEM_ALLOC2_DEBUG
#endif	/* CONFIG_UNCACHED_MEM */

/****************************************************************************/
#ifdef SADISTIC_PAGE_ALLOC

static void
mem_set(unsigned char *p, unsigned char value, int n)
{
	while (n-- > 0)
		*p++ = value;
}

static void
mem_test(unsigned char *p, unsigned char value, int n)
{
	while (n-- > 0)
		if (*p++ != value)
			break;
	if (n >= 0)
		printk("free memory changed 0x%x, 0x%x != 0x%x\n",
				(unsigned int) p - 1, *(p - 1), value);
}

#endif
/****************************************************************************/

void free_contiguous_pages(unsigned long addr, unsigned int num_adjpages)
{
	unsigned long map_nr = MAP_NR(addr);
	unsigned long flags;

	DBG_ALLOC("%s,%d: %s(0x%x, %d)\n", __FILE__, __LINE__, __FUNCTION__,
			addr, num_adjpages);
	if (map_nr < bit_map_size) {
		int freed = 0;
		mem_map_t *p, *ep;

		p = mem_map + map_nr;

		save_flags(flags);
		cli();

		if (!put_page_testzero(p)) {
			restore_flags(flags);
			return;
		}

		if (PageReserved(p)) /* we never hand out reserved pages */
			BUG();
		if (PageLRU(p))
			lru_cache_del(p);


		if (p->buffers)
			BUG();
		if (p->mapping)
			BUG();
		if (!VALID_PAGE(p))
			BUG();
		if (PageLocked(p))
			BUG();
		if (PageActive(p))
			BUG();

		for (ep = p + num_adjpages; p < ep; p++) {
			p->flags &= ~((1<<PG_referenced) | (1<<PG_dirty));
#ifdef SADISTIC_PAGE_ALLOC
			mem_set((char *) page_address(p), 0xdd, PAGE_SIZE);
#endif
			if (p-mem_map < first_usable_page)
				first_usable_page = p-mem_map;
			clear_bit(p-mem_map, bit_map);
			set_page_count(p, 0);
			freed++;
			_nr_free_pages++;
		}
#ifdef CONFIG_UNCACHED_MEM
		if( addr >= REGION3_BEGIN_ADDR){
			unsigned int link, i;

			if(addr >= REGION1_BEGIN_ADDR)
				link = 1;
			else {
				if(addr >= REGION2_BEGIN_ADDR)
					link = 2;
				else
					link = 3;
				
				fixed_region[link].used_pages -= num_adjpages;
				if(fixed_region[link].used_pages <= 0){
					fixed_region[link].in_use = NOT_ALLOCATED_REGION;
					for(i =0; i < NUM_REGION; i++){
						if( fixed_region[i].link == link ){
							fixed_region[i].link = fixed_region[link].link;
							break;
						}
					}
					// Radicalis_hans_begin (03.12.26) - to fix not used but having link problem
					fixed_region[link].link = 0xff;
					// Radicalis_hans_end (03.12.26) - to fix not used but having link problem
					if(i == NUM_REGION){
						if( new_allocation_on != 0)
						 printk("Pages(0x%x) in invalid region+(i=%d, link=%d) !!!\n",
								addr, i, link);
					}
#ifdef MEM_ALLOC2_DEBUG
					 else printk("Region %d (link of region %d) released\n", link, i);
#endif	//MEM_ALLOC2_DEBUG
				}
			}
		}else{
			unsigned int link, i;

			if(addr < REGION0_END_ADDR)
				link = 0;
			else {
				if(addr < REGION5_END_ADDR)
					link = 5;
				else
					link = 4;

				fixed_region[link].used_pages -= num_adjpages;
				if(fixed_region[link].used_pages <= 0){
					fixed_region[link].in_use = NOT_ALLOCATED_REGION;
					for(i =0; i < NUM_REGION; i++){
						if( fixed_region[i].link == link ){
							fixed_region[i].link = fixed_region[link].link;
							break;
						}
					}
					// Radicalis_hans_begin (03.12.26) - to fix not used but having link problem
					fixed_region[link].link = 0xff;
					// Radicalis_hans_end (03.12.26) - to fix not used but having link problem
					if(i == NUM_REGION){
						if( new_allocation_on != 0)
						 printk("Pages(0x%x) in invalid region-(i=%d, link=%d) !!!\n",
								addr, i, link);
					}
#ifdef MEM_ALLOC2_DEBUG
					else printk("Region %d (link of region %d) released\n", link, i);
#endif	//MEM_ALLOC2_DEBUG
				}
			}
		}
#endif	//CONFIG_UNCACHED_MEM
		restore_flags(flags);

		if (waitqueue_active(&kswapd_wait))
			wake_up_interruptible(&kswapd_wait);
	}
}

/****************************************************************************/
/*
 * Amount of free RAM allocatable as buffer memory:
 */

unsigned int nr_free_buffer_pages (void)
{
	return _nr_free_pages + nr_active_pages + nr_inactive_pages;
}

/****************************************************************************/
/*
 *	We have to keep this interface as some parts of the kernel
 *	source reference them directly
 */

unsigned long get_zeroed_page(unsigned int gfp_mask)
{
	struct page * page;

	page = alloc_pages(gfp_mask, 0);
	if (page) {
		void *address = page_address(page);
		clear_page(address);
		return (unsigned long) address;
	}
	return 0;
}

void free_pages(unsigned long addr, unsigned int order)
{
	DBG_ALLOC("%s,%d: %s(0x%x, %d)\n", __FILE__, __LINE__, __FUNCTION__,
			addr, order);
	if (addr != 0)
		__free_pages(virt_to_page(addr), order);
}

void __free_pages(struct page *page, unsigned int order)
{
	int flags;

	DBG_ALLOC("%s,%d: %s(0x%x[0x%x], %d)\n", __FILE__, __LINE__,
			__FUNCTION__, page, page_address(page), order);

	if (!PageReserved(page))
		free_contiguous_pages((unsigned long) page_address(page), 1 << order);
}

struct page *_alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	unsigned long addr;
	DBG_ALLOC("%s,%d: %s(0x%x, %d)\n", __FILE__, __LINE__, __FUNCTION__,
			gfp_mask, order);
	addr = __get_contiguous_pages(gfp_mask, 1 << order, ALIGN_ORDER(order));
	if (addr)
		return(virt_to_page(addr));
	return(NULL);
}

struct page * __alloc_pages(unsigned int gfp_mask, unsigned int order, zonelist_t *zonelist)
{
	unsigned long addr;
	DBG_ALLOC("%s,%d: %s(0x%x, %d)\n", __FILE__, __LINE__, __FUNCTION__,
			gfp_mask, order);
	addr = __get_contiguous_pages(gfp_mask, 1 << order, ALIGN_ORDER(order));
	if (addr)
		return(virt_to_page(addr));
	return(NULL);
}

/****************************************************************************/

static void find_some_memory(int n)
{
	int loops = 0, i;
	pg_data_t * pgdat;
	zone_t * zone;

	if (in_interrupt()) /* sorry, you lose */
		return;

	do {
		pgdat = pgdat_list;
		do {
			for (i = pgdat->nr_zones-1; i >= 0; i--) {
				zone = pgdat->node_zones + i;
				zone->need_balance = 1;
				try_to_free_pages_zone(zone, GFP_KSWAPD);
			}
		} while ((pgdat = pgdat->node_next));
	} while (loops++ < n);
}

/****************************************************************************/
/*
 *	look through the map for a run of consecutive pages that will
 *	hold a # of pages
 */

unsigned long
__get_contiguous_pages(
	unsigned int gfp_mask,
	unsigned long num_adjpages,
	unsigned int align_order)
{
	unsigned long	 flags;
	mem_map_t		*p;
	int              repeats = 0;
	pg_data_t		*pgdat;
	zone_t			*zone;

#ifdef CONFIG_UNCACHED_MEM
	unsigned int	gLink, link;
	unsigned int	startOfAllocBitMap, endOfAllocBitMap;
#endif
	DBG_ALLOC("%s,%d: %s(0x%x, %d, %d) - mem_map=0x%x\n", __FILE__, __LINE__,
			__FUNCTION__, gfp_mask, num_adjpages, align_order, mem_map);
	save_flags(flags);

	if (waitqueue_active(&kswapd_wait))
		wake_up_interruptible(&kswapd_wait);

repeat:
	cli();
/*
 *	Don't bother trying to find pages unless there are enough
 *	for the given context
 */
	if (num_adjpages <= _nr_free_pages) {

		int n = 0, little_alloc = 0, ff;

		p = NULL;
		
#ifdef CONFIG_UNCACHED_MEM
	if(gfp_mask & GFP_DMA){
		gLink = START_LINK_DMA;			// dma region
	}
	else {
		gLink = START_LINK_CACHEABLE;		// cacheable region
		startOfAllocBitMap = first_usable_page;
		goto scan_again2;
	}
scan_again:
	startOfAllocBitMap = fixed_range[gLink].startOfAllocBitMap;
scan_again2:
	endOfAllocBitMap = fixed_range[gLink].endOfAllocBitMap;

	if (num_adjpages <= SMALL_ALLOC_PAGES){
		little_alloc = endOfAllocBitMap;

		ff = find_next_zero_bit(bit_map, 0, little_alloc -= 8);
#if 0	//hans test
		while (little_alloc > startOfAllocBitMap) {
#else
		while (little_alloc >= startOfAllocBitMap) {
#endif
			if (ff + num_adjpages <= endOfAllocBitMap) {
				p = mem_map + ff;
				if (((unsigned long) page_address(p)) &
						((PAGE_SIZE << align_order) - 1))
					n = 0;
				else
					for (n = 0; n < num_adjpages; n++, p++) {
						if (test_bit(p-mem_map, bit_map))
							break;
					}
				if (n >= num_adjpages)
					break;
			}
			ff = find_next_zero_bit(bit_map, 0, little_alloc -= 8);
		}
	} else {
		ff = find_next_zero_bit(bit_map, endOfAllocBitMap, startOfAllocBitMap);
		while (ff + num_adjpages <= endOfAllocBitMap) {
			p = mem_map + ff;
			if (((unsigned long) page_address(p)) &
					((PAGE_SIZE << align_order) - 1))
				n = 0;
			else
				for (n = 0; n < num_adjpages; n++, p++) {
					if (test_bit(p-mem_map, bit_map))
						break;
				}
			if (n >= num_adjpages)
				break;
			ff = find_next_zero_bit(bit_map, endOfAllocBitMap, (ff + n + 1));
		}
	}
#endif	// CONFIG_UNCACHED_MEM
		if (p && n >= num_adjpages) {
#ifdef CONFIG_UNCACHED_MEM
			fixed_region[gLink].used_pages += num_adjpages;
#endif	// CONFIG_UNCACHED_MEM
			_nr_free_pages -= num_adjpages;
			while (n-- > 0) {
				p--;
#ifdef SADISTIC_PAGE_ALLOC
				if (atomic_read(&p->count))
					printk("allocated a non-free page\n");
#endif
				set_page_count(p, 1);
				set_bit(p-mem_map, bit_map);
				p->index = 0xa1c20000 | num_adjpages;
				if (num_adjpages > 0xffff)
					BUG();
			}
#ifdef SADISTIC_PAGE_ALLOC
			mem_test((char *) page_address(p), 0xdd, num_adjpages * PAGE_SIZE);
			mem_set((char *) page_address(p), 0xcc, num_adjpages * PAGE_SIZE);
#endif
			DBG_ALLOC(" return(0x%x[p=0x%x])\n", page_address(p), p);
			pgdat = pgdat_list;
			do { /* try and keep memory freed */
				int i;
				for (i = pgdat->nr_zones-1; i >= 0; i--) {
					zone = pgdat->node_zones + i;
					zone->need_balance = 1;
				}
			} while ((pgdat = pgdat->node_next));
			restore_flags(flags);

			return((unsigned long) page_address(p));
		}
#ifdef CONFIG_UNCACHED_MEM
	else {
scan_region_link:
		link = fixed_region[gLink].link;
		if(link > NUM_REGION){	
			unsigned int i;

			// no assigned region
			if(gfp_mask & GFP_DMA){
				for(i =START_OF_SEARCH_REGION-1; i < NUM_REGION; i++){
					if( ! fixed_region[i].in_use ) break;
				}
			} else {
				for(i =NUM_REGION -1; i >= START_OF_SEARCH_REGION -1; i--){
					if( ! fixed_region[i].in_use ) break;
				}
			}
			if((i >= NUM_REGION) || (i < (START_OF_SEARCH_REGION-1))){
				// no more memory region;
				if(fixed_region[gLink].in_use == ALLOCATION_CACHEABLE){
					// Search in DMA area
					gLink = START_LINK_DMA;

					goto scan_again;
				}
				// No more memory at all;
			}else{
				new_allocation_on = 1;

				// now we got a empty memory region.
				fixed_region[gLink].link = i;
				fixed_region[i].in_use = fixed_region[gLink].in_use;
				fixed_region[i].used_pages = 0;
				fixed_region[i].link = 0xff;
				gLink = i;

#ifdef MEM_ALLOC2_DEBUG
				check_data_cache_region();
				printk("[mem alloc] (fixed_region[%d].in_use-1)<<(%d+1) =0x%x ",
						i, i, (fixed_region[i].in_use-1)<<(i+1));
				i = arm940_change_region(i, (fixed_region[i].in_use-1));
				printk("change-result(%d): ", i);
				switch(i){
				case 0:	printk("not changed\n"); break;
				case 1:	printk("went to cacheable\n"); break;
				case 2: printk("went to dma region\n"); break;
				default: printk("invalid setting\n");
				}
				check_data_cache_region();
#else	//MEM_ALLOC2_DEBUG
				arm940_change_region(i, (fixed_region[i].in_use-1));
#endif	//MEM_ALLOC2_DEBUG
				goto scan_again;
			}
		}else{
			// there is allocated region
			gLink = link;
			if(PAGES_IN_A_REGION > fixed_region[gLink].used_pages + num_adjpages){
				// enough memory in this region
				goto scan_again;
			} else {
				// not enough memory in this region
				goto scan_region_link;
			}
		}
	}
#endif	// CONFIG_UNCACHED_MEM
	}
	restore_flags(flags);
	if ((current->flags & PF_MEMALLOC) == 0) {
		find_some_memory(3);
		if (repeats++ < 3)
			goto repeat;
		printk("%s: allocation of %d pages failed!\n", current->comm,
				(int) num_adjpages);
#ifdef CONFIG_MEM_MAP
		mem_map_read_proc(NULL, NULL, 0, 0, 0, 0);
#endif
		out_of_memory(); /* call this too often and we panic */
	}
	return(0);
}

/****************************************************************************/
/*
 *	as for free_pages,  we have to provide this one as well
 */

unsigned long __get_free_pages(unsigned int gfp_mask, unsigned int order)
{
	DBG_ALLOC("%s,%d: %s(0x%x, %d)\n", __FILE__, __LINE__, __FUNCTION__,
			gfp_mask, order);
	return(__get_contiguous_pages(gfp_mask, 1 << order, ALIGN_ORDER(order)));
}

/****************************************************************************/
/*
 *	dump some stats on how we are doing
 */

#define PRINTK(a...) (buffer ? (len+=sprintf(buffer+len, a)) : printk(a))
#define FIXUP(t)	if (buffer && len >= count - 80) goto t; else

static int
print_free_areas(char *buffer, int count)
{
	int				len = 0;
	mem_map_t		*p, *ep;
 	unsigned long	flags, slack;
 	unsigned long	min_free = bit_map_size * PAGE_SIZE;
 	unsigned long	min_used = bit_map_size * PAGE_SIZE;
	unsigned long	max_free=0, avg_free=0, free_blks=0;
	unsigned long	max_used=0, avg_used=0, used_blks=0;

	find_some_memory(1);

	if (realalloc)
		slack = (realalloc-askedalloc) * 100 / realalloc;
	else
		slack = 0;
	
	save_flags(flags);
	cli();

	FIXUP(got_data);

 	for (p = mem_map, ep = p + bit_map_size; p < ep; ) {
		int n;
		
		n = 0;

		if (test_bit(p-mem_map, bit_map)) {
			while (p < ep && test_bit(p-mem_map, bit_map)) {
				n++;
				p++;
			}
			avg_used += n;
			if (n < min_used)
				min_used = n;
			if (n > max_used)
				max_used = n;
			used_blks++;
		} else {
			while (p < ep && !test_bit(p-mem_map, bit_map)) {
				n++;
				p++;
			}
			avg_free += n;
			if (n < min_free)
				min_free = n;
			if (n > max_free)
				max_free = n;
			free_blks++;
		}
	}
	
	PRINTK("Active: %d, inactive: %d, free: %d\n",
	       nr_active_pages, nr_inactive_pages, nr_free_pages());
	FIXUP(got_data);
	PRINTK("Free pages:%8d (%dkB), %%%lu frag, %%%lu slack\n",
			_nr_free_pages, _nr_free_pages << (PAGE_SHIFT-10),
			(free_blks * 100) / _nr_free_pages, slack);
	FIXUP(got_data);
	PRINTK("Free blks: %8lu min=%lu max=%lu avg=%lu\n",
			free_blks, min_free, max_free, avg_free / free_blks);
	FIXUP(got_data);
	PRINTK("Used blks: %8lu min=%lu max=%lu avg=%lu\n",
			used_blks, min_used, max_used, avg_used / used_blks);
	FIXUP(got_data);

got_data:
	restore_flags(flags);
	return(len);
}

#undef FIXUP
#undef PRINTK
/****************************************************************************/

void
show_free_areas(void)
{
	(void) print_free_areas(NULL, 0);
#if defined(CONFIG_PROC_FS) && defined(CONFIG_MEM_MAP)
	(void) mem_map_read_proc(NULL, NULL, 0, 0, NULL, NULL);
#endif
}

/****************************************************************************/

/*
 * Builds allocation fallback zone lists.
 */
static inline void build_zonelists(pg_data_t *pgdat)
{
	int i, j, k;

	for (i = 0; i <= GFP_ZONEMASK; i++) {
		zonelist_t *zonelist;
		zone_t *zone;

		zonelist = pgdat->node_zonelists + i;
		memset(zonelist, 0, sizeof(*zonelist));

		j = 0;
		k = ZONE_NORMAL;
		if (i & __GFP_HIGHMEM)
			k = ZONE_HIGHMEM;
		if (i & __GFP_DMA)
			k = ZONE_DMA;

		switch (k) {
			default:
				BUG();
			/*
			 * fallthrough:
			 */
			case ZONE_HIGHMEM:
				zone = pgdat->node_zones + ZONE_HIGHMEM;
				if (zone->size) {
#ifndef CONFIG_HIGHMEM
					BUG();
#endif
					zonelist->zones[j++] = zone;
				}
			case ZONE_NORMAL:
				zone = pgdat->node_zones + ZONE_NORMAL;
				if (zone->size)
					zonelist->zones[j++] = zone;
			case ZONE_DMA:
				zone = pgdat->node_zones + ZONE_DMA;
				if (zone->size)
					zonelist->zones[j++] = zone;
		}
		zonelist->zones[j++] = NULL;
	} 
}

/****************************************************************************/
/*
 * Helper functions to size the waitqueue hash table.
 * Essentially these want to choose hash table sizes sufficiently
 * large so that collisions trying to wait on pages are rare.
 * But in fact, the number of active page waitqueues on typical
 * systems is ridiculously low, less than 200. So this is even
 * conservative, even though it seems large.
 *
 * The constant PAGES_PER_WAITQUEUE specifies the ratio of pages to
 * waitqueues, i.e. the size of the waitq table given the number of pages.
 */

#define PAGES_PER_WAITQUEUE	256

static inline unsigned long wait_table_size(unsigned long pages)
{
	unsigned long size = 1;

	pages /= PAGES_PER_WAITQUEUE;

	while (size < pages)
		size <<= 1;

	/*
	 * Once we have dozens or even hundreds of threads sleeping
	 * on IO we've got bigger problems than wait queue collision.
	 * Limit the size of the wait table to a reasonable size.
	 */
	size = min(size, 4096UL);

	return size;
}

/*
 * This is an integer logarithm so that shifts can be used later
 * to extract the more random high bits from the multiplicative
 * hash function before the remainder is taken.
 */
static inline unsigned long wait_table_bits(unsigned long size)
{
	return ffz(~size);
}


#define LONG_ALIGN(x) (((x)+(sizeof(long))-1)&~((sizeof(long))-1))

/****************************************************************************/
/*
 * Set up the zone data structures:
 *   - mark all pages reserved
 *   - mark all memory queues empty
 *   - clear the memory bitmaps
 *
 * static in this version because I haven't though it out yet ;-)
 */

void __init free_area_init_core(int nid, pg_data_t *pgdat, struct page **gmap,
	unsigned long *zones_size, unsigned long zone_start_paddr, 
	unsigned long *zholes_size, struct page *lmem_map)
{
	unsigned long i, j;
	unsigned long map_size;
	unsigned long totalpages, offset, realtotalpages;
	const unsigned long zone_required_alignment = 1UL << (MAX_ORDER-1);

	if (zone_start_paddr & ~PAGE_MASK)
		BUG();

	totalpages = 0;
	for (i = 0; i < MAX_NR_ZONES; i++) {
		unsigned long size = zones_size[i];
		totalpages += size;
	}
	realtotalpages = totalpages;
	if (zholes_size)
		for (i = 0; i < MAX_NR_ZONES; i++)
			realtotalpages -= zholes_size[i];
			
	printk("On node %d totalpages: %lu\n", nid, realtotalpages);

	INIT_LIST_HEAD(&active_list);
	INIT_LIST_HEAD(&inactive_list);

	/*
	 * Some architectures (with lots of mem and discontinous memory
	 * maps) have to search for a good mem_map area:
	 * For discontigmem, the conceptual mem map array starts from 
	 * PAGE_OFFSET, we need to align the actual array onto a mem map 
	 * boundary, so that MAP_NR works.
	 */
	map_size = (totalpages + 1)*sizeof(struct page);
	if (lmem_map == (struct page *)0) {
		lmem_map = (struct page *) alloc_bootmem_node(pgdat, map_size);
		lmem_map = (struct page *)(PAGE_OFFSET + 
			MAP_ALIGN((unsigned long)lmem_map - PAGE_OFFSET));
	}
	*gmap = pgdat->node_mem_map = lmem_map;
	pgdat->node_size = totalpages;
	pgdat->node_start_paddr = zone_start_paddr;
	pgdat->node_start_mapnr = (lmem_map - mem_map);
	pgdat->nr_zones = 0;

	/*
	 *	as we free pages we mark the first page that is usable
	 */
	bit_map_size = totalpages;
	bit_map = (unsigned char *)
			alloc_bootmem_node(pgdat, LONG_ALIGN(bit_map_size / 8));
	memset(bit_map, 0, LONG_ALIGN(bit_map_size / 8));

	/*
	 * Initially all pages are reserved - free ones are freed
	 * up by free_all_bootmem() once the early boot process is
	 * done.
	 */

	first_usable_page = totalpages;

	offset = lmem_map - mem_map;	
	for (j = 0; j < MAX_NR_ZONES; j++) {
		zone_t *zone = pgdat->node_zones + j;
		unsigned long size, realsize;

		zone_table[nid * MAX_NR_ZONES + j] = zone;
		realsize = size = zones_size[j];
		if (zholes_size)
			realsize -= zholes_size[j];

		printk("zone(%lu): %lu pages.\n", j, size);
		zone->size = size;
		zone->name = zone_names[j];
		zone->lock = SPIN_LOCK_UNLOCKED;
		zone->zone_pgdat = pgdat;
		zone->free_pages = 0;
		zone->need_balance = 0;
		if (!size)
			continue;

		/*
		 * The per-page waitqueue mechanism uses hashed waitqueues
		 * per zone.
		 */
		zone->wait_table_size = wait_table_size(size);
		zone->wait_table_shift =
				BITS_PER_LONG - wait_table_bits(zone->wait_table_size);
		zone->wait_table = (wait_queue_head_t *)
				alloc_bootmem_node(pgdat, zone->wait_table_size
						* sizeof(wait_queue_head_t));

		for (i = 0; i < zone->wait_table_size; ++i)
			init_waitqueue_head(zone->wait_table + i);

		pgdat->nr_zones = j+1;

		zone->pages_min = 0;
		zone->pages_low = 0;
		zone->pages_high = realsize; /* very agressive, always free pages */

		zone->zone_mem_map = mem_map + offset;
		zone->zone_start_mapnr = offset;
		zone->zone_start_paddr = zone_start_paddr;

		if ((zone_start_paddr >> PAGE_SHIFT) & (zone_required_alignment-1))
			printk("BUG: wrong zone alignment, it will crash\n");

		for (i = 0; i < size; i++) {
			struct page *page = mem_map + offset + i;
			set_page_zone(page, nid * MAX_NR_ZONES + j);
			set_page_count(page, 0);
			SetPageReserved(page);
			set_bit(page-mem_map, bit_map);
			INIT_LIST_HEAD(&page->list);
			if (j != ZONE_HIGHMEM)
				set_page_address(page, __va(zone_start_paddr));
			zone_start_paddr += PAGE_SIZE;
		}

		offset += size;
	}
	build_zonelists(pgdat);

#ifdef CONFIG_UNCACHED_MEM
	for(i = 0; i < NUM_REGION; i ++){
		fixed_region[i].used_pages =0;
		fixed_region[i].link = 0xff;
		fixed_region[i].in_use = NOT_ALLOCATED_REGION;
	}
	fixed_region[0].in_use = ALLOCATION_CACHEABLE;
	fixed_region[1].in_use = ALLOCATION_DMA;
#endif
}

/****************************************************************************/

void __init free_area_init(unsigned long *zones_size)
{
	free_area_init_core(0, &contig_page_data, &mem_map, zones_size, 0, 0, 0);
}

/****************************************************************************/
#if defined(CONFIG_PROC_FS) && defined(CONFIG_MEM_MAP)
/****************************************************************************/
/*
 *	A small tool to help debug/display memory allocation problems
 *	Creates /proc/mem_map,  an ascii representation of what each
 *	page in memory is being used for.  It displays the address of the
 *	memory down the left column and 64 pages per line (ie., 256K).
 *
 *	If you want better reporting,  define MEGA_HACK below and then
 *	find all the referenced FS routines in the kernel and remove static
 *	from their definition (see page_alloc2.hack for patch).
 *
 *	Obviously this code needs proc_fs,  but it is trivial to make it
 *	use printk and always include it.
 *
 *	KEY:
 *
 *	  Normal letters
 *	  --------------
 *		-         free
 *	    R         reserved (usually the kernel/mem_map/bitmap)
 *		X         owned by a device/fs (see MEGA_HACK code)
 *		S         swap cache
 *		L         locked
 *		A         Active
 *		U         LRU
 *		s         owned by the slab allocator
 *		r         referenced
 *		C         non zero count
 *		?         who knows ?
 *
 *	  Contiguous Page Alloc
 *	  ---------------------
 *		1         a single page_alloc2 page
 *		[=*]      contigous pages allocated by page_alloc2
 *
 *	  MEGA HACK values
 *	  ---------------------
 *		*         ram disk
 *		#         romfs
 *		M         minix
 *		%         ext2
 *		B         block dev (cache etc)
 *
 *	TODO:
 *	   print process name for contiguous blocks
 */  
/****************************************************************************/

#if 0
#define MEGA_HACK 1
#endif

/****************************************************************************/

#define PRINTK(a...) (page ? (len += sprintf(page + len, a)) : printk(a))

#define FIXUP(t)				\
	if (page) {					\
		if (len <= off) {		\
			off -= len;		\
			len = 0;		\
		} else {			\
			if (len-off > count - 80)	\
				goto t;		\
		}				\
	} else

static int
mem_map_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = 0;
	struct page *p, *ep;
	int cols;
	int flags;

	save_flags(flags);
	cli();

	FIXUP(got_data);

	cols = 0;
 	for (p = mem_map, ep = p + bit_map_size; p < ep; p++) {
#ifdef CONFIG_MEM_MAP_MEGA_HACK
		extern int blkdev_readpage(struct file *, struct page *);
# ifdef CONFIG_BLK_DEV_RAM
		extern int ramdisk_readpage(struct file *, struct page *);
# endif
# ifdef CONFIG_ROMFS_FS
		extern int romfs_readpage(struct file *, struct page *);
# endif
# ifdef CONFIG_EXT2_FS
		extern int ext2_readpage(struct file *, struct page *);
# endif
# ifdef CONFIG_MINIX_FS
		extern int minix_readpage(struct file *, struct page *);
# endif

#if 0  // choish_shared_lib_porting		
# ifdef CONFIG_CRAMFS
		extern int cramfs_readpage(struct file *, struct page *);
# endif
# ifdef CONFIG_RAMFS
		extern int ramfs_readpage(struct file *, struct page *);
# endif
#endif
#endif /* CONFIG_MEM_MAP_MEGA_HACK */
		
		if (cols == 0)
			PRINTK("0x%08x: ",(unsigned)page_address(p));
		if (test_bit(p-mem_map, bit_map)) {
			if (PageReserved(p))
				PRINTK("R");
			else if (p->mapping && p->mapping->a_ops) {
#ifdef CONFIG_MEM_MAP_MEGA_HACK
				if (p->mapping->a_ops->readpage == blkdev_readpage)
					PRINTK("B");
				else
# ifdef CONFIG_BLK_DEV_RAM
				if (p->mapping->a_ops->readpage == ramdisk_readpage)
					PRINTK("*");
				else
# endif
# ifdef CONFIG_ROMFS_FS
				if (p->mapping->a_ops->readpage == romfs_readpage)
					PRINTK("#");
				else
# endif
# ifdef CONFIG_MINIX_FS
				if (p->mapping->a_ops->readpage == minix_readpage)
					PRINTK("M");
				else
# endif
# ifdef CONFIG_EXT2_FS
				if (p->mapping->a_ops->readpage == ext2_readpage)
					PRINTK("%");
				else
# endif
#if 0 choish_shared_lib_porting
# ifdef CONFIG_CRAMFS
				if (p->mapping->a_ops->readpage == cramfs_readpage)
					PRINTK("@");
				else
# endif
# ifdef CONFIG_RAMFS
				if (p->mapping->a_ops->readpage == ramfs_readpage)
					PRINTK("$");
				else
# endif
#endif
#endif
					PRINTK("X");
			} else if (PageSwapCache(p))
				PRINTK("S");
			else if (PageLocked(p))
				PRINTK("L");
			else if (PageActive(p))
				PRINTK("A");
			else if (PageLRU(p))
				PRINTK("U");
			else if (PageSlab(p))
				PRINTK("s");
			else if (p->flags & (1<<PG_referenced))
				PRINTK("r");
			else if (atomic_read(&p->count)) {
#ifdef CONFIG_CONTIGUOUS_PAGE_ALLOC
				if ((p->index & ~0xffff) == 0xa1c20000) {
					if ((p->index & 0xffff) == 1)
						PRINTK("1");
					else {
						int i = p->index & 0xffff;
						PRINTK("["); p++; i--; cols++;
						if (cols >= 64) {
							PRINTK("\n");
							cols = 0;
							FIXUP(got_data);
						}
						while (i > 1) {
							if (cols == 0)
								PRINTK("0x%08x: ",(unsigned)page_address(p));
							PRINTK("="); p++; i--; cols++;
							if (cols >= 64) {
								PRINTK("\n");
								cols = 0;
								FIXUP(got_data);
							}
						}
						if (cols == 0)
							PRINTK("0x%08x: ",(unsigned)page_address(p));
						PRINTK("]");
					}
				} else
#endif
					PRINTK("C");
			} else
				PRINTK("?");
		} else
			PRINTK("-");
		cols++;
		if (cols >= 64) {
			PRINTK("\n");
			cols = 0;
			FIXUP(got_data);
		}
	}
	if (cols)
		PRINTK("\n");
	FIXUP(got_data);
	PRINTK("\n");
	FIXUP(got_data);

{
	unsigned long total_bytes = 0, total_sbytes = 0, total_slack = 0;
	struct task_struct *p;

	for_each_task(p) {
		struct mm_struct *mm = p->mm;
		unsigned long bytes = 0, sbytes = 0, slack = 0;
		struct mm_tblock_struct * tblock;

		if (!mm)
			continue;
        
		for (tblock = &mm->tblock; tblock; tblock = tblock->next) {
			if (tblock->rblock) {
				bytes += ksize(tblock);
				if (atomic_read(&mm->mm_count) > 1 ||
						tblock->rblock->refcount > 1) {
					sbytes += ksize(tblock->rblock->kblock);
					sbytes += ksize(tblock->rblock) ;
				} else {
					bytes += ksize(tblock->rblock->kblock);
					bytes += ksize(tblock->rblock) ;
					slack += ksize(tblock->rblock->kblock) - tblock->rblock->size;
				}
			}
		}
		
		((atomic_read(&mm->mm_count) > 1) ? sbytes : bytes)
				+= ksize(mm);
		(current->fs && atomic_read(&current->fs->count) > 1 ? sbytes : bytes)
				+= ksize(current->fs);
		(current->files && atomic_read(&current->files->count) > 1 ? sbytes : bytes)
				+= ksize(current->files);
		(current->sig && atomic_read(&current->sig->count) > 1 ? sbytes : bytes)
				+= ksize(current->sig);
		bytes += ksize(current); /* includes kernel stack */

		PRINTK("%-16s Mem:%8lu Slack:%8lu Shared:%8lu\n", p->comm, bytes,
				slack, sbytes);
		FIXUP(got_data);
		total_slack += slack;
		total_sbytes += sbytes;
		total_bytes += bytes;
	}
	PRINTK("%-16s Mem:%8lu Slack:%8lu Shared:%8lu\n\n", "Total", total_bytes,
				total_slack, total_sbytes);
	FIXUP(got_data);
}

	len += print_free_areas(page + len, count - len);
	FIXUP(got_data);

got_data:
	restore_flags(flags);
	
	if (page) {
		*start = page+off;

		len -= (*start-page);
		if (len <= count - 80)
			*eof = 1;
		if (len>count) len = count;
		if (len<0) len = 0;
	}
	return(len);
}

#undef FIXUP
#undef PRINTK
/****************************************************************************/

static __init int
page_alloc2_init(void)
{
	create_proc_read_entry("mem_map", S_IWUSR | S_IRUGO, NULL,
			mem_map_read_proc, NULL);
#ifdef	CONFIG_UNCACHED_MEM
#ifdef	MEM_ALLOC2_DEBUG
	create_proc_read_entry("pu_map", S_IWUSR | S_IRUGO, NULL,
			pu15_read_proc, NULL);
#endif	// MEM_ALLOC2_DEBUG
#endif	/* CONFIG_UNCACHED_MEM */
	return(0);
}

/****************************************************************************/

module_init(page_alloc2_init);

/****************************************************************************/
#endif /* CONFIG_PROC_FS && CONFIG_MEM_MAP */
/****************************************************************************/
