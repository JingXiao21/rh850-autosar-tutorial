# [Educational Implementation] mini_autosar.gdb - GDB helper commands for the mini AUTOSAR ECU images under Renode.
#
# This GDB build has no Python, so everything is written in the GDB command language.
#
# Use (interactive):
#   1. renode --disable-gui --console -e "$elf=@<abs path LightEcu.elf>; $uart=@<abs path uart.log>; include @<abs path debug_ecu.resc>"
#   2. arm-none-eabi-gdb -nx -x tools/gdb/mini_autosar.gdb <LightEcu.elf>
#   3. (gdb) mini_connect          # attach; the CPU is halted at Reset_Handler
#      (gdb) mini_break_boot       # + mini_break_os / mini_break_tasks / mini_break_swc / mini_break_comstack / mini_break_all
#      (gdb) continue ... (gdb) mini_state
# Port: set $mini_port = 3334 before mini_connect for another Renode GDB server.
#
# Commands defined here (help <command> prints the text):
#   mini_connect       attach to Renode's GDB server and show where the CPU stands
#   mini_break_boot    Reset_Handler, main, EcuM_Init, StartOS
#   mini_break_os      OS dispatcher: Os_Kernel_SelectNext (picks the next task), Os_Cm33_SwitchContext (PendSV)
#   mini_break_tasks   every generated task body (Os_Task_Task_*, i.e. TASK(Task_...) of Rte_Tasks.c)
#   mini_break_swc     LightCtl_Run20ms / LightCtl_OnSpeed (runnables of LightEcu; ignored on SensorEcu)
#   mini_break_comstack  all Rte_Write_*, Com_SendSignal, Can_Write
#   mini_break_all     everything above
#   mini_tasks         OS task table: id, name, state, base/dynamic priority, activations, events
#   mini_ready         ready queue: bitmap and the FIFO of every non-empty priority level
#   mini_current       running task, ISR depth, PSP/MSP, current backtrace
#   mini_stacks        per-task stack usage from the 0xDEADBEEF paint (like Os_GetTaskStackUsage)
#   mini_vectors       VTOR, initial MSP, Reset vector, SysTick/PendSV/IRQ entries
#   mini_state         mini_current + mini_tasks + mini_ready

set pagination off
set confirm off
set height 0
set width 0
set breakpoint pending on
set print pretty off
set disassemble-next-line off
set $mini_port = 3333

define mini_connect
  eval "target remote localhost:%d", $mini_port
  printf "connected: pc=0x%08x sp=0x%08x\n", $pc, $sp
  info symbol $pc
end
document mini_connect
Attach to Renode's GDB server (localhost:$mini_port, default 3333). The CPU is halted at the reset vector = Reset_Handler.
end

define mini_break_boot
  break Reset_Handler
  break main
  break EcuM_Init
  break StartOS
end
document mini_break_boot
Breakpoints on the start-up path: Reset_Handler (startup.c), main, EcuM_Init, StartOS (OS start; does not return).
end

define mini_break_os
  break Os_Kernel_SelectNext
  break Os_Cm33_SwitchContext
end
document mini_break_os
OS dispatcher: Os_Kernel_SelectNext() chooses the next task (called from the PendSV context switch Os_Cm33_SwitchContext).
end

define mini_break_tasks
  rbreak ^Os_Task_Task_
end
document mini_break_tasks
Breakpoint on every task body generated into Rte_Tasks.c (TASK(Task_Init), TASK(Task_LightCtl), ...; C symbol Os_Task_<name>).
end

define mini_break_swc
  break LightCtl_Run20ms
  break LightCtl_OnSpeed
end
document mini_break_swc
Runnables of LightControlSWC (LightEcu only). Pending breakpoints stay unresolved on SensorEcu.
end

define mini_break_comstack
  rbreak ^Rte_Write_
  break Com_SendSignal
  break Can_Write
end
document mini_break_comstack
RTE write ports, Com_SendSignal (signal -> I-PDU) and Can_Write (MCAL: I-PDU -> FDCAN TX FIFO).
end

define mini_break_all
  mini_break_boot
  mini_break_os
  mini_break_tasks
  mini_break_swc
  mini_break_comstack
end
document mini_break_all
All breakpoints of mini_break_boot/os/tasks/swc/comstack.
end

