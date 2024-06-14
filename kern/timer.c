#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/types.h>
#include <inc/uefi.h>
#include <inc/x86.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/pmap.h>
#include <kern/timer.h>
#include <kern/trap.h>
#include <kern/tsc.h>

#define kilo      (1000ULL)
#define Mega      (kilo * kilo)
#define Giga      (kilo * Mega)
#define Tera      (kilo * Giga)
#define Peta      (kilo * Tera)
#define ULONG_MAX ~0UL

#if LAB <= 6
/* Early variant of memory mapping that does 1:1 aligned area mapping
 * in 2MB pages. You will need to reimplement this code with proper
 * virtual memory mapping in the future. */
void *
mmio_map_region(physaddr_t pa, size_t size) {
    void map_addr_early_boot(uintptr_t addr, uintptr_t addr_phys, size_t sz);
    const physaddr_t base_2mb = 0x200000;
    uintptr_t org = pa;
    size += pa & (base_2mb - 1);
    size += (base_2mb - 1);
    pa &= ~(base_2mb - 1);
    size &= ~(base_2mb - 1);
    map_addr_early_boot(pa, pa, size);
    return (void *)org;
}
void *
mmio_remap_last_region(physaddr_t pa, void *addr, size_t oldsz, size_t newsz) {
    return mmio_map_region(pa, newsz);
}
#endif

struct Timer timertab[MAX_TIMERS];
struct Timer *timer_for_schedule;

struct Timer timer_hpet0 = {
        .timer_name = "hpet0",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim0,
        .handle_interrupts = hpet_handle_interrupts_tim0,
};

struct Timer timer_hpet1 = {
        .timer_name = "hpet1",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim1,
        .handle_interrupts = hpet_handle_interrupts_tim1,
};

struct Timer timer_acpipm = {
        .timer_name = "pm",
        .timer_init = acpi_enable,
        .get_cpu_freq = pmtimer_cpu_frequency,
};

void
acpi_enable(void) {
    FADT *fadt = get_fadt();
    outb(fadt->SMI_CommandPort, fadt->AcpiEnable);
    while ((inw(fadt->PM1aControlBlock) & 1) == 0) /* nothing */
        ;
}

static void *
acpi_find_table(const char *sign) {
    /*
     * This function performs lookup of ACPI table by its signature
     * and returns valid pointer to the table mapped somewhere.
     *
     * It is a good idea to checksum tables before using them.
     *
     * HINT: Use mmio_map_region/mmio_remap_last_region
     * before accessing table addresses
     * (Why mmio_remap_last_region is requrired?)
     * HINT: RSDP address is stored in uefi_lp->ACPIRoot
     * HINT: You may want to distunguish RSDT/XSDT
     */
    // LAB 5: Your code here:

    if (!uefi_lp->ACPIRoot) {
        panic ("No ACPI root!\n");
    }

    RSDP* acpi_rsdp = (RSDP*)mmio_map_region(uefi_lp->ACPIRoot, sizeof(RSDP));

    // check for currect
    if (strncmp(acpi_rsdp->Signature, "RSD PTR ", 8) != 0) {
        cprintf("Error: Bad acpi_rsdp signature: [%s]\n", acpi_rsdp->Signature);
        return NULL; // Incorrect signature of RSD
    }
    // checksum
    uint8_t* rsdp_bytes = (uint8_t*)acpi_rsdp;
    uint8_t checksum = 0;
    int i = 0;
    for (; i < acpi_rsdp->Length; i++)
        checksum += rsdp_bytes[i];
    if (checksum != 0) {
        cprintf("Error: Bad acpi_rsdp checksum\n");
        return NULL; // Incorrect checksum
    }
    // getting rsdt
    ACPISDTHeader *h = NULL;
    int entries = 0;
    if (acpi_rsdp->Revision == 0) {
        // ACPI 1.0, so using RSDT
        RSDT* rsdt = NULL;
        rsdt = (RSDT*)mmio_map_region((uint64_t)acpi_rsdp->RsdtAddress, sizeof(rsdt));
        rsdt = (RSDT*)mmio_remap_last_region((physaddr_t)acpi_rsdp->RsdtAddress, rsdt, sizeof(rsdt), rsdt->h.Length);

        entries = (rsdt->h.Length - sizeof(rsdt->h)) / sizeof(uint32_t);
        for (i = 0; i < entries; i++)
        {   
            h = (ACPISDTHeader *)mmio_map_region((uint64_t)rsdt->PointerToOtherSDT[i], sizeof(ACPISDTHeader));
            h = (ACPISDTHeader *)mmio_remap_last_region((uint64_t)rsdt->PointerToOtherSDT[i], h, sizeof(ACPISDTHeader), h->Length);
            if (!strncmp(h->Signature, sign, 4)) {
                break;
            }
        }
    }
    else if (acpi_rsdp->Revision == 2) {
        // ACPI 2.0 to 6.1, so using XSDT
        XSDT* rsdt = NULL;
        rsdt = (XSDT*)mmio_map_region(acpi_rsdp->XsdtAddress, sizeof(rsdt));
        rsdt = (XSDT*)mmio_remap_last_region((physaddr_t)acpi_rsdp->XsdtAddress, rsdt, sizeof(rsdt), rsdt->h.Length);

        entries = (rsdt->h.Length - sizeof(rsdt->h)) / sizeof(uint64_t);
        for (i = 0; i < entries; i++)
        {   
            h = (ACPISDTHeader *)mmio_map_region((uint64_t)rsdt->PointerToOtherSDT[i], sizeof(ACPISDTHeader));
            h = (ACPISDTHeader *)mmio_remap_last_region((uint64_t)rsdt->PointerToOtherSDT[i], h, sizeof(ACPISDTHeader), h->Length);
            if (!strncmp(h->Signature, sign, 4)) {
                break;
            }
        }
    }
    else {
        cprintf("Error: Can't resolve this RSDP revision\n");
        return NULL; // Can't resolve this RSDP Revision
    }
    if (i == entries) {
        cprintf("Error: Can't find acpisht header with sign [%.4s]\n", sign);
        return NULL;
    }
    // do checksum for ACPI table
    checksum = 0;
    for (int i = 0; i < h->Length; i++)
        checksum += ((char *)h)[i];
    
    if (checksum != 0) {
        cprintf("Error: Bad rsdt checksum\n");
        return NULL; // Incorrect checksum
    }
    return h;
}

