# [Educational Implementation] demo_session.gdb - scripted GDB session used by tools/gdb/run_gdb_session.py (batch mode).
# Walks one LightEcu through: Reset_Handler -> main -> EcuM_Init -> StartOS -> OS dispatcher -> TASK(Task_LightCtl)
# -> runnable LightCtl_Run20ms -> Rte_Write_* -> Com_SendSignal -> Can_Write.  Needs mini_autosar.gdb loaded before.
echo \n===== 1. attach: the CPU is held at the reset vector =====\n
mini_connect
mini_vectors
echo \n===== 2. Reset_Handler (halted here since attach) -> main =====\n
mini_break_boot
continue
bt
echo \n===== 3. main -> EcuM_Init =====\n
continue
bt
echo \n===== 4. EcuM_Init -> StartOS (OS start, task scheduling begins) =====\n
continue
bt
delete
echo \n===== 5. OS dispatcher: Os_Kernel_SelectNext picks the first task =====\n
break Os_Kernel_SelectNext
continue
bt 3
mini_state
delete
echo \n===== 6. a task body: TASK(Task_LightCtl) (extended task, autostarted) =====\n
tbreak Os_Task_Task_LightCtl
continue
mini_current
mini_tasks
mini_stacks
echo \n===== 7. runnable LightCtl_Run20ms (20 ms alarm -> SetEvent -> task wakes) =====\n
tbreak LightCtl_Run20ms
continue
bt 4
mini_ready
echo \n===== 8. RTE -> Com -> CAN: Rte_Write_P_HeadlightStatus_HeadlightStatus, Com_SendSignal, Can_Write =====\n
tbreak Rte_Write_P_HeadlightStatus_HeadlightStatus
tbreak Com_SendSignal
tbreak Can_Write
continue
bt 6
continue
bt 6
continue
bt 6
mini_state
echo \n===== end of demo: detach (Renode keeps running) =====\n
detach
quit
