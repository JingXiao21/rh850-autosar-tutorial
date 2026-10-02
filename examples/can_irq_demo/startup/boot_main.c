#include "boot_model.h"
#include <stdio.h>

int main(void)
{
    BootModel model;
    BootConfig config = Boot_DefaultConfig();
    config.trace = true;
    puts("=== Cold reset -> CRT -> EcuM -> StartOS -> OS ownership ===");
    if (Boot_Run(&model, &config) != BOOT_OK || !model.os_owns_cpu) { return 1; }
    printf("[STOP] EcuM calls=%lu, StartOS calls=%lu; no StartupHook, tasks or post-OS BSW work\n",
           (unsigned long)model.ecum_calls, (unsigned long)model.startos_calls);

    puts("\n=== Warm-reset example: retained RAM still needs .bss initialization ===");
    config.hardware_clears_ram = false;
    config.retained_ecc_valid = true;
    if (Boot_Run(&model, &config) != BOOT_OK) { return 1; }
    printf("[RETAINED] noinit marker=0x%08lX; bss[0]=%lu\n",
           (unsigned long)model.noinit[0], (unsigned long)model.ram_bss[0]);

    puts("\n=== Auto-start watchdog with only 64 us: refuse to claim OS takeover ===");
    config = Boot_DefaultConfig();
    config.trace = true;
    config.opbt0 = UINT32_C(0x80000000); /* 模型 OVF0、8MHz：64us。 */
    if (Boot_Run(&model, &config) != BOOT_WDG_DEADLINE || model.os_owns_cpu) { return 1; }
    return 0;
}
