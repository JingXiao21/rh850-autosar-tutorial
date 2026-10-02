#ifndef ECU_BOOT_TARGET_H
#define ECU_BOOT_TARGET_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOOT_TARGET_CRT_CHECK = 1,
    BOOT_TARGET_SNAPSHOT,
    BOOT_TARGET_ENTER_ECUM,
    BOOT_TARGET_FAILED
} BootTargetStage;

typedef struct {
    uint32_t stage;
    uint32_t failure;
    uint32_t resf;
    uint32_t opbt0;
    uint32_t wdta0md;
    uint32_t data_probe;
    uint32_t bss_probe;
} BootTargetStatus;

extern volatile BootTargetStatus Boot_TargetStatus;
void Boot_EntryC(void);

/* 以下由真实 BSP/OS 集成提供，故意不提供“成功”的空桩。
 * 确认时钟/看门狗剩余预算/异常向量/EI 屏蔽/内存保护等前置条件。
 * 不在这里替代 EcuM 的 DriverInitList，更不能再调用一次 StartOS。 */
bool Boot_BspValidateBeforeEcuM(void);
/* 故障记录与受控处置，由工程定义；应不返回，不得无条件喂狗掩盖故障。 */
void Boot_BspFatal(uint32_t reason);

#endif
