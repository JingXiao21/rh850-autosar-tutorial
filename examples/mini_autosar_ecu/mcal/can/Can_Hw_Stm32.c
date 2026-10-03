/*
 * Can_Hw_Stm32.c   (TARGET ONLY: file name contains _Stm32)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the controller specific part of a CAN MCAL driver (AUTOSAR_CP_SWS_CANDriver leaves it to the vendor).
 * Hardware: STM32L552 FDCAN1 (Bosch M_CAN IP) in classic-CAN mode (CCCR.FDOE = 0). RH850 analogue: RS-CANFD
 * (docs/04-can-mcal/02-rh850-can-peripheral.md): CCCR.INIT/CCE ~ channel reset/halt mode, NBTP ~ CmNCFG, standard filters ~ receive
 * rules (GAFLID/GAFLM), Rx FIFO0 ~ RX FIFO (RFSTS/RFPCTR), Tx buffers + TXBAR ~ TX message buffers + TMTRR.
 *
 * Register block FDCAN1 @ 0x4000A400, message RAM @ 0x4000AC00 (0x400 bytes in Renode, 0x350 used). Offsets are those of the M_CAN
 * IP as used on STM32G4/L5 (RM0438 "FDCAN registers"). Status of every fact used below:
 *   [V] = VERIFIED empirically in Renode 1.17.0 (target/renode/probe/probe.c and target/renode/smoke/; ramn.repl, series .L5)
 *   [R] = from the reference manual / M_CAN spec only; Renode does not model it or it was not exercised -> UNVERIFIED on silicon
 *
 *   0x00 CREL  [V] reads 0x32141218          0x04 ENDN [V] 0x87654321
 *   0x18 CCCR  [V] reset 0x1 (INIT=1); INIT bit0, CCE bit1, FDOE bit8, BRSE bit9; INIT/CCE handshake is immediate in Renode
 *   0x1C NBTP  [V] reset 0x06000A03; NSJW[31:25] NBRP[24:16] NTSEG1[15:8] NTSEG2[6:0], all stored as value-1
 *   0x44 PSR   BO = bit7 [R]
 *   0x50 IR    RF0N = bit0, TC = bit9, BO = bit25 (write 1 to clear) [V for RF0N/TC]; Renode left an unnamed IR bit 7 set (ignored)
 *   0x54 IE    RF0NE = bit0 [V]    0x58 ILS (all lines 0)    0x5C ILE EINT0 = bit0 [V]
 *   0x80 RXGFC [V] ANFS[5:4], ANFE[3:2] = 2 (reject non matching), RRFS bit1, RRFE bit0,
 *                  LSS[20:16] = standard filter count, LSE[27:24] = extended filter count (NOT the other way round: first guess was wrong,
 *                  Renode's DumpDoubleWordRegister showed LSS at 16-20 and silently dropped every frame)
 *   0x90 RXF0S [V] F0FL fill level, F0GI get index, F0PI put index (STM32 widths: F0FL[3:0], F0GI[9:8], F0PI[17:16]; wider masks used below are harmless)     0x94 RXF0A = get index to acknowledge
 *   0xC4 TXFQS [V] reset 0x3 (TFFL = 3 free), TFGI[9:8], TFQPI[20:16] put index, TFQF bit21 FIFO full
 *   0xCC TXBAR add request bit n   0xC8 TXBRP pending   0xD4 TXBTO transmission occurred   0xD0 TXBCR cancel
 *   0x100 CKDIV (FDCAN_CKDIV) [R]
 * Message RAM layout is FIXED on G4/L5 [R, matches Renode, see smoke test]: std filters 0x000 (28 x 1 word), ext filters 0x070,
 * RX FIFO0 0x0B0 (3 x 18 words), RX FIFO1 0x188, TX event FIFO 0x260, TX buffers 0x278 (3 x 18 words).
 * Element formats: std filter word = SFT[31:30] SFEC[29:27] SFID1[26:16] SFID2[10:0] (SFT=2 classic: id + mask, SFEC=1 -> FIFO0);
 *   RX element R0 = XTD b30, RTR b29, id[28:18] (std id), R1 = DLC[19:16], R2/R3 data bytes little endian;
 *   TX element T0 = id[28:18], T1 = MM[31:24] DLC[19:16] (FDF=BRS=EFC=0), T2/T3 data.
 * Clock: RCC APB1ENR1.FDCANEN (bit 25) [R]; kernel clock select RCC_CCIPR1.FDCANSEL [25:24] = 01 (PLL "Q") [R, assumption taken from
 * the G4/L4+ family; Renode's RCC does not implement these registers (they read 0), so nothing is verified]. Bit timing assumes the
 * 80 MHz kernel clock established by Mcu_InitClock.
 *
 * What Renode's STM32_FDCAN actually needs (VERIFIED, see smoke test): CCCR.INIT/CCE, NBTP (any value accepted), RXGFC + std filter
 * elements in message RAM (filters ARE evaluated), IE/ILE for the interrupt, TXFQS put index + TXBAR for transmit, RXF0S/RXF0A for
 * receive, IR write-1-to-clear. CKDIV, TXBC and the clock registers are not needed.
 * Owner: agent B.
 */