# ---------------------------------------------------------------------------------------------------- OS pretty printers
define mini_tasks
  set $n = (int)Os_Config.numTasks
  printf "%-3s %-16s %-10s %4s %4s %4s %4s %10s %10s\n", "id", "task", "state", "base", "cur", "act", "res", "eventsSet", "eventsWait"
  set $i = 0
  while $i <= $n
    set $st = Os_Tcb[$i].state
    if $i < $n
      printf "%-3d %-16s ", $i, Os_Config.tasks[$i].name
      set $base = (int)Os_Config.tasks[$i].priority
    else
      printf "%-3d %-16s ", $i, "(idle)"
      set $base = 0
    end
    if $st == 0
      printf "%-10s ", "RUNNING"
    end
    if $st == 1
      printf "%-10s ", "WAITING"
    end
    if $st == 2
      printf "%-10s ", "READY"
    end
    if $st == 3
      printf "%-10s ", "SUSPENDED"
    end
    printf "%4d %4d %4d %4d 0x%08x 0x%08x\n", $base, Os_Tcb[$i].curPriority, Os_Tcb[$i].activations, Os_Tcb[$i].resCount, Os_Tcb[$i].eventsSet, Os_Tcb[$i].eventsWaited
    set $i = $i + 1
  end
end
document mini_tasks
Print the OS task table: Os_Config.tasks[] (name, base priority) joined with the runtime TCBs Os_Tcb[] (state, dynamic priority, ...).
end

define mini_ready
  printf "ready bitmap s_readyMask = 0x%08x (bit p set = queue of priority p not empty)\n", s_readyMask
  set $p = 31
  while $p >= 0
    if s_qCount[$p] > 0
      printf "  prio %2d (%d queued):", $p, s_qCount[$p]
      set $k = 0
      while $k < s_qCount[$p]
        set $t = s_q[$p][(s_qHead[$p] + $k) & 15]
        if $t < Os_Config.numTasks
          printf " %s", Os_Config.tasks[$t].name
        else
          printf " task#%d", $t
        end
        set $k = $k + 1
      end
      printf "\n"
    end
    set $p = $p - 1
  end
  if s_readyMask == 0
    printf "  (empty: only the idle task can run)\n"
  end
end
document mini_ready
Print the ready queue of the kernel (Os_Core.c): bitmap plus, per priority level, the FIFO order of READY tasks.
end

define mini_current
  if Os_Running == 255
    printf "running task: none yet (INVALID_TASK, before the first dispatch)\n"
  else
    if Os_Running < Os_Config.numTasks
      printf "running task: %d = %s (priority %d)\n", Os_Running, Os_Config.tasks[Os_Running].name, Os_Config.tasks[Os_Running].priority
    else
      printf "running task: %d = (idle)\n", Os_Running
    end
  end
  printf "Os_Started=%d  Os_IsrDepth=%d  IPSR=0x%02x\n", Os_Started, Os_IsrDepth, ($xpsr & 0x1ff)
  info registers pc sp lr
  backtrace 6
end
document mini_current
Which task owns the CPU, ISR nesting, core registers and a short backtrace. A task's outermost frame shows
"Os_Cm33_IdleEntry": that is the fake return address (LR = Os_Port_TaskReturn, built by Os_Port_InitTaskContext) which
lies directly behind Os_Cm33_IdleEntry, and GDB looks up return addresses at pc-1. The backtrace then stops there.
end

define mini_stacks
  set $n = (int)Os_Config.numTasks
  printf "%-16s %10s %6s %6s %6s\n", "task", "base", "words", "used", "free"
  set $i = 0
  while $i < $n
    set $b = (unsigned int *)Os_Config.tasks[$i].stackBase
    set $w = (int)Os_Config.tasks[$i].stackWords
    set $f = 0
    while $f < $w && $b[$f] == 0xDEADBEEF
      set $f = $f + 1
    end
    printf "%-16s 0x%08x %6d %6d %6d\n", Os_Config.tasks[$i].name, $b, $w, $w - $f, $f
    set $i = $i + 1
  end
end
document mini_stacks
Stack watermark per task: words still holding the 0xDEADBEEF paint are unused (same method as Os_GetTaskStackUsage).
end

define mini_vectors
  printf "VTOR = 0x%08x\n", *(unsigned int *)0xE000ED08
  printf "vector[0] initial MSP  = 0x%08x   (_estack = 0x%08x)\n", g_vectors[0], &_estack
  printf "vector[1] Reset        = 0x%08x   (Reset_Handler = 0x%08x, bit0 = Thumb)\n", g_vectors[1], Reset_Handler
  printf "vector[14] PendSV      = 0x%08x\n", g_vectors[14]
  printf "vector[15] SysTick     = 0x%08x\n", g_vectors[15]
  printf "vector[16+39] FDCAN1_IT0 = 0x%08x   (all external IRQs -> Os_Cm33_IrqEntry = 0x%08x)\n", g_vectors[55], Os_Cm33_IrqEntry
end
document mini_vectors
Show the vector table words that matter and where VTOR points.
end

define mini_state
  mini_current
  mini_tasks
  mini_ready
end
document mini_state
mini_current + mini_tasks + mini_ready.
end
