#include "boot_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ROM 初值与 RAM 存储分开，避免误以为 C 变量会靠硬件复位自动恢复初值。 */
static const uint32_t rom_data[4] = {UINT32_C(0x13579BDF), 500000u, 190u, 1u};
static const char *const stage_names[] = {
    "Reset vector / PSW.ID=1", "Assembly: initialize defined GPR values",
    "Early reset / watchdog policy", "RAM and ECC ready before stack use",
    "Set SP / ABI base registers", "CRT: copy .data ROM -> RAM",
    "CRT: clear .bss", "C entry / main: verify CRT and capture reset info",
    "EcuM_Init -> DriverInitZero (model)", "OS preparation, not scheduling yet",
    "Select and validate configuration", "DriverInitOne / pre-OS readiness",
    "EcuM calls StartOS(appMode)", "OS owns execution: STOP THIS LESSON"
};

const char *Boot_ErrorName(BootError error)
{
    static const char *const names[] = {
        "OK", "RAM/ECC unsafe", "CRT verification failed", "EI enabled too early",
        "watchdog deadline expired", "configuration invalid", "pre-OS driver failed",
        "OS vector binding missing", "StartOS unexpectedly returned"
    };
    return names[error];
}

BootConfig Boot_DefaultConfig(void)
{
    BootConfig config = {0};
    config.hardware_clears_ram = true;
    config.configuration_valid = true;
    config.drivers_ready = true;
    config.os_vectors_ready = true;
    /* 仅模型字段组合，不是可烧录的完整 OPBT0：自动启动、8 MHz、OVF7。 */
    config.opbt0 = UINT32_C(0x80000000) | (UINT32_C(7) << 25);
    config.reset_flags = UINT32_C(0x00000001); /* 只演示原始值保存，不解码原因。 */
    config.stage_cost_us = 100u;
    return config;
}

static bool fail(BootModel *model, const BootConfig *config, BootError error)
{
    model->error = error;
    if (config->trace) {
        printf("[BOOT FAIL] at stage=%u: %s; no OS takeover\n",
               (unsigned int)model->stage, Boot_ErrorName(error));
    }
    return false;
}

static bool step(BootModel *model, const BootConfig *config, BootStage stage)
{
    assert(model->history_count < sizeof(model->history) / sizeof(model->history[0]));
    model->stage = stage;
    model->history[model->history_count++] = stage;
    /* 正好到截止时间也判失败，避免演示一个没有裕量的“成功”。 */
    if (model->wdg_deadline_us != 0u &&
        config->stage_cost_us >= model->wdg_deadline_us - model->elapsed_us) {
        return fail(model, config, BOOT_WDG_DEADLINE);
    }
    if (UINT32_MAX - model->elapsed_us < config->stage_cost_us) {
        return fail(model, config, BOOT_WDG_DEADLINE);
    }
    model->elapsed_us += config->stage_cost_us;
    if (config->trace) {
        printf("[BOOT %02u] %-52s model_time=%lu us\n", (unsigned int)stage,
               stage_names[stage], (unsigned long)model->elapsed_us);
    }
    return true;
}

static bool StartOS_Model(BootModel *model, const BootConfig *config)
{
    model->startos_calls++;
    if (!step(model, config, BOOT_START_OS)) { return false; }
    if (!model->os_prepared || !config->os_vectors_ready) {
        return fail(model, config, BOOT_VECTOR_MISSING);
    }
    if (config->startos_returns) {
        return fail(model, config, BOOT_STARTOS_RETURNED);
    }
    /* 这里是模型中的 OS 内部所有权标记：并未执行 StartupHook 或任何任务。
     * 真正 StartOS 的成功路径不返回启动调用者；主机函数返回便于断言。 */
    if (!step(model, config, BOOT_OS_OWNED)) { return false; }
    model->os_owns_cpu = true;
    return true;
}

