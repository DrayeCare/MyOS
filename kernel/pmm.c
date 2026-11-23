#include <stdint.h>

#include "pmm.h"

struct pmm_span {
    uint64_t base;
    uint64_t pages;
    uint64_t first;
};

struct pmm_range {
    uint64_t start;
    uint64_t end;
};

static struct pmm_span spans[PMM_MAX_REGIONS];
static struct pmm_range reserved[PMM_MAX_REGIONS + 4];
static unsigned span_count;
static unsigned reserved_count;

static uint8_t *bitmap;
static uint64_t bitmap_bytes;
static uint64_t metadata_pages;
static uint64_t total_pages;
static uint64_t used_pages;
static uint64_t total_usable_bytes;
static uint64_t hhdm_offset_saved;
static int ready;

static uint64_t align_up(uint64_t value) {
    return (value + PMM_PAGE_SIZE - 1) & ~(uint64_t)(PMM_PAGE_SIZE - 1);
}

static uint64_t align_down(uint64_t value) {
    return value & ~(uint64_t)(PMM_PAGE_SIZE - 1);
}

static void set_bit(uint64_t index) {
    bitmap[index >> 3] |= (uint8_t)(1u << (index & 7));
}

static void clear_bit(uint64_t index) {
    bitmap[index >> 3] &= (uint8_t)~(1u << (index & 7));
}

static int bit_is_set(uint64_t index) {
    return (bitmap[index >> 3] & (uint8_t)(1u << (index & 7))) != 0;
}

static void add_reserved(uint64_t start, uint64_t end) {
    if (end <= start || reserved_count >= PMM_MAX_REGIONS + 4)
        return;

    reserved[reserved_count].start = start;
    reserved[reserved_count].end = end;
    reserved_count++;
}

static int is_reserved(uint64_t physical) {
    for (unsigned i = 0; i < reserved_count; i++) {
        if (physical >= reserved[i].start && physical < reserved[i].end)
            return 1;
    }
    return 0;
}

static uint64_t phys_for_index(uint64_t index) {
    for (unsigned i = 0; i < span_count; i++) {
        uint64_t end = spans[i].first + spans[i].pages;
        if (index >= spans[i].first && index < end) {
            return spans[i].base +
                   (index - spans[i].first) * PMM_PAGE_SIZE;
        }
    }
    return 0;
}

static uint64_t index_for_phys(uint64_t physical) {
    for (unsigned i = 0; i < span_count; i++) {
        uint64_t end = spans[i].base + spans[i].pages * PMM_PAGE_SIZE;
        if (physical >= spans[i].base && physical < end) {
            return spans[i].first +
                   (physical - spans[i].base) / PMM_PAGE_SIZE;
        }
    }
    return UINT64_MAX;
}

