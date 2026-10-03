/*
 * Adc.c
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: ADC Driver (AUTOSAR_CP_SWS_ADCDriver, R25-11). Implemented: ONE-SHOT, SOFTWARE-TRIGGERED conversion groups of
 * ONE channel each, "single access" result buffer of one value, polling of the completion (no notification, no DMA, no streaming).
 * Adc_Init, Adc_DeInit, Adc_SetupResultBuffer, Adc_StartGroupConversion, Adc_StopGroupConversion, Adc_ReadGroup, Adc_GetGroupStatus,
 * Adc_GetVersionInfo. Group state machine: ADC_IDLE -> (Start) ADC_BUSY -> (EOC seen) ADC_COMPLETED -> (ReadGroup) ADC_IDLE.
 *
 * Hardware: STM32L552 ADC1 @ 0x42028000 (Cortex-M33 AHB2). Sequence = RM0438 "ADC on/off control": leave deep power-down, enable the
 * voltage regulator, calibrate (CR.ADCAL), enable (CR.ADEN, wait ISR.ADRDY); per conversion: SMPRx (sample time of the channel),
 * SQR1 (L = 0, SQ1 = channel), ISR flags cleared, CR.ADSTART, wait ISR.EOC, read DR (12 bit right aligned, 0..4095).
 *   ISR 0x00 (ADRDY b0, EOSMP b1, EOC b2, EOS b3)  CR 0x08 (ADEN b0, ADSTART b2, ADSTP b4, ADVREGEN b28, DEEPPWD b29, ADCAL b31)
 *   CFGR 0x0C (all zero: 12 bit, right aligned, single, software trigger)  SMPR1 0x14 (channels 0..9, 3 bits each)  SMPR2 0x18 (10..18)
 *   SQR1 0x30 (L[3:0], SQ1[10:6])  DR 0x40   ADC12_COMMON CCR 0x42028308 (CKMODE[17:16] = 3: HCLK/4)
 * RENODE FACTS, VERIFIED with target/renode/probe (Analog.STM32L5_ADC): ADEN -> ADRDY immediately; ADCAL is accepted but ignored (bit
 * stays 0); ADSTART completes at once: ISR = 0x0F (ADRDY|EOSMP|EOC|EOS); DR = floor(uV * 4096 / 3.3 V) clipped to 4095 for the voltage
 * set with `sysbus.adc1 SetVoltage <MICROVOLT> <channel>` - NOTE the unit is MICROvolt, not millivolt (DESIGN 11.2 assumed mV);
 * ISR flags are cleared by writing 1 (only bits 0..9 are implemented); channel numbering in SetVoltage = ADC channel number (6 = PA1).
 * [R] only: calibration timing, sample-time effect, CCR, clock enable RCC_AHB2ENR.ADCEN (bit 13).
 * RH850 analogue: A/D converter ADCA0 with scan groups (SGSTCR start, ADCAnDR result registers, SGCR trigger select) - docs/11-classic-autosar-primer/08.
 * Spec: AUTOSAR_CP_SWS_ADCDriver SWS_Adc_00365 Adc_Init, 00366 Adc_DeInit, 91000 Adc_SetupResultBuffer, 00367 Adc_StartGroupConversion,
 *       00368 Adc_StopGroupConversion, 00369 Adc_ReadGroup, 00374 Adc_GetGroupStatus, SWS_Adc_00431 result buffer pointer reset.
 */
#include "Adc.h"
#include "Mmio.h"
#include "Det.h"
#include "Mini_ModuleIds.h"

#define ADC_E_UNINIT                0x0Au
#define ADC_E_BUSY                  0x0Bu
#define ADC_E_IDLE                  0x0Cu
#define ADC_E_ALREADY_INITIALIZED   0x0Du
#define ADC_E_PARAM_POINTER         0x14u
#define ADC_E_PARAM_GROUP           0x15u
#define ADC_E_BUFFER_UNINIT         0x19u
#define ADC_E_PERIPHERAL_NOT_PREPARED 0x1Du

#define ADC_SID_INIT                0x00u
#define ADC_SID_DEINIT              0x01u
#define ADC_SID_STARTGROUP          0x02u
#define ADC_SID_STOPGROUP           0x03u
#define ADC_SID_READGROUP           0x04u
#define ADC_SID_GETGROUPSTATUS      0x09u
#define ADC_SID_GETVERSIONINFO      0x0Au
#define ADC_SID_SETUPRESULTBUFFER   0x0Cu

