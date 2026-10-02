#ifndef RH850_CAN_REGS_H
#define RH850_CAN_REGS_H

#include <stdint.h>

/* RH850/P1M-E，HW-E Rev.1.20。固定使用 RCMC=1 的 FD 寄存器接口，
 * 本课只向上交付 Classical 数据帧。接口模式与每帧的 FDF 不是同一概念。 */
#define CAN_RX_EI              190u
#define WRONG_EI                90u
#define EI_CHANNELS            384u
#define EIC190_ADDRESS         UINT32_C(0xFFFFB17C) /* 16 位访问 */
#define EIC_EIMK               UINT16_C(0x0080)
#define EIC_EITB               UINT16_C(0x0040)
#define EIC_EICT               UINT16_C(0x8000)
#define EIC_EIRF               UINT16_C(0x1000)

/* 下列 CAN 寄存器在本示例中统一使用 32 位访问。只使用 FIFO0 / AFL0。 */
#define REG_GRMCFG             UINT32_C(0xFFD204FC)
#define REG_GAFLECTR           UINT32_C(0xFFD20098)
#define REG_GAFLCFG0           UINT32_C(0xFFD2009C)
#define REG_GAFLID0            UINT32_C(0xFFD21000)
#define REG_GAFLM0             UINT32_C(0xFFD21004)
#define REG_GAFLP0_0           UINT32_C(0xFFD21008)
#define REG_GAFLP1_0           UINT32_C(0xFFD2100C)
#define REG_RFCC0              UINT32_C(0xFFD200B8)
#define REG_RFSTS0             UINT32_C(0xFFD200D8)
#define REG_RFPCTR0            UINT32_C(0xFFD200F8)
#define REG_RFID0              UINT32_C(0xFFD23000)
#define REG_RFPTR0             UINT32_C(0xFFD23004)
#define REG_RFFDSTS0           UINT32_C(0xFFD23008)
#define REG_RFDF0_0            UINT32_C(0xFFD2300C)
#define REG_RFDF1_0            UINT32_C(0xFFD23010)

#define RFCC_RFE               UINT32_C(1)
#define RFCC_RFIE              (UINT32_C(1) << 1)
#define RFCC_RFIM              (UINT32_C(1) << 12)
#define RFCC_DEPTH8            (UINT32_C(2) << 8)
#define RFSTS_RFEMP            UINT32_C(1)
#define RFSTS_RFFLL            (UINT32_C(1) << 1)
#define RFSTS_RFMLT            (UINT32_C(1) << 2)
#define RFSTS_RFIF             (UINT32_C(1) << 3)
#define RFID_IDE               (UINT32_C(1) << 31)
#define RFID_RTR               (UINT32_C(1) << 30)
#define RFFDSTS_FDF            (UINT32_C(1) << 2)
#define AFL_WRITE_ENABLE       (UINT32_C(1) << 8)

#endif
