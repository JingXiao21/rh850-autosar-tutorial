; RH850/P1M-E 教学启动入口，CC-RH 语法参考；未用目标汇编器验证。
; 不是 GHS reset.850 的替代文件。与生成的启动文件二选一，不可重复链接。
; 只面向 PE1 的静态链接 C 教学配置；无 C++/TLS/PIC/硬浮点初始化。
; 必须提供 __Boot_BspBeforeStack 和工程链接布局；不提供空 BSP 默认实现。
;
; RESET 区由 rlink 放到真实复位向量。user mat 默认入口为 0；
; 若使用 bootloader/variable reset vector，必须按真实启动布局调整。
        .section "BOOT_RESET", text
        .align  512
        .public __Boot_ResetVector
__Boot_ResetVector:
        jr32    __Boot_Reset
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x010：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x020：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x030：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x040：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x050：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x060：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x070：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x080：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x090：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0A0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0B0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0C0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0D0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0E0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x0F0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x100：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x110：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x120：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x130：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x140：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x150：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x160：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x170：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x180：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x190：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1A0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1B0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1C0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1D0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1E0：启动期统一停在故障入口
        .align  16
        jr32    __Boot_EarlyTrap    ; +0x1F0：启动期统一停在故障入口
        .align  512                 ; 保留整个 0x200 字节的启动向量区域

        .section ".boot_text", text
        .align  2
__Boot_Reset:
        di                         ; 保持 SV 模式，暂不放开 EI；FE 异常仍可能发生
        ; lock-step：先给每个 GPR 确定值，再把寄存器写入外部存储。
        mov     r0, r1
        mov     r0, r2
        mov     r0, r3
        mov     r0, r4
        mov     r0, r5
        mov     r0, r6
        mov     r0, r7
        mov     r0, r8
        mov     r0, r9
        mov     r0, r10
        mov     r0, r11
        mov     r0, r12
        mov     r0, r13
        mov     r0, r14
        mov     r0, r15
        mov     r0, r16
        mov     r0, r17
        mov     r0, r18
        mov     r0, r19
        mov     r0, r20
        mov     r0, r21
        mov     r0, r22
        mov     r0, r23
        mov     r0, r24
        mov     r0, r25
        mov     r0, r26
        mov     r0, r27
        mov     r0, r28
        mov     r0, r29
        mov     r0, r30
        mov     r0, r31

        ; 这是汇编接口，不是 C 函数！SP/GP/EP 尚未建立。
        ; BSP 必须用无栈代码确认 RAM/ECC、启动异常路径、系统寄存器、
        ; 访问保护和早期 WDG 策略。它不得清掉尚未采集的 RESF。
        ; 保留/恢复 LP，返回 jmp [lp]；嵌套调用也不得借助未准备的栈。
        jarl32  __Boot_BspBeforeStack, lp

        mov     #__Boot_StackTop, sp
        mov     #__gp_data, gp
        mov     #__ep_data, ep
        ; r5/TP 保持 0：本配置不使用依赖 TP 的 PIC。
        ; 真正 ABI、浮点、GP/EP 选项不同，必须使用对应编译器的 startup。

        ; CRT 表由 crt_tables_ccrh.asm 描述；RAM/ECC 必须已准备好。
        mov     #__s.INIT_DSEC.const, r6
        mov     #__e.INIT_DSEC.const, r7
        mov     #__s.INIT_BSEC.const, r8
        mov     #__e.INIT_BSEC.const, r9
        jarl32  __INITSCT_RH, lp

        ; 普通 C 调用，此时才能安全使用全局变量和自动变量。
        jarl32  _Boot_EntryC, lp
        ; Boot_EntryC/EcuM/StartOS 正常不会返回到这里。
__Boot_EarlyTrap:
        syncp
        di
        br      __Boot_EarlyTrap

        ; 仅保留一块示例启动栈；地址必须由真实 RAM 链接布局决定。
        ; 0x800 是教学分配，不代表真实 EcuM/OS 启动栈需求已满足。
        .section ".boot_stack.bss", bss
        .align  8
        .ds     0x800
__Boot_StackTop:
