# docs/results: what was run, and what was seen

One record per test, named `<date>_<topic>.md`, with captures (`_console.txt`, `_feed.txt`) and data
folders next to it. Console captures are masked (`tools/mask_log.py`): the API key shows its first
4 characters only, MAC addresses and BSSIDs lose their last 3 bytes, SSIDs keep their first 3
characters.

**Toolchain.** Everything here was run on Ubuntu 25.04 with STM32CubeIDE 2.1.0 (GNU Tools for STM32
14.3.rel1), unless a record says otherwise. Flash-image hashes (the first 16 hex digits of the SHA-256
of `arm-none-eabi-objcopy -O binary`) are for that toolchain. An image is byte-identical only when it
is built with the same compiler version, so another STM32CubeIDE version (e.g. 2.2.0) may give other
hashes from the same source.

**Secrets.** No record holds a password or a full key; `tools/check_secrets.sh` checks every file
in this folder before each commit.
