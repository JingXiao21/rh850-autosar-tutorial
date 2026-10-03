/*
 * Os_Port_Cm33.c  (TARGET BUILD ONLY)
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: the processor-specific part ("port"/HAL) of an AUTOSAR OS for Cortex-M (e.g. the
 * Os_Hal_Context / Os_Hal_Interrupt layers of commercial stacks).  Not standardised; the kernel behaviour it
 * must support is in AUTOSAR_CP_SWS_OS chapters 7.1 (task switch), 7.9.9 (interrupt disabling) and the Cat2
 * ISR rules (SWS_Os_00368/00369).  DESIGN.md section 6.4 is the design of this file.
 * Implemented: PendSV based context switch (r4-r11 + EXC_RETURN saved on the task's PSP stack), per-task PSP /
 * shared MSP, initial stack frames, SysTick tick hook, BASEPRI (OS interrupts) / PRIMASK (all interrupts)
 * locking, NVIC set-up for the configured Cat2 ISRs, one common IRQ dispatcher (Os_Cm33_IrqEntry), stack
 * painting + canary check + high-water mark.
 * NOT implemented: FPU context, MPU, TrustZone, nested-vector tricks (tail chaining is done by the hardware).
 *
 * MEMORY MODEL
 *   MSP  main stack (.stack, 4 KB): Reset, all exceptions/ISRs, PendSV, and the code that runs before the
 *        first task starts.  PSP  process stack: one private stack per task (.os_stack), used in thread mode.
 *
 * TASK SWITCH (all in PendSV, priority 0xF0 = lowest, so it only runs when no other interrupt is active):
 *      hardware on exception entry pushes  r0-r3, r12, lr, pc, xPSR   (8 words, "hardware frame")  onto PSP
 *      PendSV_Handler then pushes          r4-r11, EXC_RETURN(lr)      (9 words, "software frame")  onto PSP
 *      Os_Cm33_SwitchContext(psp)  stores the PSP of the leaving task, asks the kernel for the next task
 *      (Os_Kernel_SelectNext) and returns the saved PSP of that task.
 *      PendSV_Handler pops r4-r11 + EXC_RETURN from the new stack, sets PSP, "bx lr" (EXC_RETURN) makes the
 *      hardware pop the 8-word frame and continue in the new task.
 *   Initial frame of a new task (17 words, lowest address first):
 *      r4..r11 (0)  EXC_RETURN=0xFFFFFFFD  r0..r3 (0)  r12 (0)  lr=Os_Port_TaskReturn  pc=entry  xPSR=0x01000000
 *   EXC_RETURN 0xFFFFFFFD = "return to thread mode, use PSP, no FPU frame, Secure state" (Armv8-M).
 *
 * LOCKING
 *   Os_Port_DisableAll/RestoreAll : PRIMASK        (kernel critical sections, Suspend/DisableAllInterrupts)
 *   Os_Port_SetOsMask             : BASEPRI = 0x40 (SuspendOSInterrupts: Cat2 ISRs, SysTick and PendSV masked)
 */
#include "Os_Port.h"
#include "Os_CfgTypes.h"
#include "Mini_Time.h"
#include "MemMap.h"
#include "Trace.h"

#define REG32(a)            (*(volatile uint32 *)(a))
#define REG8(a)             (*(volatile uint8 *)(a))

#define SCB_ICSR            REG32(0xE000ED04u)          /* bit28 PENDSVSET */
#define SCB_VTOR_ADDR       0xE000ED08u                 /* vector table offset: word 0 = initial MSP */
#define SCB_SHPR_PENDSV     REG8(0xE000ED22u)           /* system handler priority: PendSV   (SHPR3 byte 2) */
#define SCB_SHPR_SYSTICK    REG8(0xE000ED23u)           /* system handler priority: SysTick  (SHPR3 byte 3) */
#define NVIC_ISER(n)        REG32(0xE000E100u + (4u * (n)))   /* set-enable   */
#define NVIC_ICER(n)        REG32(0xE000E180u + (4u * (n)))   /* clear-enable */
#define NVIC_ICPR(n)        REG32(0xE000E280u + (4u * (n)))   /* clear-pending */
#define NVIC_IPR(irq)       REG8(0xE000E400u + (irq))         /* 8-bit priority per IRQ (upper 4 bits implemented) */

#define ICSR_PENDSVSET      (1uL << 28)
#define EXC_RETURN_THREAD_PSP   0xFFFFFFFDu
#define XPSR_THUMB          0x01000000u
#define FRAME_WORDS         17u                         /* 9 software + 8 hardware words */
#define CANARY_WORDS        4u                          /* lowest words of every stack must keep the fill pattern */
#define IDLE_STACK_WORDS    64u                         /* idle only runs WFI; ISRs use MSP */
#define BOOT_STACK_WORDS    32u

