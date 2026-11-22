#include <stdint.h>
#include <limine.h>

#define LIMINE_BASE_REVISION(N) \
    { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);

void kmain(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