static bool EcuM_Init_Model(BootModel *model, const BootConfig *config)
{
    model->ecum_calls++;
    /* 顺序参考本地 openAUTOSAR；模型没有实现真实 InitOS/Os_IsrInit/MCAL。 */
    if (!step(model, config, BOOT_ECUM_ZERO)) { return false; }
    if (!step(model, config, BOOT_OS_PREPARE)) { return false; }
    model->os_prepared = true;
    if (!step(model, config, BOOT_CONFIGURATION)) { return false; }
    if (!config->configuration_valid) {
        return fail(model, config, BOOT_CONFIGURATION_BAD);
    }
    if (!step(model, config, BOOT_ECUM_ONE)) { return false; }
    if (!config->drivers_ready) { return fail(model, config, BOOT_DRIVER_FAILURE); }
    return StartOS_Model(model, config); /* main 不能再重复调用 StartOS。 */
}

BootError Boot_Run(BootModel *model, const BootConfig *config)
{
    unsigned int i;
    unsigned int ovf = (unsigned int)((config->opbt0 >> 25) & 7u);
    bool slow_wdg = (config->opbt0 & (UINT32_C(1) << 21)) != 0u;
    memset(model, 0, sizeof(*model));
    model->ei_masked = true;
    /* 模拟“复位前遗留的 RAM”，不是在真机 CRT 前使用 C memset。 */
    for (i = 0u; i < 4u; ++i) {
        model->ram_data[i] = UINT32_C(0xA5A5A5A5);
        model->ram_bss[i] = UINT32_C(0xA5A5A5A5);
    }
    model->noinit[0] = UINT32_C(0x5245544E);
    model->noinit[1] = 42u;
    if ((config->opbt0 & (UINT32_C(1) << 31)) != 0u) {
        /* 8 MHz: 64us << OVF；250 kHz: 2048us << OVF。
         * 模型无喂狗操作：只检查整个受控启动是否赶在第一次溢出前完成。
         * 起点为模型 reset 入口，真实预算还要扣除此前已消耗的时间和容差。 */
        model->wdg_deadline_us = (slow_wdg ? 2048u : 64u) << ovf;
    }

#define STEP(s) do { if (!step(model, config, (s))) { return model->error; } } while (0)
    STEP(BOOT_RESET);
    STEP(BOOT_GPR);
    model->gpr_ready = true;
    STEP(BOOT_EARLY_POLICY);
    STEP(BOOT_RAM);
    if (config->hardware_clears_ram) {
        memset(model->ram_data, 0, sizeof(model->ram_data));
        memset(model->ram_bss, 0, sizeof(model->ram_bss));
        memset(model->noinit, 0, sizeof(model->noinit)); /* .noinit 挡不住硬件清 RAM。 */
        model->ram_safe = true;
    } else {
        model->ram_safe = config->retained_ecc_valid;
    }
    if (!model->ram_safe) {
        (void)fail(model, config, BOOT_RAM_UNSAFE);
        return model->error;
    }
    STEP(BOOT_ABI);
    model->stack_ready = model->abi_ready = true;
    STEP(BOOT_DATA);
    if (!config->skip_data_copy) { memcpy(model->ram_data, rom_data, sizeof(rom_data)); }
    STEP(BOOT_BSS);
    if (!config->skip_bss_clear) { memset(model->ram_bss, 0, sizeof(model->ram_bss)); }
    STEP(BOOT_MAIN);
    model->crt_ready = memcmp(model->ram_data, rom_data, sizeof(rom_data)) == 0;
    for (i = 0u; i < 4u; ++i) { model->crt_ready &= model->ram_bss[i] == 0u; }
    if (!model->crt_ready) {
        (void)fail(model, config, BOOT_CRT_BAD);
        return model->error;
    }
    model->reset_snapshot = config->reset_flags;
    model->option_snapshot = config->opbt0;
    model->ei_masked = !config->enable_ei_before_os;
    if (!model->ei_masked) {
        (void)fail(model, config, BOOT_IRQ_EARLY);
        return model->error;
    }
    (void)EcuM_Init_Model(model, config);
    return model->error;
#undef STEP
}
