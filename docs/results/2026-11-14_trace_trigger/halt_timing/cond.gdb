set $n = 0
set $k0 = xTickCount
break tasks.c:3432 if ($n = $n + 1) < 0
continue
printf "passes %d, kernel ticks advanced %u\n", $n, xTickCount - $k0
delete
detach
