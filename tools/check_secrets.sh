#!/usr/bin/env bash
# check_secrets.sh -- make sure no secret from Core/Inc/app_secrets.h is about to be committed.
#
#   tools/check_secrets.sh          (run from the repository root, before every commit)
#
# Reads APP_WIFI_SSID, APP_WIFI_PASSWORD and APP_THINGSPEAK_WRITE_KEY from Core/Inc/app_secrets.h
# and searches for each value in: the files staged for commit (the index), all tracked files, and
# everything under docs/results/ (tracked or not). Prints only file names and which secret was
# found, never the value. Also checks that app_secrets.h itself is ignored and not tracked.
# Exit status: 0 = clean, 1 = a secret was found or app_secrets.h is tracked, 2 = setup problem.
#
# The SSID is reported as a warning only (it is not secret in itself, but a home network name
# identifies a place; console logs mask it, see tools/mask_log.py).
set -uo pipefail

SECRETS=Core/Inc/app_secrets.h
[ -f "$SECRETS" ] || { echo "check_secrets: $SECRETS not found (nothing to check against)"; exit 2; }

value() {   # value of a #define "..." in app_secrets.h
  sed -n "s/^[[:space:]]*#define[[:space:]]\+$1[[:space:]]\+\"\(.*\)\".*/\1/p" "$SECRETS" | head -1
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

check APP_WIFI_PASSWORD        "$(value APP_WIFI_PASSWORD)"        ERROR
check APP_THINGSPEAK_WRITE_KEY "$(value APP_THINGSPEAK_WRITE_KEY)" ERROR
check APP_WIFI_SSID            "$(value APP_WIFI_SSID)"            WARNING

[ $status -eq 0 ] && echo "check_secrets: clean (password and write key not found; $SECRETS ignored and untracked)"
exit $status