#define ADC1_BASE       0x42028000u
#define ADC_ISR         (ADC1_BASE + 0x00u)
#define ADC_CR          (ADC1_BASE + 0x08u)
#define ADC_CFGR        (ADC1_BASE + 0x0Cu)
#define ADC_SMPR1       (ADC1_BASE + 0x14u)
#define ADC_SMPR2       (ADC1_BASE + 0x18u)
#define ADC_SQR1        (ADC1_BASE + 0x30u)
#define ADC_DR          (ADC1_BASE + 0x40u)
#define ADC12_CCR       (ADC1_BASE + 0x308u)
#define RCC_AHB2ENR     0x4002104Cu

#define ISR_ADRDY       (1u << 0)
#define ISR_EOC         (1u << 2)
#define ISR_CLEAR_ALL   0x1Fu            /* ADRDY EOSMP EOC EOS OVR, write 1 to clear */
#define CR_ADEN         (1u << 0)
#define CR_ADDIS        (1u << 1)
#define CR_ADSTART      (1u << 2)
#define CR_ADSTP        (1u << 4)
#define CR_ADVREGEN     (1u << 28)
#define CR_DEEPPWD      (1u << 29)
#define CR_ADCAL        (1u << 31)

#define ADC_MAX_GROUPS      4u
#define ADC_WAIT_LOOPS      100000u
#define ADC_VREG_LOOPS      2000u        /* >= 20 us regulator start-up (tADCVREG_STUP) at 80 MHz; bounded busy wait */

#define ADC_DET(api, err) \
    do { if ((MINI_DEV_ERROR_DETECT) == STD_ON) { (void)Det_ReportError(MINI_MODULE_ADC, 0u, (api), (err)); } } while (0)

static const Adc_ConfigType *adc_cfg;
static Adc_StatusType        adc_status[ADC_MAX_GROUPS];
static Adc_ValueGroupType   *adc_buffer[ADC_MAX_GROUPS];

static boolean adc_wait(uint32 addr, uint32 mask, uint32 value)
{
    uint32 n;
    for (n = 0u; n < ADC_WAIT_LOOPS; n++) {
        if ((Mmio_Read32(addr) & mask) == value) {
            return TRUE;
        }
    }
    return FALSE;
}

static boolean adc_group_ok(Adc_GroupType group, uint8 api)
{
    if (adc_cfg == NULL_PTR) {
        ADC_DET(api, ADC_E_UNINIT);
        return FALSE;
    }
    if ((group >= adc_cfg->numGroups) || (group >= ADC_MAX_GROUPS)) {
        ADC_DET(api, ADC_E_PARAM_GROUP);
        return FALSE;
    }
    return TRUE;
}

/* Result is latched by the driver as soon as EOC is observed (polling mode). */
static void adc_poll_group(Adc_GroupType g)
{
    if ((adc_status[g] == ADC_BUSY) && ((Mmio_Read32(ADC_ISR) & ISR_EOC) != 0u)) {
        *adc_buffer[g]  = (Adc_ValueGroupType)(Mmio_Read32(ADC_DR) & 0xFFFu);    /* reading DR also clears EOC on silicon */
        Mmio_Write32(ADC_ISR, ISR_CLEAR_ALL & ~ISR_ADRDY);
        adc_status[g]   = ADC_COMPLETED;
    }
}

void Adc_Init(const Adc_ConfigType *ConfigPtr)
{
    uint32 n;
    if (ConfigPtr == NULL_PTR) {
        ADC_DET(ADC_SID_INIT, ADC_E_PARAM_POINTER);
        return;
    }
    if (adc_cfg != NULL_PTR) {
        ADC_DET(ADC_SID_INIT, ADC_E_ALREADY_INITIALIZED);
        return;
    }
    Mmio_Modify32(RCC_AHB2ENR, 0u, 1u << 13);                 /* ADCEN [R] */
    Mmio_Modify32(ADC12_CCR, 3u << 16, 3u << 16);             /* CKMODE: HCLK/4 [R] */
    Mmio_Modify32(ADC_CR, CR_DEEPPWD, 0u);                    /* leave deep power-down ... */
    Mmio_Modify32(ADC_CR, 0u, CR_ADVREGEN);                   /* ... start the internal voltage regulator */
    for (n = 0u; n < ADC_VREG_LOOPS; n++) {
        (void)Mmio_Read32(ADC_ISR);                           /* volatile access = the delay */
    }
    Mmio_Modify32(ADC_CR, 0u, CR_ADCAL);                      /* single-ended calibration */
    (void)adc_wait(ADC_CR, CR_ADCAL, 0u);
    Mmio_Write32(ADC_ISR, ISR_ADRDY);                         /* clear stale ADRDY, then enable */
    Mmio_Modify32(ADC_CR, 0u, CR_ADEN);
    if (!adc_wait(ADC_ISR, ISR_ADRDY, ISR_ADRDY)) {
        ADC_DET(ADC_SID_INIT, ADC_E_PERIPHERAL_NOT_PREPARED);
        return;
    }
    Mmio_Write32(ADC_CFGR, 0u);                               /* 12 bit, right aligned, single conversion, software trigger */
    for (n = 0u; n < ADC_MAX_GROUPS; n++) {
        adc_status[n] = ADC_IDLE;
        adc_buffer[n] = NULL_PTR;
    }
    adc_cfg = ConfigPtr;
}

