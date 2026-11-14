printf "APB1FZR1 = 0x%08x (bit 4 DBG_TIM6_STOP)\n", *(unsigned int *)0xE0042008
define halt3
  set $u0 = uwTick
  set $x0 = xTickCount
  set $c0 = *(unsigned int *)0xE0001004
  set $t0 = *(unsigned int *)0x40001024
  shell sleep 3
  printf "halted 3 s: uwTick +%u, xTickCount +%u, DWT_CYCCNT +%u, TIM6_CNT %u -> %u\n", uwTick - $u0, xTickCount - $x0, *(unsigned int *)0xE0001004 - $c0, $t0, *(unsigned int *)0x40001024
  stepi
  printf "after one instruction: uwTick +%u\n", uwTick - $u0
end
printf "== TIM6 frozen (Debug-Trace as built)\n"
halt3
printf "== TIM6 not frozen (bit cleared by GDB for this test)\n"
set *(unsigned int *)0xE0042008 = *(unsigned int *)0xE0042008 & ~0x10
halt3
set *(unsigned int *)0xE0042008 = *(unsigned int *)0xE0042008 | 0x10
printf "APB1FZR1 restored = 0x%08x\n", *(unsigned int *)0xE0042008
detach
