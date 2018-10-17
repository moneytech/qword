#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <acpi/madt.h>
#include <panic.h>
#include <smp.h>
#include <time.h>
#include <mm.h>
#include <task.h>

#define CPU_STACK_SIZE 16384

/* External assembly routines */
void smp_init_cpu0_local(void *, void *);
void *smp_prepare_trampoline(void *, void *, void *, void *, void *);
int smp_check_ap_flag(void);

int smp_cpu_count = 1;

struct tss_t {
    uint32_t unused0 __attribute__((aligned(16)));
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint32_t iopb_offset;
} __attribute__((packed));

static size_t cpu_stack_top = KERNEL_PHYS_OFFSET + 0xeffff0;

struct cpu_local_t cpu_locals[MAX_CPUS];
static struct tss_t cpu_tss[MAX_CPUS] __attribute__((aligned(16)));

static void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "smp: Started up AP #%u", current_cpu);
    kprint(KPRN_INFO, "smp: Kernel stack top: %X", cpu_locals[current_cpu].kernel_stack);

    /* Enable this AP's local APIC */
    lapic_enable();

    /* Enable interrupts */
    asm volatile ("sti");

    /* Wait for some job to be scheduled */
    for (;;) asm volatile ("hlt");
}

static inline void setup_cpu_local(int cpu_number, uint8_t lapic_id) {
    /* Prepare CPU local */
    cpu_locals[cpu_number].cpu_number = cpu_number;
    cpu_locals[cpu_number].kernel_stack = cpu_stack_top;
    cpu_locals[cpu_number].current_process = -1;
    cpu_locals[cpu_number].current_thread = -1;
    cpu_locals[cpu_number].current_task = -1;
    cpu_locals[cpu_number].lapic_id = lapic_id;

    /* Prepare TSS */
    cpu_tss[cpu_number].rsp0 = (uint64_t)cpu_stack_top;
    cpu_tss[cpu_number].ist1 = (uint64_t)cpu_stack_top;

    return;
}

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    if (cpu_number == MAX_CPUS) {
        panic("smp: CPU limit exceeded", smp_cpu_count, 0);
    }

    setup_cpu_local(cpu_number, target_apic_id);

    struct cpu_local_t *cpu_local = &cpu_locals[cpu_number];
    struct tss_t *tss = &cpu_tss[cpu_number];

    void *trampoline = smp_prepare_trampoline(ap_kernel_entry, (void *)kernel_pagemap.pml4,
                                (void *)cpu_stack_top, cpu_local, tss);

    /* Send the INIT IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4500);
    /* wait 10ms */
    ksleep(10);
    /* Send the Startup IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
    /* wait 1ms */
    ksleep(1);

    if (smp_check_ap_flag()) {
        goto success;
    } else {
        /* Send the Startup IPI again */
        lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
        lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
        /* wait 1s */
        ksleep(1000);
        if (smp_check_ap_flag())
            goto success;
        else
            return -1;
    }

success:
    cpu_stack_top -= CPU_STACK_SIZE;
    return 0;
}

static void init_cpu0(void) {
    setup_cpu_local(0, 0);

    struct cpu_local_t *cpu_local = &cpu_locals[0];
    struct tss_t *tss = &cpu_tss[0];

    smp_init_cpu0_local(cpu_local, tss);

    cpu_stack_top -= CPU_STACK_SIZE;

    return;
}

void init_smp(void) {
    /* prepare CPU 0 first */
    init_cpu0();

    /* start up the APs and jump them into the kernel */
    for (size_t i = 1; i < madt_local_apic_i; i++) {
        kprint(KPRN_INFO, "smp: Starting up AP #%u", i);
        if (start_ap(madt_local_apics[i]->apic_id, smp_cpu_count)) {
            kprint(KPRN_ERR, "smp: Failed to start AP #%u", i);
            continue;
        }
        smp_cpu_count++;
        /* wait a bit */
        ksleep(10);
    }

    kprint(KPRN_INFO, "smp: Total CPU count: %u", smp_cpu_count);

    return;
}
