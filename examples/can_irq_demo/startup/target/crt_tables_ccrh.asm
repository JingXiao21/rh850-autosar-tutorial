; CC-RH 的 CRT 表格式参考。必须根据 map 中真实 section 清单扩展。
; 本教学配置只列 .data/.sdata 和 .bss/.sbss，不承诺覆盖供应商库的其他段。
; 对应 ROM -> RAM 重定位选项：-rom=.data=.data.R -rom=.sdata=.sdata.R
; .noinit 不列入清零表；启动栈也不列入，避免清除 CRT 自己使用的活动栈。
        .section ".INIT_DSEC.const", const
        .align  4
        .dw     #__s.data,  #__e.data,  #__s.data.R
        .dw     #__s.sdata, #__e.sdata, #__s.sdata.R

        .section ".INIT_BSEC.const", const
        .align  4
        .dw     #__s.bss,  #__e.bss
        .dw     #__s.sbss, #__e.sbss