int pmm_init(uint64_t hhdm_offset,
             uint64_t kernel_phys_start, uint64_t kernel_phys_end,
             const struct pmm_region *regions, uint64_t region_count) {
    ready = 0;

    if (regions == 0 || region_count == 0 || region_count > PMM_MAX_REGIONS)
        return -1;

    span_count = 0;
    reserved_count = 0;
    total_pages = 0;
    used_pages = 0;
    total_usable_bytes = 0;
    bitmap = 0;
    hhdm_offset_saved = hhdm_offset;

    for (uint64_t i = 0; i < region_count; i++) {
        if (regions[i].type != PMM_USABLE)
            continue;

        total_usable_bytes += regions[i].length;

        uint64_t raw_end = regions[i].base + regions[i].length;
        if (raw_end < regions[i].base)
            continue;

        uint64_t base = align_up(regions[i].base);
        uint64_t end = align_down(raw_end);
        if (end <= base || span_count >= PMM_MAX_REGIONS)
            continue;

        spans[span_count].base = base;
        spans[span_count].pages = (end - base) / PMM_PAGE_SIZE;
        spans[span_count].first = total_pages;
        total_pages += spans[span_count].pages;
        span_count++;
    }

    if (span_count == 0 || total_pages == 0)
        return -1;

    bitmap_bytes = (total_pages + 7) / 8;
    metadata_pages = (bitmap_bytes + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    /* Metadata must live in one usable span and must never cover the kernel. */
    int metadata_span = -1;
    for (unsigned i = 0; i < span_count; i++) {
        uint64_t span_end = spans[i].base + metadata_pages * PMM_PAGE_SIZE;
        if (kernel_phys_end > spans[i].base &&
            kernel_phys_start < span_end)
            continue;
        if (spans[i].pages >= metadata_pages) {
            metadata_span = (int)i;
            break;
        }
    }
    if (metadata_span < 0)
        return -1;

    uint64_t metadata_phys = spans[metadata_span].base;
    bitmap = (uint8_t *)(uintptr_t)(metadata_phys + hhdm_offset);

    for (uint64_t i = 0; i < bitmap_bytes; i++)
        bitmap[i] = 0;

    add_reserved(metadata_phys,
                 metadata_phys + metadata_pages * PMM_PAGE_SIZE);
    for (uint64_t i = 0; i < metadata_pages; i++)
        set_bit(spans[metadata_span].first + i);

    /* Mark every page overlapping the kernel image as used, even when the
     * boot loader reported that range as usable. */
    if (kernel_phys_end > kernel_phys_start) {
        for (unsigned i = 0; i < span_count; i++) {
            uint64_t base = spans[i].base;
            uint64_t span_end = base + spans[i].pages * PMM_PAGE_SIZE;

            if (kernel_phys_end <= base || kernel_phys_start >= span_end)
                continue;

            uint64_t first = kernel_phys_start > base
                           ? (kernel_phys_start - base) / PMM_PAGE_SIZE : 0;
            uint64_t last_raw = kernel_phys_end > base
                              ? kernel_phys_end - base : 0;
            uint64_t last = (last_raw + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
            if (last > spans[i].pages)
                last = spans[i].pages;

            add_reserved(base + first * PMM_PAGE_SIZE,
                         base + last * PMM_PAGE_SIZE);
            for (uint64_t k = first; k < last; k++)
                set_bit(spans[i].first + k);
        }
    }

    used_pages = 0;
    for (uint64_t i = 0; i < total_pages; i++) {
        if (bit_is_set(i))
            used_pages++;
    }

    ready = 1;
    return 0;
}

uint64_t pmm_alloc_page(void) {
    if (!ready)
        return 0;

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bit_is_set(i)) {
            set_bit(i);
            used_pages++;
            return phys_for_index(i);
        }
    }
    return 0;                       /* exhaustion */
}

int pmm_free_page(uint64_t physical) {
    if (!ready || physical == 0 || (physical & (PMM_PAGE_SIZE - 1)) != 0)
        return -1;
    if (is_reserved(physical))
        return -1;

    uint64_t index = index_for_phys(physical);
    if (index == UINT64_MAX || !bit_is_set(index))
        return -1;                  /* unmanaged or double free */

    clear_bit(index);
    used_pages--;
    return 0;
}

void pmm_get_stats(struct pmm_stats *stats) {
    if (stats == 0)
        return;

    stats->total_usable_bytes = total_usable_bytes;
    stats->managed_pages = total_pages;
    stats->used_pages = used_pages;
    stats->free_pages = ready ? total_pages - used_pages : 0;
    stats->metadata_pages = metadata_pages;
}

int pmm_initialized(void) {
    return ready;
}

int pmm_self_test(void) {
    if (!ready)
        return -1;

    uint64_t pages[4] = {0, 0, 0, 0};
    unsigned allocated = 0;

    for (; allocated < 4; allocated++) {
        pages[allocated] = pmm_alloc_page();
        if (pages[allocated] == 0)
            goto fail;
    }

    for (unsigned i = 0; i < 4; i++) {
        if (is_reserved(pages[i]))
            goto fail;
        for (unsigned j = 0; j < i; j++) {
            if (pages[i] == pages[j])
                goto fail;
        }
    }

    /* Write through HHDM into freshly allocated, non-reserved pages only. */
    for (unsigned i = 0; i < 4; i++) {
        volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)
            (pages[i] + hhdm_offset_saved);
        uint8_t pattern = (uint8_t)(0xa5u + i);

        for (uint64_t j = 0; j < PMM_PAGE_SIZE; j++)
            p[j] = (uint8_t)((j & 0xff) ^ pattern);
    }

    for (unsigned i = 0; i < 4; i++) {
        volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)
            (pages[i] + hhdm_offset_saved);
        uint8_t pattern = (uint8_t)(0xa5u + i);

        for (uint64_t j = 0; j < PMM_PAGE_SIZE; j++) {
            if (p[j] != (uint8_t)((j & 0xff) ^ pattern))
                goto fail;
        }
    }

    for (unsigned i = 0; i < 4; i++) {
        if (pmm_free_page(pages[i]) != 0)
            goto fail;
    }
    allocated = 0;

    /* First-fit must hand back the lowest freed page. */
    uint64_t again = pmm_alloc_page();
    if (again != pages[0])
        goto fail;
    if (pmm_free_page(again) != 0)
        goto fail;

    return 0;

fail:
    for (unsigned i = 0; i < allocated; i++)
        (void)pmm_free_page(pages[i]);
    return -2;
}