#define OS_PORT_IDLE        ((TaskType)OS_NUM_TASKS)

/* ------------------------------------------------------------------ port state */
static uint32 *s_sp[OS_NUM_TASKS + 1u];                 /* saved PSP of every task (index OS_PORT_IDLE = idle) */
static boolean s_fresh[OS_NUM_TASKS + 1u];              /* context was (re)built since it last ran: do not overwrite s_sp */
static uint32 *s_curSp;                                 /* PSP of the task being switched out (for the stack check) */

MINI_VAR_OS_STACK static Os_StackElementType s_idleStack[IDLE_STACK_WORDS];   /* the idle task's own stack */
static uint32 s_bootStack[BOOT_STACK_WORDS] __attribute__((aligned(8)));      /* dummy PSP target for the very first PendSV */

/* ------------------------------------------------------------------ helpers */

static void stack_of(TaskType t, Os_StackElementType **base, uint32 *words)
{
    if (t == OS_PORT_IDLE) {
        *base = s_idleStack;
        *words = IDLE_STACK_WORDS;
    } else {
        *base = Os_Config.tasks[t].stackBase;
        *words = Os_Config.tasks[t].stackWords;
    }
}

/* idle task body (runs on the idle stack at priority 0, preempted by anything READY) */
static void Os_Cm33_IdleEntry(void)
{
    for (;;) {
        Os_Port_Idle();
    }
}

/* The task entry function returned (no TerminateTask): the kernel takes over (E_OS_MISSINGEND).
 * The address of this function is the LR in the initial stack frame. */
static void Os_Port_TaskReturn(void)
{
    Os_Kernel_TaskReturn();
    for (;;) { }
}

/* ------------------------------------------------------------------ Os_Port.h: start-up */

void Os_Port_Init(void)
{
    uint8 i;
    uint32 w;
    Os_StackElementType *base;
    uint32 words;

    /* exception priorities: PendSV lowest (switch only when no ISR is active), SysTick = OS level (BASEPRI masks it) */
    SCB_SHPR_PENDSV = (uint8)OS_CM33_PENDSV_PRIO;
    SCB_SHPR_SYSTICK = (uint8)OS_CM33_MAX_SYSCALL_PRIO;

    /* paint all task stacks so that Os_GetTaskStackUsage() and the canary check work */
    for (i = 0u; i <= Os_Config.numTasks; i++) {
        stack_of(i, &base, &words);
        if (base != NULL_PTR) {
            for (w = 0u; w < words; w++) {
                base[w] = 0xDEADBEEFu;
            }
        }
        s_sp[i] = NULL_PTR;
        s_fresh[i] = FALSE;
    }

    /* Cat2 ISRs: NVIC priority and enable.  Priorities must be >= OS_CM33_MAX_SYSCALL_PRIO so that BASEPRI
     * (SuspendOSInterrupts) masks them. */
    for (i = 0u; i < Os_Config.numIsrs; i++) {
        uint16 irq = Os_Config.isrs[i].irqNumber;
        uint8 prio = Os_Config.isrs[i].nvicPriority;

        if (prio < OS_CM33_MAX_SYSCALL_PRIO) {
            prio = (uint8)OS_CM33_MAX_SYSCALL_PRIO;
        }
        NVIC_IPR(irq) = prio;
        NVIC_ICPR(irq >> 5) = (1uL << (irq & 31u));
        NVIC_ISER(irq >> 5) = (1uL << (irq & 31u));
    }
}

void Os_Port_StartTick(void)
{
    Mini_Time_Init();                                   /* SysTick 1 kHz, TICKINT on; handler calls Mini_Time_TickHook */
}

/* the SysTick handler (Mini_Time_Target.c) calls this strong definition instead of the weak default */
MINI_CODE_FAST void Mini_Time_TickHook(void)
{
    Os_Kernel_TickHandler();
}

