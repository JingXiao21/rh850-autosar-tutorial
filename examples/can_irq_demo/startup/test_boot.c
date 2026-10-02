#include "boot_model.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "BOOT FAIL line %d: %s\n", __LINE__, #x); exit(1); \
} } while (0)

static unsigned int passed;
static void pass(const char *name) { printf("BOOT PASS %02u %s\n", ++passed, name); }

int main(void)
{
    BootModel m;
    BootConfig c = Boot_DefaultConfig();
    unsigned int i;
    CHECK(Boot_Run(&m, &c) == BOOT_OK && m.os_owns_cpu);
    CHECK(m.crt_ready && m.stack_ready && m.abi_ready && m.ei_masked);
    CHECK(m.ecum_calls == 1u && m.startos_calls == 1u && m.history_count == 14u);
    for (i = 0u; i < m.history_count; ++i) { CHECK(m.history[i] == (BootStage)i); }
    CHECK(m.reset_snapshot == c.reset_flags && m.option_snapshot == c.opbt0);
    CHECK(m.wdg_deadline_us == 8192u && m.stage == BOOT_OS_OWNED);
    pass("ordered cold boot reaches OS exactly once, captures raw reset/options");

    CHECK(m.noinit[0] == 0u); /* noinit 不能跨硬件 RAM 清零保留。 */
    pass("hardware RAM clear also clears noinit storage");

    c.hardware_clears_ram = false; c.retained_ecc_valid = true;
    CHECK(Boot_Run(&m, &c) == BOOT_OK && m.noinit[1] == 42u);
    CHECK(m.ram_data[0] == UINT32_C(0x13579BDF) && m.ram_bss[0] == 0u);
    pass("warm retained RAM: data restored, bss cleared, noinit excluded from CRT");

    c.retained_ecc_valid = false;
    CHECK(Boot_Run(&m, &c) == BOOT_RAM_UNSAFE);
    CHECK(!m.stack_ready && m.ecum_calls == 0u && !m.os_owns_cpu);
    pass("unsafe RAM cannot become a stack or reach C/OS");

    c = Boot_DefaultConfig(); c.skip_data_copy = true;
    CHECK(Boot_Run(&m, &c) == BOOT_CRT_BAD && m.ecum_calls == 0u);
    pass("missing ROM-to-RAM copy prevents EcuM entry");

    c = Boot_DefaultConfig(); c.hardware_clears_ram = false;
    c.retained_ecc_valid = true; c.skip_bss_clear = true;
    CHECK(Boot_Run(&m, &c) == BOOT_CRT_BAD && m.ecum_calls == 0u);
    pass("warm boot exposes missing bss clear, not masked by cold reset zeros");

    c = Boot_DefaultConfig(); c.enable_ei_before_os = true;
    CHECK(Boot_Run(&m, &c) == BOOT_IRQ_EARLY && m.startos_calls == 0u);
    pass("this teaching profile prohibits premature EI enable");

    c = Boot_DefaultConfig(); c.opbt0 = UINT32_C(0x80000000);
    CHECK(Boot_Run(&m, &c) == BOOT_WDG_DEADLINE && !m.os_owns_cpu);
    c.stage_cost_us = 64u;
    CHECK(Boot_Run(&m, &c) == BOOT_WDG_DEADLINE);
    pass("64us auto watchdog rejects overrun and exact-deadline arrival");

    c = Boot_DefaultConfig(); c.opbt0 |= UINT32_C(1) << 21;
    CHECK(Boot_Run(&m, &c) == BOOT_OK && m.wdg_deadline_us == 262144u);
    pass("250kHz / OVF7 watchdog nominal timeout decoded");

    c = Boot_DefaultConfig(); c.opbt0 = 0u;
    CHECK(Boot_Run(&m, &c) == BOOT_OK && m.wdg_deadline_us == 0u);
    pass("software-start watchdog is not started by this model");

    c = Boot_DefaultConfig(); c.configuration_valid = false;
    CHECK(Boot_Run(&m, &c) == BOOT_CONFIGURATION_BAD && m.startos_calls == 0u);
    pass("invalid configuration stops before driver list and StartOS");

    c = Boot_DefaultConfig(); c.drivers_ready = false;
    CHECK(Boot_Run(&m, &c) == BOOT_DRIVER_FAILURE && m.startos_calls == 0u);
    pass("pre-OS driver failure blocks StartOS");

    c = Boot_DefaultConfig(); c.os_vectors_ready = false;
    CHECK(Boot_Run(&m, &c) == BOOT_VECTOR_MISSING && !m.os_owns_cpu);
    pass("calling StartOS alone is not proof of OS takeover");

    c = Boot_DefaultConfig(); c.startos_returns = true;
    CHECK(Boot_Run(&m, &c) == BOOT_STARTOS_RETURNED && !m.os_owns_cpu);
    pass("unexpected StartOS return is failure, not boot success");

    printf("ALL %u BOOT TESTS PASSED\n", passed);
    return 0;
}