#include "Can_Hw.h"
#include "Mmio.h"

#define FDCAN1_BASE     0x4000A400u
#define FDCAN_MSGRAM    0x4000AC00u
#define RCC_BASE        0x40021000u

#define FD_CCCR         (FDCAN1_BASE + 0x018u)
#define FD_NBTP         (FDCAN1_BASE + 0x01Cu)
#define FD_PSR          (FDCAN1_BASE + 0x044u)
#define FD_IR           (FDCAN1_BASE + 0x050u)
#define FD_IE           (FDCAN1_BASE + 0x054u)
#define FD_ILS          (FDCAN1_BASE + 0x058u)
#define FD_ILE          (FDCAN1_BASE + 0x05Cu)
#define FD_RXGFC        (FDCAN1_BASE + 0x080u)
#define FD_RXF0S        (FDCAN1_BASE + 0x090u)
#define FD_RXF0A        (FDCAN1_BASE + 0x094u)
#define FD_TXFQS        (FDCAN1_BASE + 0x0C4u)
#define FD_TXBRP        (FDCAN1_BASE + 0x0C8u)
#define FD_TXBAR        (FDCAN1_BASE + 0x0CCu)
#define FD_TXBCR        (FDCAN1_BASE + 0x0D0u)
#define FD_TXBTO        (FDCAN1_BASE + 0x0D4u)

#define CCCR_INIT       (1u << 0)
#define CCCR_CCE        (1u << 1)
#define CCCR_FDOE       (1u << 8)
#define CCCR_BRSE       (1u << 9)
#define IR_RF0N         (1u << 0)
#define IR_RF0L         (1u << 3)
#define IR_TC           (1u << 9)
#define PSR_BO          (1u << 7)

#define RAM_SIDF_BASE   (FDCAN_MSGRAM + 0x000u)      /* standard filter elements */
#define RAM_SIDF_MAX    28u
#define RAM_RXF0_BASE   (FDCAN_MSGRAM + 0x0B0u)
#define RAM_RXF_ELEM    72u                          /* 18 words */
#define RAM_TXB_BASE    (FDCAN_MSGRAM + 0x278u)
#define RAM_TXB_ELEM    72u

#define RCC_APB1ENR1    (RCC_BASE + 0x058u)
#define RCC_CCIPR1      (RCC_BASE + 0x088u)

#define CAN_KERNEL_CLOCK_HZ   80000000u              /* Mcu_InitClock: PLL 80 MHz -> FDCANSEL = PLL "Q" 80 MHz */
#define CAN_HW_WAIT_LOOPS     20000u                 /* bounded wait for INIT handshake (SWS_Can_00398 uses an OS counter) */

static uint32 hw_txPendingMask;                      /* buffers with a request outstanding (bit n = buffer n) */
static uint32 hw_rxLost;