MCFG *
get_mcfg(void) {
    static MCFG *kmcfg;
    if (!kmcfg) {
        struct AddressSpace *as = switch_address_space(&kspace);
        kmcfg = acpi_find_table("MCFG");
        switch_address_space(as);
    }

    return kmcfg;
}

#define MAX_SEGMENTS 16

uintptr_t
make_fs_args(char *ustack_top) {

    MCFG *mcfg = get_mcfg();
    if (!mcfg) {
        cprintf("MCFG table is absent!");
        return (uintptr_t)ustack_top;
    }

    char *argv[MAX_SEGMENTS + 3] = {0};

    /* Store argv strings on stack */

    ustack_top -= 3;
    argv[0] = ustack_top;
    nosan_memcpy(argv[0], "fs", 3);

    int nent = (mcfg->h.Length - sizeof(MCFG)) / sizeof(CSBAA);
    if (nent > MAX_SEGMENTS)
        nent = MAX_SEGMENTS;

    for (int i = 0; i < nent; i++) {
        CSBAA *ent = &mcfg->Data[i];

        char arg[64];
        snprintf(arg, sizeof(arg) - 1, "ecam=%llx:%04x:%02x:%02x",
                 (long long)ent->BaseAddress, ent->SegmentGroup, ent->StartBus, ent->EndBus);

        int len = strlen(arg) + 1;
        ustack_top -= len;
        nosan_memcpy(ustack_top, arg, len);
        argv[i + 1] = ustack_top;
    }

    char arg[64];
    snprintf(arg, sizeof(arg) - 1, "tscfreq=%llx", (long long)tsc_calibrate());
    int len = strlen(arg) + 1;
    ustack_top -= len;
    nosan_memcpy(ustack_top, arg, len);
    argv[nent + 1] = ustack_top;

    /* Realign stack */
    ustack_top = (char *)((uintptr_t)ustack_top & ~(2 * sizeof(void *) - 1));

    /* Copy argv vector */
    ustack_top -= (nent + 3) * sizeof(void *);
    nosan_memcpy(ustack_top, argv, (nent + 3) * sizeof(argv[0]));

    char **argv_arg = (char **)ustack_top;
    long argc_arg = nent + 2;

    /* Store argv and argc arguemnts on stack */
    ustack_top -= sizeof(void *);
    nosan_memcpy(ustack_top, &argv_arg, sizeof(argv_arg));
    ustack_top -= sizeof(void *);
    nosan_memcpy(ustack_top, &argc_arg, sizeof(argc_arg));

    /* and return new stack pointer */
    return (uintptr_t)ustack_top;
}