/* Build the initial context of task t (see the frame layout in the file header). */
void Os_Port_InitTaskContext(TaskType t)
{
    Os_StackElementType *base;
    uint32 words;
    uint32 *top;
    uint32 *frame;
    uint32 i;
    void (*entry)(void);

    stack_of(t, &base, &words);
    entry = (t == OS_PORT_IDLE) ? Os_Cm33_IdleEntry : Os_Config.tasks[t].entry;

    top = (uint32 *)((uintptr_t)(base + words) & ~(uintptr_t)7u);      /* AAPCS: 8-byte aligned stack top */
    frame = top - FRAME_WORDS;
    for (i = 0u; i < FRAME_WORDS; i++) {
        frame[i] = 0u;                                  /* r4-r11, r0-r3, r12 start as 0 */
    }
    frame[8]  = EXC_RETURN_THREAD_PSP;                  /* popped into LR by PendSV: "return to thread mode on PSP" */
    frame[14] = (uint32)(uintptr_t)Os_Port_TaskReturn;  /* LR  : where the task body returns to */
    frame[15] = (uint32)(uintptr_t)entry & ~1u;         /* PC  : first instruction of the task (Thumb bit lives in xPSR) */
    frame[16] = XPSR_THUMB;                             /* xPSR: T bit must be 1 on Cortex-M */
    s_sp[t] = frame;
    s_fresh[t] = TRUE;
}

/* ------------------------------------------------------------------ context switch */

/* Called from PendSV_Handler on MSP with BASEPRI raised.  `sp` = PSP of the leaving task after the software
 * frame was pushed.  Returns the PSP of the entering task (software frame at its lowest address). */
__attribute__((used)) MINI_CODE_FAST uint32 *Os_Cm33_SwitchContext(uint32 *sp)
{
    TaskType prev;
    TaskType next;

    s_curSp = sp;
    next = Os_Kernel_SelectNext(&prev);
    if ((prev != INVALID_TASK) && (s_fresh[prev] == FALSE)) {
        s_sp[prev] = sp;                                /* leaving task keeps its context (unless it was just re-initialised) */
    }
    if (prev != INVALID_TASK) {
        s_fresh[prev] = FALSE;
    }
    s_fresh[next] = FALSE;
    return s_sp[next];
}

/* PendSV: the context switch.  Written as a naked function so that the compiler adds no prologue/epilogue
 * and the stack/LR are exactly as the hardware left them. */
__attribute__((naked, used)) MINI_CODE_FAST void PendSV_Handler(void)
{
    __asm volatile (
        "mrs   r0, psp                \n"   /* r0 = PSP of the task that was interrupted (hardware frame is on top of it)   */
        "isb                          \n"
        "stmdb r0!, {r4-r11, lr}      \n"   /* push r4-r11 and EXC_RETURN below the hardware frame, r0 = new PSP            */
        "mov   r1, #0x40              \n"   /* OS_CM33_MAX_SYSCALL_PRIO: from now on no OS interrupt may touch kernel data   */
        "msr   basepri, r1            \n"
        "isb                          \n"
        "bl    Os_Cm33_SwitchContext  \n"   /* r0 = old PSP in, new PSP out (C function, may clobber r0-r3, r12, lr)         */
        "mov   r1, #0                 \n"
        "msr   basepri, r1            \n"   /* unmask again */
        "ldmia r0!, {r4-r11, lr}      \n"   /* pop r4-r11 and EXC_RETURN of the entering task, r0 -> its hardware frame      */
        "msr   psp, r0                \n"   /* PSP := hardware frame of the entering task                                    */
        "isb                          \n"
        "bx    lr                     \n"   /* EXC_RETURN: hardware pops r0-r3,r12,lr,pc,xPSR and resumes the task           */
    );
}

/* Ask for a task switch: pend PendSV.  It runs as soon as no other exception is active and neither BASEPRI nor
 * PRIMASK blocks it - i.e. at the end of a Cat2 ISR, or immediately when called from a task with interrupts open. */
void Os_Port_RequestDispatch(void)
{
    SCB_ICSR = ICSR_PENDSVSET;
    __asm volatile ("dsb \n isb" : : : "memory");       /* make sure the exception is taken before we continue */
}

/* Leave StartOS: never returns.  Sets the process stack to a dummy area (the first PendSV has nobody to save),
 * resets MSP to the top of the main stack (everything StartOS used is dead now), pends PendSV and opens
 * interrupts; PendSV then starts the first task.  After msr msp no stack variable may be used - all inputs
 * are therefore passed in registers. */
