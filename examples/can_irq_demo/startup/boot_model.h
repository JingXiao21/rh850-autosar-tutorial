#ifndef ECU_BOOT_MODEL_H
#define ECU_BOOT_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* 全部是 PC 教学模型的状态，不是 AUTOSAR 或 RH850 的寄存器定义。 */
typedef enum {
    BOOT_RESET, BOOT_GPR, BOOT_EARLY_POLICY, BOOT_RAM, BOOT_ABI,
    BOOT_DATA, BOOT_BSS, BOOT_MAIN, BOOT_ECUM_ZERO, BOOT_OS_PREPARE,
    BOOT_CONFIGURATION, BOOT_ECUM_ONE, BOOT_START_OS, BOOT_OS_OWNED
} BootStage;

typedef enum {
    BOOT_OK, BOOT_RAM_UNSAFE, BOOT_CRT_BAD, BOOT_IRQ_EARLY,
    BOOT_WDG_DEADLINE, BOOT_CONFIGURATION_BAD, BOOT_DRIVER_FAILURE,
    BOOT_VECTOR_MISSING, BOOT_STARTOS_RETURNED
} BootError;

typedef struct {
    bool hardware_clears_ram;
    bool retained_ecc_valid;
    bool skip_data_copy;              /* 以下若干开关只用于故障注入。 */
    bool skip_bss_clear;
    bool enable_ei_before_os;
    bool configuration_valid;
    bool drivers_ready;
    bool os_vectors_ready;
    bool startos_returns;
    uint32_t opbt0;
    uint32_t reset_flags;
    uint32_t stage_cost_us;            /* 人工设置的模拟时间，不是实测 WCET。 */
    bool trace;
} BootConfig;

typedef struct {
    BootStage stage;
    BootError error;
    BootStage history[16];
    unsigned int history_count;
    bool gpr_ready, ram_safe, stack_ready, abi_ready, crt_ready;
    bool ei_masked, os_prepared, os_owns_cpu;
    uint32_t ram_data[4], ram_bss[4], noinit[2];
    uint32_t reset_snapshot, option_snapshot;
    uint32_t elapsed_us, wdg_deadline_us;
    uint32_t ecum_calls, startos_calls;
} BootModel;

BootConfig Boot_DefaultConfig(void);
/* 从新的一次模拟复位执行到 OS 所有权边界或失败。函数返回仅为主机测试。 */
BootError Boot_Run(BootModel *model, const BootConfig *config);
const char *Boot_ErrorName(BootError error);

#endif
