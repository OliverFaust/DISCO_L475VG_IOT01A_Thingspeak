#!/usr/bin/env bash
# toolchain.sh -- sourced by the scripts in tools/: finds STM32CubeIDE, STM32CubeProgrammer and the
# GNU tools (arm-none-eabi-nm, -gdb) without requiring them on the PATH.
#
#   tools/toolchain.sh        run directly: prints what it found (a quick check of the setup)
#
# Overrides (environment): CUBEIDE (STM32CubeIDE folder), STM32_PROGRAMMER_CLI (the CLI itself).
# Defaults: the newest stm32cubeide_* folder (any version) in ~/st or /opt/st that has a plugins/
# folder, and STM32CubeProgrammer in ~/st/STM32CubeProgrammer or the copy inside STM32CubeIDE.
# Tested on Ubuntu with STM32CubeIDE 2.1.0 (not with 2.2.0).
#
# Never exits the calling script: what is missing is reported by the caller through
# toolchain_missing (below), which names the folders searched and how to set CUBEIDE.

TOOLCHAIN_SEARCHED="$HOME/st/stm32cubeide_* /opt/st/stm32cubeide_*"
if [ -n "${CUBEIDE:-}" ]; then
  TOOLCHAIN_FROM="CUBEIDE=$CUBEIDE (set in the environment)"
else
  CUBEIDE=
  # Newest version first (sort -V on the version after "stm32cubeide_"); the first with plugins/ wins.
  while IFS= read -r _d; do
    [ -d "$_d/plugins" ] && { CUBEIDE=$_d; break; }
  done < <(for _d in "$HOME"/st/stm32cubeide_* /opt/st/stm32cubeide_*; do
             [ -d "$_d" ] && printf '%s\t%s\n' "${_d##*/stm32cubeide_}" "$_d"
           done | sort -t "$(printf '\t')" -k1,1 -rV | cut -f2- || true)
  unset _d
  if [ -n "$CUBEIDE" ]; then
    TOOLCHAIN_FROM="$CUBEIDE (newest of $TOOLCHAIN_SEARCHED)"
  else
    TOOLCHAIN_FROM="no STM32CubeIDE found in $TOOLCHAIN_SEARCHED"
  fi
fi

# cubeide_tool <plugin name part> <file>: path of <file> in an STM32CubeIDE plugin, or empty.
cubeide_tool() {
  [ -n "$CUBEIDE" ] || return 0
  ls -d "$CUBEIDE"/plugins/com.st.stm32cube.ide.mcu.externaltools."$1".*/tools/bin/"$2" 2>/dev/null | sort -rV | head -1 || true
}

# toolchain_missing <what>: explain what was searched and how to fix it (stderr). The caller exits.
toolchain_missing() {
  {
    echo "${0##*/}: $1 not found."
    echo "  STM32CubeIDE: $TOOLCHAIN_FROM"
    if [ -n "$CUBEIDE" ] && [ ! -d "$CUBEIDE/plugins" ]; then
      echo "  $CUBEIDE has no plugins/ folder: not an STM32CubeIDE installation folder."
    fi
    echo "  Also searched: the PATH (arm-none-eabi-*, STM32_Programmer_CLI)."
    echo "  Fix: install STM32CubeIDE (docs/01-prerequisites.md), or name its folder, e.g."
    echo "    CUBEIDE=\$HOME/st/stm32cubeide_2.1.0 ${0##*/} ..."
  } >&2
}

# need_cmd <command>: 0 if on the PATH (after the additions below); otherwise toolchain_missing, 1.
need_cmd() {
  command -v "$1" >/dev/null 2>&1 && return 0
  toolchain_missing "$1"
  return 1
}

# GNU tools of STM32CubeIDE, unless arm-none-eabi-nm is already on the PATH.
if ! command -v arm-none-eabi-nm >/dev/null 2>&1; then
  _tc=$(cubeide_tool gnu-tools-for-stm32 arm-none-eabi-nm)
  [ -z "$_tc" ] || PATH="$(dirname "$_tc"):$PATH"
  unset _tc
fi

if [ -z "${STM32_PROGRAMMER_CLI:-}" ]; then
  STM32_PROGRAMMER_CLI=$(command -v STM32_Programmer_CLI || true)
  [ -n "$STM32_PROGRAMMER_CLI" ] || [ ! -x "$HOME/st/STM32CubeProgrammer/bin/STM32_Programmer_CLI" ] \
    || STM32_PROGRAMMER_CLI=$HOME/st/STM32CubeProgrammer/bin/STM32_Programmer_CLI
  [ -n "$STM32_PROGRAMMER_CLI" ] || STM32_PROGRAMMER_CLI=$(cubeide_tool cubeprogrammer STM32_Programmer_CLI)
fi
export PATH CUBEIDE STM32_PROGRAMMER_CLI

if [ "${BASH_SOURCE[0]}" = "$0" ]; then   # run, not sourced: report
  show() { printf '  %-22s %s\n' "$1" "${2:-NOT FOUND}"; }
  echo "toolchain: STM32CubeIDE: $TOOLCHAIN_FROM"
  show arm-none-eabi-nm "$(command -v arm-none-eabi-nm || true)"
  show arm-none-eabi-gdb "$(cubeide_tool gnu-tools-for-stm32 arm-none-eabi-gdb)"
  show ST-LINK_gdbserver "$(cubeide_tool stlink-gdb-server ST-LINK_gdbserver)"
  show STM32_Programmer_CLI "$STM32_PROGRAMMER_CLI"
fi