/* Obtain and map FADT ACPI table address. */
FADT *
get_fadt(void) {
    // LAB 5: Your code here
    // (use acpi_find_table)
    // HINT: ACPI table signatures are
    //       not always as their names
    static FADT *rsdt = NULL;
    if (!rsdt) {
        struct AddressSpace *as = switch_address_space(&kspace);
        rsdt = acpi_find_table("FACP");
        switch_address_space(as);
    }
    return rsdt;
}

/* Obtain and map RSDP ACPI table address. */
HPET *
get_hpet(void) {
    // LAB 5: Your code here
    // (use acpi_find_table)

    static RSDT *rsdt = NULL;
    if (!rsdt) {
        struct AddressSpace *as = switch_address_space(&kspace);
        rsdt = acpi_find_table("HPET");
        switch_address_space(as);
    }
    return (HPET*)rsdt;
}

/* Getting physical HPET timer address from its table. */
HPETRegister *
hpet_register(void) {
    HPET *hpet_timer = get_hpet();
    if (!hpet_timer->address.address) panic("hpet is unavailable\n");

    uintptr_t paddr = hpet_timer->address.address;
    return mmio_map_region(paddr, sizeof(HPETRegister));
}

/* Debug HPET timer state. */
void
hpet_print_struct(void) {
    HPET *hpet = get_hpet();
    assert(hpet != NULL);
    cprintf("signature = %s\n", (hpet->h).Signature);
    cprintf("length = %08x\n", (hpet->h).Length);
    cprintf("revision = %08x\n", (hpet->h).Revision);
    cprintf("checksum = %08x\n", (hpet->h).Checksum);

    cprintf("oem_revision = %08x\n", (hpet->h).OEMRevision);
    cprintf("creator_id = %08x\n", (hpet->h).CreatorID);
    cprintf("creator_revision = %08x\n", (hpet->h).CreatorRevision);

    cprintf("hardware_rev_id = %08x\n", hpet->hardware_rev_id);
    cprintf("comparator_count = %08x\n", hpet->comparator_count);
    cprintf("counter_size = %08x\n", hpet->counter_size);
    cprintf("reserved = %08x\n", hpet->reserved);
    cprintf("legacy_replacement = %08x\n", hpet->legacy_replacement);
    cprintf("pci_vendor_id = %08x\n", hpet->pci_vendor_id);
    cprintf("hpet_number = %08x\n", hpet->hpet_number);
    cprintf("minimum_tick = %08x\n", hpet->minimum_tick);

    cprintf("address_structure:\n");
    cprintf("address_space_id = %08x\n", (hpet->address).address_space_id);
    cprintf("register_bit_width = %08x\n", (hpet->address).register_bit_width);
    cprintf("register_bit_offset = %08x\n", (hpet->address).register_bit_offset);
    cprintf("address = %08lx\n", (unsigned long)(hpet->address).address);
}

static volatile HPETRegister *hpetReg;
/* HPET timer period (in femtoseconds) */
static uint64_t hpetFemto = 0;
/* HPET timer frequency */
static uint64_t hpetFreq = 0;

/* HPET timer initialisation */
void
hpet_init() {
    if (hpetReg == NULL) {
        nmi_disable();
        hpetReg = hpet_register();
        uint64_t cap = hpetReg->GCAP_ID;
        hpetFemto = (uintptr_t)(cap >> 32);
        if (!(cap & HPET_LEG_RT_CAP)) panic("HPET has no LegacyReplacement mode");

        // cprintf("hpetFemto = %llu\n", hpetFemto);
        hpetFreq = (1 * Peta) / hpetFemto;
        // cprintf("HPET: Frequency = %d.%03dMHz\n", (uintptr_t)(hpetFreq / Mega), (uintptr_t)(hpetFreq % Mega));
        /* Enable ENABLE_CNF bit to enable timer */
        hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
        nmi_enable();
    }
}

