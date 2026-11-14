# toolchain.sh -- sourced by the scripts in tools/: finds STM32CubeIDE, STM32CubeProgrammer and the
# GNU tools (arm-none-eabi-nm, -gdb) without requiring them on the PATH.
#
# Overrides (environment): CUBEIDE (STM32CubeIDE folder), STM32_PROGRAMMER_CLI (the CLI itself).
# Defaults: the first STM32CubeIDE 2.1.0 folder found in ~/st or /opt/st, and STM32CubeProgrammer in
# ~/st/STM32CubeProgrammer or the copy inside STM32CubeIDE. Tested on Ubuntu.

if [ -z "${CUBEIDE:-}" ]; then
  for d in "$HOME/st/stm32cubeide_2.1.0" /opt/st/stm32cubeide_2.1.0; do
    [ -d "$d/plugins" ] && { CUBEIDE=$d; break; }
  done
fi
CUBEIDE=${CUBEIDE:-$HOME/st/stm32cubeide_2.1.0}

# GNU tools of STM32CubeIDE, unless arm-none-eabi-nm is already on the PATH.
if ! command -v arm-none-eabi-nm >/dev/null 2>&1; then
  _tc=$(ls -d "$CUBEIDE"/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin 2>/dev/null | head -1 || true)
  [ -n "$_tc" ] && PATH="$_tc:$PATH"
  unset _tc
fi

if [ -z "${STM32_PROGRAMMER_CLI:-}" ]; then
  STM32_PROGRAMMER_CLI=$(command -v STM32_Programmer_CLI || true)
  for c in "$HOME/st/STM32CubeProgrammer/bin/STM32_Programmer_CLI" \
           "$CUBEIDE"/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.*/tools/bin/STM32_Programmer_CLI; do
    [ -n "$STM32_PROGRAMMER_CLI" ] && break
    [ -x "$c" ] && STM32_PROGRAMMER_CLI=$c
  done
fi
export PATH CUBEIDE STM32_PROGRAMMER_CLI
