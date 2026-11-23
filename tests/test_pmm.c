/* Host-side tests for the physical page allocator.  These exercise pure PMM
 * logic with synthetic memory maps and a host buffer standing in for HHDM;
 * they do NOT verify real physical memory access. */
#include <stdint.h>
#include <stdio.h>

#include "pmm.h"

#define MIB (1024u * 1024u)
#define PAGE 4096u

static uint8_t metadata_buffer[65536];
static int checks;
static int failures;

static void check(int condition, const char *label) {
    checks++;
    printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        failures++;
}

static uint64_t hhdm_for(uint64_t metadata_phys) {
    return (uint64_t)(uintptr_t)metadata_buffer - metadata_phys;
}

static void test_init_and_stats(void) {
    struct pmm_region regions[] = {
        { 0x100000, 8 * MIB, PMM_USABLE },
        { 0x900000, MIB, 1 }             /* reserved */
    };

    int rc = pmm_init(hhdm_for(0x100000), 0, 0, regions, 2);
    check(rc == 0 && pmm_initialized(),
          "init succeeds from a synthetic Limine-style map");

    struct pmm_stats s;
    pmm_get_stats(&s);
    check(s.total_usable_bytes == 8 * MIB &&
          s.managed_pages == (8 * MIB) / PAGE &&
          s.metadata_pages == 1,
          "stats report usable bytes, managed pages and metadata pages");
    check(s.used_pages >= 1 && s.free_pages == s.managed_pages - s.used_pages,
          "stats are internally consistent after init");
}

static void test_allocation_alignment_and_range(void) {
    struct pmm_region regions[] = {
        { 0x100000, 4 * MIB, PMM_USABLE },
        { 0x500000, 4 * MIB, 7 }         /* framebuffer */
    };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 2);

    uint64_t page = pmm_alloc_page();
    check(page != 0 && (page & (PAGE - 1)) == 0,
          "allocated address is page-aligned");
    check(page >= 0x100000 && page < 0x500000,
          "allocation stays inside the usable region");

    uint64_t second = pmm_alloc_page();
    check(second != 0 && second != page,
          "successive allocations return distinct pages");
}

static void test_free_and_reallocate(void) {
    struct pmm_region regions[] = { { 0x100000, 4 * MIB, PMM_USABLE } };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 1);

    struct pmm_stats before, after;
    pmm_get_stats(&before);

    uint64_t page = pmm_alloc_page();
    pmm_get_stats(&after);
    check(after.used_pages == before.used_pages + 1,
          "allocation increases the used-page count");

    check(pmm_free_page(page) == 0,
          "freeing an allocated page succeeds");
    pmm_get_stats(&after);
    check(after.used_pages == before.used_pages,
          "free restores the used-page count");

    check(pmm_alloc_page() == page,
          "a freed page is handed out again");
}

static void test_exhaustion(void) {
    /* Two pages: one holds the bitmap, one is allocatable. */
    struct pmm_region regions[] = { { 0x100000, 2 * PAGE, PMM_USABLE } };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 1);

    uint64_t first = pmm_alloc_page();
    uint64_t second = pmm_alloc_page();

    check(first != 0 && second == 0,
          "allocation returns 0 when pages are exhausted");

    struct pmm_stats s;
    pmm_get_stats(&s);
    check(s.free_pages == 0 && s.used_pages == s.managed_pages,
          "exhausted allocator reports zero free pages");
}

static void test_invalid_and_double_free(void) {
    struct pmm_region regions[] = { { 0x100000, 4 * MIB, PMM_USABLE } };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 1);

    check(pmm_free_page(0) != 0, "freeing physical address 0 is rejected");
    check(pmm_free_page(0x100001) != 0,
          "freeing an unaligned address is rejected");
    check(pmm_free_page(0x80000000) != 0,
          "freeing an address outside usable regions is rejected");

    uint64_t page = pmm_alloc_page();
    check(pmm_free_page(page) == 0 && pmm_free_page(page) != 0,
          "double free of the same page is rejected");
}

static void test_reserved_exclusion(void) {
    struct pmm_region regions[] = {
        { 0x100000, MIB, PMM_USABLE },
        { 0x200000, MIB, 1 },
        { 0x300000, MIB, 7 }
    };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 3);

    int outside = 0;
    unsigned allocations = 0;
    for (unsigned i = 0; i < 300; i++) {
        uint64_t page = pmm_alloc_page();
        if (page == 0)
            break;
        allocations++;
        if (page < 0x100000 || page >= 0x200000)
            outside = 1;
    }

    check(allocations == 255 && !outside,
          "every allocatable page comes only from the usable region");
    check(pmm_alloc_page() == 0,
          "allocator stops after all usable pages are handed out");
}