/* HPET register contents debugging. */
void
hpet_print_reg(void) {
    cprintf("GCAP_ID = %016lx\n", (unsigned long)hpetReg->GCAP_ID);
    cprintf("GEN_CONF = %016lx\n", (unsigned long)hpetReg->GEN_CONF);
    cprintf("GINTR_STA = %016lx\n", (unsigned long)hpetReg->GINTR_STA);
    cprintf("MAIN_CNT = %016lx\n", (unsigned long)hpetReg->MAIN_CNT);
    cprintf("TIM0_CONF = %016lx\n", (unsigned long)hpetReg->TIM0_CONF);
    cprintf("TIM0_COMP = %016lx\n", (unsigned long)hpetReg->TIM0_COMP);
    cprintf("TIM0_FSB = %016lx\n", (unsigned long)hpetReg->TIM0_FSB);
    cprintf("TIM1_CONF = %016lx\n", (unsigned long)hpetReg->TIM1_CONF);
    cprintf("TIM1_COMP = %016lx\n", (unsigned long)hpetReg->TIM1_COMP);
    cprintf("TIM1_FSB = %016lx\n", (unsigned long)hpetReg->TIM1_FSB);
    cprintf("TIM2_CONF = %016lx\n", (unsigned long)hpetReg->TIM2_CONF);
    cprintf("TIM2_COMP = %016lx\n", (unsigned long)hpetReg->TIM2_COMP);
    cprintf("TIM2_FSB = %016lx\n", (unsigned long)hpetReg->TIM2_FSB);
}

/* HPET main timer counter value. */
uint64_t
hpet_get_main_cnt(void) {
    return hpetReg->MAIN_CNT;
}

uint64_t
hpet_get_freq(void) {
    uint64_t cap = hpetReg->GCAP_ID;
    hpetFemto = (uintptr_t)(cap >> 32);
    return 1e15 / hpetFemto;
}

/* - Configure HPET timer 0 to trigger every 0.5 seconds on IRQ_TIMER line
 * - Configure HPET timer 1 to trigger every 1.5 seconds on IRQ_CLOCK line
 *
 * HINT To be able to use HPET as PIT replacement consult
 *      LegacyReplacement functionality in HPET spec.
 * HINT Don't forget to unmask interrupt in PIC */
void
hpet_enable_interrupts_tim0(void) {
    // LAB 5: Your code here
    pic_irq_unmask(IRQ_TIMER);

    // disabling interruptions
    hpetReg->GEN_CONF &= ~HPET_ENABLE_CNF;
    // enabling LegacyReplacement Route
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    // reset main counter;
    hpetReg->MAIN_CNT = 0;
    // setting timer to periodic mode
    hpetReg->TIM0_CONF |= HPET_TN_TYPE_CNF;
    hpetReg->TIM0_CONF |= HPET_TN_INT_ENB_CNF;
    
    uint64_t clk_period = hpetReg->GCAP_ID >> 32; // reading clock period in femptoseconds
    uint64_t timer_in_fs = 5 * 1e12; // 0.005 s in fs
    uint64_t periods = timer_in_fs / clk_period;
    // setting clock period
    hpetReg->TIM0_CONF |= HPET_TN_VAL_SET_CNF;
    hpetReg->TIM0_COMP = periods;

    // enable timer back
    hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
}

void
hpet_enable_interrupts_tim1(void) {
    // LAB 5: Your code here
    pic_irq_unmask(IRQ_CLOCK);

    // disabling interruptions
    hpetReg->GEN_CONF &= ~HPET_ENABLE_CNF;
    // enabling LegacyReplacement Route
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    // reset main counter;
    hpetReg->MAIN_CNT = 0;
    // setting timer to periodic mode
    hpetReg->TIM1_CONF |= HPET_TN_TYPE_CNF;
    hpetReg->TIM1_CONF |= HPET_TN_INT_ENB_CNF;
    
    uint64_t clk_period = hpetReg->GCAP_ID >> 32; // reading clock period in femptoseconds
    uint64_t timer_in_fs = 15 * 1e12; // 0.015 s in fs
    uint64_t periods = timer_in_fs / clk_period;
    // setting clock period
    hpetReg->TIM1_CONF |= HPET_TN_VAL_SET_CNF;
    hpetReg->TIM1_COMP = periods;

    // enable timer back
    hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
}

void
hpet_handle_interrupts_tim0(void) {
    pic_send_eoi(IRQ_TIMER);
}

void
hpet_handle_interrupts_tim1(void) {
    pic_send_eoi(IRQ_CLOCK);
}

#define DEFAULT_FREQ  2500000
#define TIMES         100

uint32_t
pmtimer_get_timeval(void) {
    FADT *fadt = get_fadt();
    return inl(fadt->PMTimerBlock);
}

static inline int
hpet_verify(uint64_t val) {
    return (hpet_get_main_cnt() >> 16) == val;
}

static inline int
hpet_expect(uint64_t val, uint64_t *tscp, unsigned long *deltap) {
    uint64_t tsc = 0;

    int count = 0;
    while (count++ < 50000) {
        asm volatile("pause");
        if (!hpet_verify(val)) break;
        tsc = read_tsc();
    }
    *deltap = read_tsc() - tsc;
    *tscp = tsc;

    /* We require _some_ success, but the quality control
     * will be based on the error terms on the TSC values. */
    //cprintf("expect: %d\n", count);
    return count > 6;
}

