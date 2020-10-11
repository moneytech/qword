#include <stdint.h>
#include <stddef.h>
#include <sys/apic.h>
#include <lib/klib.h>
#include <acpi/madt.h>
#include <mm/mm.h>
#include <sys/cpu.h>

#define APIC_CPUID_BIT (1 << 9)

int apic_supported(void) {
    unsigned int eax, ebx, ecx, edx = 0;

    kprint(KPRN_INFO, "apic: Checking for support...");

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    /* Check if the apic bit is set */
    if ((edx & APIC_CPUID_BIT)) {
        kprint(KPRN_INFO, "apic: Supported!");
        return 1;
    } else {
        kprint(KPRN_INFO, "apic: Unsupported!");
        return 0;
    }
}

uint32_t lapic_read(uint32_t reg) {
    size_t lapic_base = (size_t)madt->local_controller_addr + MEM_PHYS_OFFSET;
    return *((volatile uint32_t *)(lapic_base + reg));
}

void lapic_write(uint32_t reg, uint32_t data) {
    size_t lapic_base = (size_t)madt->local_controller_addr + MEM_PHYS_OFFSET;
    *((volatile uint32_t *)(lapic_base + reg)) = data;
}

void lapic_set_nmi(uint8_t vec, uint16_t flags, uint8_t lint) {
    uint32_t nmi = 0x400 | vec;

    if (flags & 2) {
        nmi |= (1 << 13);
    }

    if (flags & 8) {
        nmi |= (1 << 15);
    }

    if (lint == 1) {
        lapic_write(0x360, nmi);
    } else if (lint == 0) {
        lapic_write(0x350, nmi);
    }
}

void lapic_install_nmi(int vec, int nmi) {
    lapic_set_nmi(vec, madt_nmis[nmi]->flags, madt_nmis[nmi]->lint);
}

void lapic_enable(void) {
    lapic_write(0xf0, lapic_read(0xf0) | 0x1ff);
}

void lapic_eoi(void) {
    lapic_write(0xb0, 0);
}

void lapic_send_ipi(int cpu, uint8_t vector) {
    lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[cpu].lapic_id) << 24);
    lapic_write(APICREG_ICR0, vector);
}

/* Read from the `io_apic_num`'th I/O APIC as described by the MADT */
uint32_t io_apic_read(size_t io_apic_num, uint32_t reg) {
    volatile uint32_t *base = (volatile uint32_t *)((size_t)madt_io_apics[io_apic_num]->addr + MEM_PHYS_OFFSET);
    *base = reg;
    return *(base + 4);
}

/* Write to the `io_apic_num`'th I/O APIC as described by the MADT */
void io_apic_write(size_t io_apic_num, uint32_t reg, uint32_t data) {
    volatile uint32_t *base = (volatile uint32_t *)((size_t)madt_io_apics[io_apic_num]->addr + MEM_PHYS_OFFSET);
    *base = reg;
    *(base + 4) = data;
}

/* Return the index of the I/O APIC that handles this redirect */
size_t io_apic_from_redirect(uint32_t gsi) {
    for (size_t i = 0; i < madt_io_apic_i; i++) {
        if (madt_io_apics[i]->gsib <= gsi && madt_io_apics[i]->gsib + io_apic_get_max_redirect(i) > gsi)
            return i;
    }

    return -1;
}

/* Get the maximum number of redirects this I/O APIC can handle */
uint32_t io_apic_get_max_redirect(size_t io_apic_num) {
    return (io_apic_read(io_apic_num, 1) & 0xff0000) >> 16;
}

static void io_apic_set_redirect(uint8_t vec, uint32_t gsi, uint16_t flags, int cpu, int status) {
    size_t io_apic = io_apic_from_redirect(gsi);

    uint64_t redirect = vec;

    // Active high(0) or low(1)
    if (flags & 2) {
        redirect |= (1 << 13);
    }

    // Edge(0) or level(1) triggered
    if (flags & 8) {
        redirect |= (1 << 15);
    }

    if (!status) {
        /* Set mask bit */
        redirect |= (1 << 16);
    }

    /* Set target APIC ID */
    redirect |= ((uint64_t)cpu_locals[cpu].lapic_id) << 56;
    uint32_t ioredtbl = (gsi - madt_io_apics[io_apic]->gsib) * 2 + 16;

    io_apic_write(io_apic, ioredtbl + 0, (uint32_t)redirect);
    io_apic_write(io_apic, ioredtbl + 1, (uint32_t)(redirect >> 32));
}

void io_apic_set_up_legacy_irq(int cpu, uint8_t irq, int status) {
    /* Redirect will handle whether the IRQ is masked or not, we just need to search the
     * MADT ISOs for a corresponding IRQ */
    for (size_t i = 0; i < madt_iso_i; i++) {
        if (madt_isos[i]->irq_source == irq) {
            io_apic_set_redirect(madt_isos[i]->irq_source + 0x20, madt_isos[i]->gsi,
                            madt_isos[i]->flags, cpu, status);
            return;
        }
    }
    io_apic_set_redirect(irq + 0x20, irq, 0, cpu, status);
}

void io_apic_connect_gsi_to_vec(int cpu, uint8_t vec, uint32_t gsi, uint16_t flags, int status) {
    io_apic_set_redirect(vec, gsi, flags, cpu, status);
}

uint32_t *lapic_eoi_ptr;

void init_apic(void) {
    lapic_enable();
    size_t lapic_base = (size_t)madt->local_controller_addr + MEM_PHYS_OFFSET;
    lapic_eoi_ptr = (uint32_t *)(lapic_base + 0xb0);
    kprint(KPRN_INFO, "apic: Done! APIC initialised.");
}