void Adc_DeInit(void)
{
    if (adc_cfg == NULL_PTR) {
        ADC_DET(ADC_SID_DEINIT, ADC_E_UNINIT);
        return;
    }
    Mmio_Modify32(ADC_CR, 0u, CR_ADDIS);
    (void)adc_wait(ADC_CR, CR_ADEN, 0u);
    Mmio_Modify32(ADC_CR, CR_ADVREGEN, CR_DEEPPWD);
    adc_cfg = NULL_PTR;
}

Std_ReturnType Adc_SetupResultBuffer(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr)
{
    if (!adc_group_ok(Group, ADC_SID_SETUPRESULTBUFFER)) {
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL_PTR) {
        ADC_DET(ADC_SID_SETUPRESULTBUFFER, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    adc_buffer[Group] = DataBufferPtr;
    return E_OK;
}

void Adc_StartGroupConversion(Adc_GroupType Group)
{
    uint32 ch;
    if (!adc_group_ok(Group, ADC_SID_STARTGROUP)) {
        return;
    }
    if (adc_buffer[Group] == NULL_PTR) {
        ADC_DET(ADC_SID_STARTGROUP, ADC_E_BUFFER_UNINIT);
        return;
    }
    if (adc_status[Group] == ADC_BUSY) {
        ADC_DET(ADC_SID_STARTGROUP, ADC_E_BUSY);
        return;
    }
    ch = (uint32)(adc_cfg->groups[Group].channel & 0x1Fu);
    if (ch < 10u) {                                           /* sample time of the channel: 3 bits in SMPR1 (0..9) / SMPR2 (10..18) */
        Mmio_Modify32(ADC_SMPR1, 7u << (3u * ch), ((uint32)adc_cfg->groups[Group].sampleTime & 7u) << (3u * ch));
    } else {
        Mmio_Modify32(ADC_SMPR2, 7u << (3u * (ch - 10u)), ((uint32)adc_cfg->groups[Group].sampleTime & 7u) << (3u * (ch - 10u)));
    }
    Mmio_Write32(ADC_SQR1, ch << 6);                          /* sequence length 1 (L = 0), first conversion = ch */
    Mmio_Write32(ADC_ISR, ISR_CLEAR_ALL & ~ISR_ADRDY);        /* clear stale EOC/EOS (write 1 to clear) */
    adc_status[Group] = ADC_BUSY;
    Mmio_Modify32(ADC_CR, 0u, CR_ADSTART);
}

void Adc_StopGroupConversion(Adc_GroupType Group)
{
    if (!adc_group_ok(Group, ADC_SID_STOPGROUP)) {
        return;
    }
    if (adc_status[Group] == ADC_BUSY) {
        Mmio_Modify32(ADC_CR, 0u, CR_ADSTP);
    }
    adc_status[Group] = ADC_IDLE;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group)
{
    if (!adc_group_ok(Group, ADC_SID_GETGROUPSTATUS)) {
        return ADC_IDLE;
    }
    adc_poll_group(Group);
    return adc_status[Group];
}

Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr)
{
    if (!adc_group_ok(Group, ADC_SID_READGROUP)) {
        return E_NOT_OK;
    }
    if (DataBufferPtr == NULL_PTR) {
        ADC_DET(ADC_SID_READGROUP, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    adc_poll_group(Group);
    if (adc_status[Group] == ADC_IDLE) {
        ADC_DET(ADC_SID_READGROUP, ADC_E_IDLE);               /* nothing was started / result already read */
        return E_NOT_OK;
    }
    if (adc_status[Group] != ADC_COMPLETED) {                 /* still converting */
        return E_NOT_OK;
    }
    *DataBufferPtr    = *adc_buffer[Group];
    adc_status[Group] = ADC_IDLE;                             /* single access one-shot: result consumed */
    return E_OK;
}

void Adc_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
    if (versioninfo == NULL_PTR) {
        ADC_DET(ADC_SID_GETVERSIONINFO, ADC_E_PARAM_POINTER);
        return;
    }
    versioninfo->vendorID         = MINI_VENDOR_ID;
    versioninfo->moduleID         = MINI_MODULE_ADC;
    versioninfo->sw_major_version = 1u;
    versioninfo->sw_minor_version = 0u;
    versioninfo->sw_patch_version = 0u;
}
