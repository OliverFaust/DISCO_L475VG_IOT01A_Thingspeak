/* csp_trace.ld -- RAM trace ring of CSP4CMSIS (csp_trace_ring.h; defined in
 * Core/Src/csp_trace_board.c), Debug-Trace configuration only: a second linker script next to
 * STM32L475VGTX_FLASH.ld, passed only in Debug-Trace (G++ linker, other flags: -Wl,-T,../csp_trace.ld).
 * INSERT adds .csp_trace to the main script. It goes to SRAM2 (32 KB at 0x10000000, region RAM2
 * of the main script), which nothing else uses. The address is given directly rather than as
 * ">RAM2": ld reads this fragment before the main script, where RAM2 is declared.
 * NOLOAD: the debugger's load does not write it; csp_trace_ring_init() initialises it.
 * Adapted from the STM32N6570-DK port's CSP/csp_trace.ld. */
SECTIONS
{
  .csp_trace 0x10000000 (NOLOAD) :
  {
    . = ALIGN(32);
    KEEP(*(.csp_trace))
    . = ALIGN(32);
  }
  ASSERT(ADDR(.csp_trace) + SIZEOF(.csp_trace) <= 0x10008000, "Error: .csp_trace does not fit into SRAM2 (32 KB): reduce CSP_TRACE_RECORDS")
}
INSERT AFTER .noinit;