static void test_non_aligned_region(void) {
    /* base 0x100003, end 0x103005 -> managed pages 0x101000 and 0x102000.
     * One is reserved for metadata, leaving one allocatable page. */
    struct pmm_region regions[] = { { 0x100003, 3 * PAGE + 10, PMM_USABLE } };
    pmm_init(hhdm_for(0x101000), 0, 0, regions, 1);

    struct pmm_stats s;
    pmm_get_stats(&s);
    check(s.managed_pages == 2,
          "unaligned region edges are trimmed to whole pages");

    uint64_t page = pmm_alloc_page();
    check(page == 0x101000 || page == 0x102000,
          "allocation from an unaligned region stays page-aligned");
}

static void test_empty_maps(void) {
    check(pmm_init(0, 0, 0, 0, 0) != 0,
          "empty memory map is rejected");

    struct pmm_region reserved_only[] = { { 0x100000, MIB, 1 } };
    check(pmm_init(0, 0, 0, reserved_only, 1) != 0,
          "map without usable regions is rejected");

    struct pmm_region tiny[] = { { 0x100000, 100, PMM_USABLE } };
    check(pmm_init(0, 0, 0, tiny, 1) != 0,
          "usable region smaller than a page is rejected");
}

static void test_kernel_exclusion(void) {
    struct pmm_region regions[] = { { 0x100000, MIB, PMM_USABLE } };
    pmm_init(hhdm_for(0x100000), 0x101000, 0x103000, regions, 1);

    uint64_t page = pmm_alloc_page();
    check(page == 0x103000,
          "pages overlapping the kernel image are never allocated");
    check(pmm_free_page(0x101000) != 0,
          "freeing a kernel page is rejected as reserved");
}

/* A 256 MiB map needs 8192 bytes of bitmap = two 4 KiB pages. The boot log's
 * "2 used pages" is exactly this metadata, not a one-page bitmap. */
static void test_multipage_bitmap(void) {
    struct pmm_region regions[] = {
        { 0x100000, 256 * MIB, PMM_USABLE }
    };

    check(pmm_init(hhdm_for(0x100000), 0, 0, regions, 1) == 0,
          "large synthetic map initializes successfully");

    struct pmm_stats s;
    pmm_get_stats(&s);
    check(s.managed_pages == 65536 && s.metadata_pages == 2,
          "bitmap for 65536 pages occupies two metadata pages");
    check(s.used_pages == 2 && s.free_pages == 65534,
          "both metadata pages are marked used and reserved");

    uint64_t a = pmm_alloc_page();
    uint64_t b = pmm_alloc_page();
    check(a == 0x102000 && b == 0x103000 && a != b,
          "first allocations start immediately after both metadata pages");
    check(pmm_free_page(0x100000) != 0 && pmm_free_page(0x101000) != 0,
          "freeing either metadata page is rejected");
}

/* Bitmap indices are cumulative across spans; freeing in the second span
 * must map back to the correct physical page. */
static void test_bitmap_across_spans(void) {
    struct pmm_region regions[] = {
        { 0x100000, MIB, PMM_USABLE },
        { 0x400000, MIB, PMM_USABLE }
    };

    pmm_init(hhdm_for(0x100000), 0, 0, regions, 2);

    uint64_t pages[(MIB / PAGE) * 2 + 1];
    unsigned capacity = (MIB / PAGE) * 2 + 1;
    unsigned count = 0;

    while (count < capacity) {
        uint64_t page = pmm_alloc_page();
        if (page == 0)
            break;
        pages[count++] = page;
    }

    check(count == 511, "all pages except metadata are allocated across spans");
    check(pages[255] >= 0x400000 && pages[255] < 0x500000,
          "bitmap index correctly reaches the second usable span");

    uint64_t from_second = pages[300];
    check(pmm_free_page(from_second) == 0 && pmm_alloc_page() == from_second,
          "free/reallocate in the second span uses the right bitmap index");
}

static void test_runtime_self_test_logic(void) {
    struct pmm_region regions[] = { { 0x100000, 8 * MIB, PMM_USABLE } };
    pmm_init(hhdm_for(0x100000), 0, 0, regions, 1);

    check(pmm_self_test() == 0,
          "page allocation, HHDM pattern write/read, free and reallocate pass");

    struct pmm_stats s;
    pmm_get_stats(&s);
    check(s.used_pages == s.metadata_pages,
          "self-test leaves the allocator counts unchanged");
}

int main(void) {
    test_init_and_stats();
    test_allocation_alignment_and_range();
    test_free_and_reallocate();
    test_exhaustion();
    test_invalid_and_double_free();
    test_reserved_exclusion();
    test_non_aligned_region();
    test_empty_maps();
    test_kernel_exclusion();
    test_multipage_bitmap();
    test_bitmap_across_spans();
    test_runtime_self_test_logic();

    printf("\n%d checks, %d failures: %s\n",
           checks, failures, failures ? "FAILED" : "ALL TESTS PASSED");
    return failures != 0;
}