#define MAX_HPET_MS         25
#define MAX_HPET_ITERATIONS (MAX_HPET_MS * hpet_get_freq() / 1000 / 256 / 256)

static unsigned long
quick_hpet_calibrate(void) {
    int i;
    uint64_t tsc, delta, start_cnt;
    unsigned long d1, d2;

    start_cnt = (hpet_get_main_cnt() >> 16);

    if (hpet_expect(start_cnt, &tsc, &d1)) {
        for (i = 1; i <= MAX_HPET_ITERATIONS; i++) {

            if (!hpet_expect(start_cnt + i, &delta, &d2)) {
                //cprintf("expect break\n");
                break;
            }

            delta -= tsc;
            if (d1 + d2 >= delta >> 11) continue;

            if (!hpet_verify(start_cnt + i + 1)) {
                //cprintf("verify break\n");
                break;
            }
            goto success;
        }
    }
    return 0;

success:

    delta += (long)(d2 - d1) / 2;
    delta *= hpet_get_freq();
    delta /= i * 256 * 1000 * 256;

    return delta;
}

/* Calculate CPU frequency in Hz with the help with HPET timer.
 * HINT Use hpet_get_main_cnt function and do not forget about
 * about pause instruction. */
uint64_t
hpet_cpu_frequency(void) {
    static uint64_t cpu_freq;

    if (!cpu_freq) {
        int i = 100;
        while (--i > 0) {
            if ((cpu_freq = quick_hpet_calibrate())) break;
        }
        if (!i) {
            cpu_freq = DEFAULT_FREQ;
            cprintf("Can't calibrate hpet timer. Using default frequency\n");
        }
    }

    return cpu_freq * 1000;
}


static inline int
pm_verify(uint32_t val) {
    return (pmtimer_get_timeval() >> 12) == val;
}

static inline int
pm_expect(uint32_t val, uint64_t *tscp, unsigned long *deltap) {
    uint64_t tsc = 0;

    int count = 0;
    while (count++ < 50000) {
        if (!pm_verify(val)) break;
        tsc = read_tsc();
        asm volatile("pause");
    }
    *deltap = read_tsc() - tsc;
    *tscp = tsc;

    /* We require _some_ success, but the quality control
     * will be based on the error terms on the TSC values. */
    //cprintf("expect: %d\n", count);
    return count > 6;
}

#define MAX_PM_MS         25
#define MAX_PM_ITERATIONS (MAX_PM_MS * PM_FREQ / 1000 / 256 / 16)

static unsigned long
quick_pm_calibrate(void) {
    int i;
    uint64_t tsc, delta;
    unsigned long d1, d2;

    FADT* fadt = get_fadt();
    int is_24bit = 1;
    if (fadt->Flags & (1 << 8)) {
         is_24bit = 0;
         cprintf("32 bits\n");
    }

    //cprintf("Start:\n");

    uint32_t start_cnt = (pmtimer_get_timeval() >> 12);
    if (pm_expect(start_cnt, &tsc, &d1)) {
        for (i = 1; i <= MAX_PM_ITERATIONS; i++) {

            uint32_t cur_cnt = start_cnt + i;

            if (is_24bit && cur_cnt >= 1 << (24 - 12)) {
                cur_cnt -= (1 << (24 - 12));
                //cprintf("overflow\n");
            }

            if (!pm_expect(cur_cnt, &delta, &d2)) break;

            delta -= tsc;
            if (d1 + d2 >= delta >> 11) continue;

            if (!pm_verify(cur_cnt + 1)) break;

            goto success;
        }
    }
    return 0;

success:

    delta += (long)(d2 - d1) / 2;
    delta *= PM_FREQ;
    delta /= i * 256 * 1000 * 16;

    return delta;
}


/* Calculate CPU frequency in Hz with the help with ACPI PowerManagement timer.
 * HINT Use pmtimer_get_timeval function and do not forget that ACPI PM timer
 *      can be 24-bit or 32-bit. */
uint64_t
pmtimer_cpu_frequency(void) {
    static uint64_t cpu_freq;

    if (!cpu_freq) {
        int i = 100;
        while (--i > 0) {
            if ((cpu_freq = quick_pm_calibrate())) break;
        }
        if (!i) {
            cpu_freq = DEFAULT_FREQ;
            cprintf("Can't calibrate pm timer. Using default frequency\n");
        }
    }

    return cpu_freq * 1000;
}