void Os_Port_StartFirstTask(void)
{
    uint32 bootTop = (uint32)(uintptr_t)&s_bootStack[BOOT_STACK_WORDS];
    uint32 mspTop = *(volatile uint32 *)(uintptr_t)(*(volatile uint32 *)(uintptr_t)SCB_VTOR_ADDR);   /* vector[0] */
    volatile uint32 *icsr = &SCB_ICSR;

    __asm volatile (
        "msr   psp, %0                \n"   /* PSP -> dummy stack (the first PendSV pushes its 9 words there)                */
        "msr   msp, %1                \n"   /* MSP -> top of main stack: discard StartOS's frames                            */
        "isb                          \n"
        "str   %3, [%2]               \n"   /* ICSR.PENDSVSET = 1                                                            */
        "dsb                          \n"
        "cpsie i                      \n"   /* PRIMASK = 0: PendSV is taken here and starts the first task                   */
        "isb                          \n"
        "1: b 1b                      \n"   /* never reached (PendSV does not come back to this thread context)              */
        : /* no outputs */
        : "r" (bootTop), "r" (mspTop), "r" (icsr), "r" (ICSR_PENDSVSET)
        : "memory");
    for (;;) { }
}

void Os_Port_Idle(void)
{
    __asm volatile ("wfi");                             /* sleep until the next interrupt (SysTick at the latest) */
}

void Os_Port_Halt(void)
{
    __asm volatile ("cpsid i" : : : "memory");          /* SWS_Os_00425: all interrupts off ... */
    for (;;) {
        __asm volatile ("wfi");                         /* ... and an endless loop */
    }
}

/* ------------------------------------------------------------------ interrupt locking */

uint32 Os_Port_DisableAll(void)
{
    uint32 primask;

    __asm volatile ("mrs %0, primask \n cpsid i" : "=r" (primask) : : "memory");
    return primask;                                     /* 1 if interrupts were already off */
}

void Os_Port_RestoreAll(uint32 prev)
{
    __asm volatile ("msr primask, %0" : : "r" (prev) : "memory");
}

void Os_Port_SetOsMask(boolean masked)
{
    uint32 v = (masked != FALSE) ? OS_CM33_MAX_SYSCALL_PRIO : 0u;

    __asm volatile ("msr basepri, %0 \n isb" : : "r" (v) : "memory");
}

boolean Os_Port_InIsr(void)
{
    uint32 ipsr;

    __asm volatile ("mrs %0, ipsr" : "=r" (ipsr));      /* exception number; 0 = thread mode */
    return (boolean)(ipsr != 0u);
}

/* the NVIC delivers interrupts by itself: nothing to poll */
void Os_Port_InterruptPoint(void)
{
}

/* ------------------------------------------------------------------ Category 2 ISR dispatcher */

/* ALL external interrupt vectors point here (startup.c).  IPSR = 16 + IRQ number tells which one fired; the
 * handler comes from the generated Os_Config.isrs[] table.  Equivalent to the "ISR wrapper" of a real OS. */
MINI_CODE_FAST void Os_Cm33_IrqEntry(void)
{
    uint32 ipsr;
    uint32 irq;
    uint8 i;

    __asm volatile ("mrs %0, ipsr" : "=r" (ipsr));
    irq = ipsr - 16u;
    for (i = 0u; i < Os_Config.numIsrs; i++) {
        if (Os_Config.isrs[i].irqNumber == irq) {
            Os_Kernel_IsrEnter((ISRType)i);
            Os_Config.isrs[i].handler();
            Os_Kernel_IsrExit();                        /* may pend PendSV: the switch happens after the last ISR ends */
            return;
        }
    }
    /* No handler configured for this interrupt: switch the line off, otherwise it would fire forever. */
    NVIC_ICER(irq >> 5) = (1uL << (irq & 31u));
    TRACE(TRACE_CAT_OS, "ERROR spurious irq=%u", (unsigned)irq);
}

/* ------------------------------------------------------------------ stack supervision */

/* canary: the lowest words of the stack must still hold the paint pattern, and the saved SP must be above them */
boolean Os_Port_StackCheck(TaskType t)
{
    Os_StackElementType *base;
    uint32 words;
    uint32 i;

    stack_of(t, &base, &words);
    if ((base == NULL_PTR) || (words <= CANARY_WORDS)) {
        return TRUE;
    }
    for (i = 0u; i < CANARY_WORDS; i++) {
        if (base[i] != 0xDEADBEEFu) {
            return FALSE;
        }
    }
    if ((s_curSp != NULL_PTR) && (s_curSp < (uint32 *)&base[CANARY_WORDS])) {
        return FALSE;
    }
    return TRUE;
}

/* high-water mark: first word from the bottom that no longer holds the pattern */
uint32 Os_Port_StackUsage(TaskType t)
{
    Os_StackElementType *base;
    uint32 words;
    uint32 i = 0u;

    stack_of(t, &base, &words);
    if (base == NULL_PTR) {
        return 0u;
    }
    while ((i < words) && (base[i] == 0xDEADBEEFu)) {
        i++;
    }
    return (words - i) * 4u;
}
