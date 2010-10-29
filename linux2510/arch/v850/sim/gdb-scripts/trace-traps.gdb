set $linux_banner = *(char **)_linux_banner
set $vers_2_5 = ($linux_banner[16] == '5')

set $r0_ram = (unsigned long *)((*((long *)&trap) >> 16) - 4)

break *trap
command
  silent
  set $km = ($r0_ram[1] & 0x1)
  if ($km)
    # kernel mode
    set $task = (struct task_struct *)$r16
  else
    # user mode
    set $kernel_sp = $r0_ram[0]
    if ($vers_2_5)
      set $thread_info = (struct thread_info *)(($kernel_sp - 1) & -8192)
      set $task = $thread_info->task
    else
      set $task = (struct task_struct *)(($kernel_sp - 1) & -8192)
    end
  end
  printf "[pid %d%s: trap %d, %d]\n", $task->pid, ($km ? " <km>" : ""), ($ecr - 0x40), $r12
  cont
end

break *ret_from_trap
command
  silent
  set $task = (struct task_struct *)$r16
  set $regs = ((struct pt_regs *)($sp + 24))
  printf "[pid %d%s: ret_from_trap %d, 0x%x]\n", $task->pid, ($regs->kernel_mode ? " <km>" : ""), $regs->gpr[0], $r10
  cont
end
