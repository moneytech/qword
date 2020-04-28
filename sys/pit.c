#include <stdint.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <sys/pit.h>
#include <sys/apic.h>

int init_pit(void) {
    kprint(KPRN_INFO, "pit: Setting frequency to %uHz", PIT_FREQUENCY_HZ);

    uint16_t x = 1193182 / PIT_FREQUENCY_HZ;
    if ((1193182 % PIT_FREQUENCY_HZ) > (PIT_FREQUENCY_HZ / 2))
        x++;

    port_out_b(0x40, (uint8_t)(x & 0x00ff));
    io_wait();
    port_out_b(0x40, (uint8_t)((x & 0xff00) >> 8));
    io_wait();

    kprint(KPRN_INFO, "pit: Frequency updated");

    kprint(KPRN_INFO, "pit: Unmasking PIT IRQ");
    io_apic_set_up_legacy_irq(0, 0, 1);

    return 0;
}