static boolean hw_wait_cccr_init(boolean wantSet)
{
    uint32 n;
    for (n = 0u; n < CAN_HW_WAIT_LOOPS; n++) {
        boolean isSet = ((Mmio_Read32(FD_CCCR) & CCCR_INIT) != 0u) ? TRUE : FALSE;
        if (isSet == wantSet) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Nominal bit timing: 16 time quanta per bit, sample point 87.5 % (seg1 = 13, seg2 = 2), as used by most 500 kbit/s networks. */
static boolean hw_calc_nbtp(uint32 baud, uint32 *nbtp)
{
    uint32 tq;
    for (tq = 16u; tq >= 8u; tq--) {
        uint32 div = baud * tq;
        if ((div != 0u) && ((CAN_KERNEL_CLOCK_HZ % div) == 0u)) {
            uint32 brp   = CAN_KERNEL_CLOCK_HZ / div;
            uint32 seg1  = (tq * 7u) / 8u - 1u;          /* tq 16 -> 13 */
            uint32 seg2  = tq - 1u - seg1;               /* tq 16 -> 2  */
            uint32 sjw   = (seg2 > 4u) ? 4u : seg2;
            if ((brp >= 1u) && (brp <= 512u)) {
                *nbtp = ((sjw - 1u) << 25) | ((brp - 1u) << 16) | ((seg1 - 1u) << 8) | (seg2 - 1u);
                return TRUE;
            }
        }
    }
    return FALSE;
}

Std_ReturnType CanHw_Init(const Can_ConfigType *cfg)
{
    uint32 nbtp = 0u;
    uint32 nf = 0u;
    uint8  i;

    if (!hw_calc_nbtp(cfg->baudrate, &nbtp)) {
        return E_NOT_OK;                                   /* CAN_E_PARAM_BAUDRATE in a real driver */
    }
    hw_txPendingMask = 0u;
    hw_rxLost        = 0u;

    Mmio_Modify32(RCC_APB1ENR1, 0u, 1u << 25);             /* FDCANEN [R] */
    Mmio_Modify32(RCC_CCIPR1, 3u << 24, 1u << 24);         /* FDCANSEL = PLL "Q" [R] */

    Mmio_Modify32(FD_CCCR, 0u, CCCR_INIT);                 /* enter initialisation mode ... */
    if (!hw_wait_cccr_init(TRUE)) {
        return E_NOT_OK;
    }
    Mmio_Modify32(FD_CCCR, 0u, CCCR_CCE);                  /* ... and unlock the protected configuration registers */
    Mmio_Modify32(FD_CCCR, CCCR_FDOE | CCCR_BRSE, 0u);     /* classic CAN: no FD operation, no bit rate switching */
    Mmio_Write32(FD_NBTP, nbtp);

    /* One classic standard-id filter (id + mask, store in RX FIFO0) per receive HOH: the "CanHardwareObject" of AUTOSAR. */
    for (i = 0u; (i < cfg->numHoh) && (nf < RAM_SIDF_MAX); i++) {
        if (cfg->hoh[i].type == CAN_HOH_RECEIVE) {
            Mmio_Write32(RAM_SIDF_BASE + 4u * nf,
                         (2u << 30) | (1u << 27) | ((cfg->hoh[i].canId & 0x7FFu) << 16) | (cfg->hoh[i].filterMask & 0x7FFu));
            nf++;
        }
    }
    /* RXGFC: list size, non matching std/ext frames rejected (ANFS = ANFE = 2), remote frames rejected, FIFO0 blocking mode */
    Mmio_Write32(FD_RXGFC, (nf << 16) | (2u << 4) | (2u << 2) | (1u << 1) | (1u << 0));

    Mmio_Write32(FD_IR, 0xFFFFFFFFu);
    Mmio_Write32(FD_ILS, 0u);                              /* all interrupt sources on line 0 -> IRQ 39 FDCAN1_IT0 */
    Mmio_Write32(FD_IE, cfg->rxInterrupt ? IR_RF0N : 0u);  /* RX FIFO0 new message */
    Mmio_Write32(FD_ILE, cfg->rxInterrupt ? 1u : 0u);      /* EINT0 */
    return E_OK;                                           /* stays in INIT mode == CAN_CS_STOPPED */
}

Std_ReturnType CanHw_Start(void)
{
    Mmio_Modify32(FD_CCCR, CCCR_INIT, 0u);                 /* leave init mode: controller joins the bus */
    return hw_wait_cccr_init(FALSE) ? E_OK : E_NOT_OK;
}

Std_ReturnType CanHw_Stop(void)
{
    Mmio_Write32(FD_TXBCR, 0x7u);                          /* cancel pending requests */
    Mmio_Modify32(FD_CCCR, 0u, CCCR_INIT);
    if (!hw_wait_cccr_init(TRUE)) {
        return E_NOT_OK;
    }
    Mmio_Modify32(FD_CCCR, 0u, CCCR_CCE);
    hw_txPendingMask = 0u;
    return E_OK;
}

Std_ReturnType CanHw_TxRequest(Can_IdType id, uint8 dlc, const uint8 *data, uint8 *bufIdx)
{
    uint32 fqs = Mmio_Read32(FD_TXFQS);
    uint32 idx;
    uint32 base;
    uint32 w0 = 0u;
    uint32 w1 = 0u;
    uint8  i;

    if ((fqs & (1u << 21)) != 0u) {                        /* TFQF: TX FIFO/queue full */
        return E_NOT_OK;
    }
    idx  = (fqs >> 16) & 0x1Fu;                            /* TFQPI: element the hardware wants to be filled next */
    if (idx >= CAN_HW_TX_BUFFERS) {
        return E_NOT_OK;
    }
    base = RAM_TXB_BASE + idx * RAM_TXB_ELEM;
    for (i = 0u; i < dlc; i++) {
        if (i < 4u) { w0 |= ((uint32)data[i]) << (8u * i); }
        else        { w1 |= ((uint32)data[i]) << (8u * (i - 4u)); }
    }
    Mmio_Write32(base + 0u,  (id & 0x7FFu) << 18);         /* T0: standard id, data frame, no ESI */
    Mmio_Write32(base + 4u,  ((uint32)dlc << 16) | (idx << 24));   /* T1: DLC, MM = buffer index, no FD/BRS/event */
    Mmio_Write32(base + 8u,  w0);
    Mmio_Write32(base + 12u, w1);
    Mmio_Write32(FD_TXBAR, 1u << idx);                     /* "add request": hardware arbitrates and sends */
    hw_txPendingMask |= (1u << idx);
    *bufIdx = (uint8)idx;
    return E_OK;
}

boolean CanHw_TxIsDone(uint8 bufIdx)
{
    uint32 m = 1u << bufIdx;
    if (((hw_txPendingMask & m) != 0u) && ((Mmio_Read32(FD_TXBRP) & m) == 0u) && ((Mmio_Read32(FD_TXBTO) & m) != 0u)) {
        hw_txPendingMask &= ~m;
        Mmio_Write32(FD_IR, IR_TC);
        return TRUE;
    }
    return FALSE;
}

boolean CanHw_RxFetch(CanHw_FrameType *frame)
{
    uint32 s = Mmio_Read32(FD_RXF0S);
    uint32 gi;
    uint32 base;
    uint32 r0;
    uint32 r1;
    uint32 d[2];
    uint8  i;

    if ((s & 0x7Fu) == 0u) {                               /* F0FL == 0: empty */
        return FALSE;
    }
    gi   = (s >> 8) & 0x3Fu;
    base = RAM_RXF0_BASE + gi * RAM_RXF_ELEM;
    r0   = Mmio_Read32(base + 0u);
    r1   = Mmio_Read32(base + 4u);
    d[0] = Mmio_Read32(base + 8u);
    d[1] = Mmio_Read32(base + 12u);
    Mmio_Write32(FD_RXF0A, gi);                            /* free the element (advances the get index) */

    frame->id  = (r0 >> 18) & 0x7FFu;
    frame->dlc = (uint8)((r1 >> 16) & 0xFu);
    if (frame->dlc > 8u) {
        frame->dlc = 8u;                                   /* DLC codes 9..15 mean 8 bytes in classic CAN */
    }
    for (i = 0u; i < 8u; i++) {
        frame->data[i] = (uint8)(d[i / 4u] >> (8u * (i % 4u)));
    }
    return TRUE;
}

void CanHw_RxIrqAck(void)
{
    uint32 ir = Mmio_Read32(FD_IR);
    if ((ir & IR_RF0L) != 0u) {
        hw_rxLost++;                                       /* RX FIFO0 message lost */
    }
    Mmio_Write32(FD_IR, IR_RF0N | IR_RF0L | (1u << 1) | (1u << 2));   /* RF0N, RF0W, RF0F, RF0L: write 1 to clear */
}

void CanHw_RxIrqEnable(boolean enable)
{
    Mmio_Write32(FD_ILE, enable ? 1u : 0u);                /* EINT0 gates interrupt line 0 */
}

boolean CanHw_IsBusOff(void)
{
    return ((Mmio_Read32(FD_PSR) & PSR_BO) != 0u) ? TRUE : FALSE;
}

uint32 CanHw_GetRxLostCount(void)
{
    return hw_rxLost;
}
