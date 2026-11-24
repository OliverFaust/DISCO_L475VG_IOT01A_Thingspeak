#!/usr/bin/env bash
# check_history.sh -- make sure no secret is anywhere in the git history that you are about to push.
#
#   tools/check_history.sh [<commit or ref> ...]     (run from the repository root)
#
# Without arguments it checks every commit of every local branch, tag and remote-tracking branch
# (git rev-list --all); with arguments, every commit reachable from them (the pre-push hook passes
# the commits being pushed). tools/check_secrets.sh checks the files of your next commit; this
# script checks what is already committed: a secret that was committed once and removed later is
# still in the history, and a push publishes all of it.
#
# Reads the same values as check_secrets.sh: every APP_WIFI_SSID, APP_WIFI_PASSWORD and
# APP_THINGSPEAK_WRITE_KEY in Core/Inc/app_secrets.h (also commented-out ones), and
# THINGSPEAK_CHANNEL_ID and THINGSPEAK_READ_KEY from tools/thingspeak_host.env if present. Values
# still equal to the template's placeholders, and values shorter than 4 characters, are skipped.
# Searches each value in the files of every commit and in every commit message. Also looks for
# unmasked MAC addresses (xx:xx:xx:xx:xx:xx, see tools/mask_log.py) in docs/results/ of every commit.
# Prints only counts and which secret was found, never a value.
# Exit status: 0 = clean, 1 = something found, 2 = setup problem (nothing checked).
#
# The SSID and the channel ID are reported as warnings only (not secret in themselves, but they
# identify a place and a channel); the MAC check is a warning too. Passwords and keys are errors.
#
# Tested on Ubuntu (bash 5, git 2.48). On Windows: Git Bash (not tested).
set -uo pipefail
# Anything unexpected that stops the script is reported (script, line, command), never silent.
trap 'rc=$?; echo "${0##*/}: unexpected error (exit $rc) at line $LINENO: $BASH_COMMAND" >&2' ERR

SECRETS=Core/Inc/app_secrets.h
TEMPLATE=Core/Inc/app_secrets.h.template
HOSTENV=tools/thingspeak_host.env

git rev-parse --git-dir >/dev/null 2>&1 || { echo "check_history: not in a git repository"; exit 2; }
cd "$(git rev-parse --show-toplevel)" || exit 2
[ -f "$SECRETS" ] || [ -f "$HOSTENV" ] || {
  echo "check_history: neither $SECRETS nor $HOSTENV found (nothing to check against)"; exit 2; }

defines() {   # file, name: every value of a #define NAME "..." -- also commented-out ones
  [ -f "$1" ] && sed -n "s/^[[:space:]]*\(\/\/[[:space:]]*\)\{0,1\}#define[[:space:]]\+$2[[:space:]]\+\"\(.*\)\".*/\2/p" "$1"
}
envvalue() {  # name: its value in thingspeak_host.env, without quotes and blanks
  [ -f "$HOSTENV" ] && sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\(.*\)$/\1/p" "$HOSTENV" | tr -d '\r"'"'"' \t' | head -1
}

if [ $# -gt 0 ]; then
  revs=$(git rev-list "$@") || { echo "check_history: cannot list the commits of: $*"; exit 2; }
else
  revs=$(git rev-list --all)
fi
ncommits=$(printf '%s\n' "$revs" | grep -c . || true)
[ "$ncommits" -gt 0 ] || { echo "check_history: no commits to check"; exit 0; }
msgs=$(printf '%s\n' "$revs" | git log --no-walk=unsorted --stdin --format=%B)

status=0 checked=0
count() {     # path, git grep options...: number of matching lines in all commits; exit 2 on a git error
  local path=$1 out rc; shift
  # shellcheck disable=SC2086  # $revs: one commit per word
  out=$(git grep -c "$@" $revs -- "$path" 2>&1); rc=$?
  if [ $rc -gt 1 ]; then echo "check_history: git grep failed (exit $rc): ${out%%$'\n'*}" >&2; exit 2; fi
  # one line "<commit>:<file>:<count>" per file that matches
  awk -F: '{n += $NF} END {print n + 0}' <<<"$out"
}
check() {     # label, value, severity[, extra git grep option, e.g. -w]
  local label=$1 val=$2 sev=$3 opt=${4:-} tree msg
  [ ${#val} -ge 4 ] || { echo "check_history: $label too short or empty, not checked"; return; }
  if grep -qxF -e "$val" < <(defines "$TEMPLATE" APP_WIFI_SSID; defines "$TEMPLATE" APP_WIFI_PASSWORD;
                              defines "$TEMPLATE" APP_THINGSPEAK_WRITE_KEY); then
    echo "check_history: $label is still the template's placeholder, not checked"; return
  fi
  tree=$(count . -F $opt -e "$val") || exit 2
  msg=$(grep -cF $opt -e "$val" <<<"$msgs" || true)
  checked=$((checked + 1))
  if [ "$tree" -gt 0 ] || [ "$msg" -gt 0 ]; then
    echo "check_history: $sev: $label found: $tree line(s) in files (summed over all commits), $msg commit message line(s)"
    if [ "$sev" = ERROR ]; then status=1; fi
  fi
}

n=0; while IFS= read -r v; do n=$((n + 1)); check "APP_WIFI_PASSWORD #$n"        "$v" ERROR;   done < <(defines "$SECRETS" APP_WIFI_PASSWORD)
n=0; while IFS= read -r v; do n=$((n + 1)); check "APP_THINGSPEAK_WRITE_KEY #$n" "$v" ERROR;   done < <(defines "$SECRETS" APP_THINGSPEAK_WRITE_KEY)
n=0; while IFS= read -r v; do n=$((n + 1)); check "APP_WIFI_SSID #$n"            "$v" WARNING; done < <(defines "$SECRETS" APP_WIFI_SSID)
if [ -f "$HOSTENV" ]; then
  check THINGSPEAK_READ_KEY   "$(envvalue THINGSPEAK_READ_KEY)"   ERROR
  # The channel ID is a number: whole words only, so that it is not found inside longer numbers.
  check THINGSPEAK_CHANNEL_ID "$(envvalue THINGSPEAK_CHANNEL_ID)" WARNING -w
fi

macs=$(count docs/results -E -e '\b([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\b') || exit 2
if [ "$macs" -gt 0 ]; then
  echo "check_history: WARNING: $macs line(s) with an unmasked MAC address in docs/results/ (summed over all commits; tools/mask_log.py)"
fi

if [ $status -eq 0 ]; then
  echo "check_history: clean ($checked value(s) checked in $ncommits commit(s): no password or key in files or messages)"
fi
exit $status
