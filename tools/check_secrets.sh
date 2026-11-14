#!/usr/bin/env bash
# check_secrets.sh -- make sure no secret from Core/Inc/app_secrets.h is about to be committed.
#
#   tools/check_secrets.sh          (run from the repository root, before every commit)
#
# Reads every APP_WIFI_SSID, APP_WIFI_PASSWORD and APP_THINGSPEAK_WRITE_KEY in Core/Inc/app_secrets.h
# (also commented-out ones),
# and THINGSPEAK_READ_KEY from tools/thingspeak_host.env if present, and searches for each value
# in: the files staged for commit (the index), all tracked files, and everything under
# docs/results/ (tracked or not). Prints only file names and which secret was
# found, never the value. Also checks that both files are ignored and not tracked.
# Exit status: 0 = clean, 1 = a secret was found or app_secrets.h is tracked, 2 = setup problem.
#
# The SSID is reported as a warning only (it is not secret in itself, but a home network name
# identifies a place; console logs mask it, see tools/mask_log.py).
set -uo pipefail

SECRETS=Core/Inc/app_secrets.h
[ -f "$SECRETS" ] || { echo "check_secrets: $SECRETS not found (nothing to check against)"; exit 2; }

values() {  # every value of a #define "..." in app_secrets.h, one per line -- also commented-out
            # ones (e.g. a second network kept in a comment): all of them must stay out of git
  sed -n "s/^[[:space:]]*#define[[:space:]]\+$1[[:space:]]\+\"\(.*\)\".*/\1/p" "$SECRETS"
}

status=0
if git ls-files --error-unmatch "$SECRETS" >/dev/null 2>&1; then
  echo "check_secrets: ERROR: $SECRETS is tracked by git"; status=1
fi
git check-ignore -q "$SECRETS" || { echo "check_secrets: ERROR: $SECRETS is not ignored"; status=1; }

check() {   # name, value, severity
  local name=$1 val=$2 sev=$3 hits
  if [ ${#val} -lt 4 ]; then echo "check_secrets: $name too short or missing, not checked"; return; fi
  hits=$( { git grep -l -F --cached -e "$val" 2>/dev/null; git grep -l -F -e "$val" 2>/dev/null;
            grep -rlF -e "$val" docs/results 2>/dev/null; } | sort -u | grep -vx "$SECRETS")
  if [ -n "$hits" ]; then
    echo "check_secrets: $sev: $name found in:"; echo "$hits" | sed 's/^/  /'
    [ "$sev" = ERROR ] && status=1
  fi
}

while IFS= read -r v; do check APP_WIFI_PASSWORD        "$v" ERROR;   done < <(values APP_WIFI_PASSWORD)
while IFS= read -r v; do check APP_THINGSPEAK_WRITE_KEY "$v" ERROR;   done < <(values APP_THINGSPEAK_WRITE_KEY)
while IFS= read -r v; do check APP_WIFI_SSID            "$v" WARNING; done < <(values APP_WIFI_SSID)

HOSTENV=tools/thingspeak_host.env
if [ -f "$HOSTENV" ]; then
  if git ls-files --error-unmatch "$HOSTENV" >/dev/null 2>&1; then echo "check_secrets: ERROR: $HOSTENV is tracked by git"; status=1; fi
  git check-ignore -q "$HOSTENV" || { echo "check_secrets: ERROR: $HOSTENV is not ignored"; status=1; }
  check THINGSPEAK_READ_KEY "$(sed -n 's/^[[:space:]]*THINGSPEAK_READ_KEY[[:space:]]*=[[:space:]]*\(.*\)$/\1/p' "$HOSTENV" | tr -d '\r"'"'"' \t' | head -1)" ERROR
fi

[ $status -eq 0 ] && echo "check_secrets: clean (password, write key, read key not found; secrets files ignored and untracked)"
exit $status
