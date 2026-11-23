#ifndef MYOS_PMM_H
#define MYOS_PMM_H

#include <stdint.h>

/* Physical memory manager: page-granular allocator over Limine's usable
 * regions.  Physical addresses are always uint64_t; never dereference one
 * directly.  Access requires adding the HHDM offset supplied at init.
 *
 * The kernel is single-core today, so no locking is implemented. */
#define PMM_PAGE_SIZE 4096u
#define PMM_USABLE 0u
#define PMM_MAX_REGIONS 32u

struct pmm_region {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct pmm_stats {
    uint64_t total_usable_bytes;
    uint64_t managed_pages;
    uint64_t used_pages;
    uint64_t free_pages;
    uint64_t metadata_pages;
};

/* hhdm_offset is added to physical addresses to obtain accessible pointers.
 * kernel_phys_start/end are excluded even if the memory map calls them usable.
 * Returns 0 on success, -1 on invalid or unusable input. */
int pmm_init(uint64_t hhdm_offset,
             uint64_t kernel_phys_start, uint64_t kernel_phys_end,
             const struct pmm_region *regions, uint64_t region_count);

/* Returns a page-aligned physical address, or 0 on exhaustion/failure. */
uint64_t pmm_alloc_page(void);

/* Returns 0 on success, -1 on invalid or duplicate free. */
int pmm_free_page(uint64_t physical_address);

void pmm_get_stats(struct pmm_stats *stats);
int pmm_initialized(void);

/* Kernel-side diagnostic: allocate pages, write/read patterns through HHDM,
 * verify distinct addresses, free and reallocate. Returns 0 on success.
 * Safe only for mapped, allocatable pages; host tests use a synthetic map. */
int pmm_self_test(void);

#endif
