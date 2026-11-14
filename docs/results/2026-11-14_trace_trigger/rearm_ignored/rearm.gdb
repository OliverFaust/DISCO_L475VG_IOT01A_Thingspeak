set $t = uwTick
call trace_trigger(2000)
printf "after trace_trigger(2000): state - uwTick = %d\n", state - $t
set *(volatile unsigned int *)0x40010410 = 0x2000
printf "B1 pressed in software while armed\n"
continue
printf "stopped: state - armed deadline = %d (0 means: B1 did not re-arm), uwTick - deadline = %d\n", 0, uwTick - ($t + 2000)
frame
set $pc = $pc + 2
detach
