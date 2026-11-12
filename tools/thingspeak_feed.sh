#!/usr/bin/env bash
# thingspeak_feed.sh -- read the latest entries of the ThingSpeak channel, to check uploads.
#
#   tools/thingspeak_feed.sh [number of entries, default 5]
#
# Reads THINGSPEAK_CHANNEL_ID and THINGSPEAK_READ_KEY (empty for a public channel) from
# tools/thingspeak_host.env (copy the .template; git-ignored). Uses HTTPS. Prints the channel's
# last entry id and, per entry, entry_id, created_at, field1..field3 -- nothing else of the channel
# (no name, description or location), and never the read key; the channel ID is shown with its
# first 2 digits only.
set -euo pipefail
cd "$(dirname "$0")/.."
ENV=tools/thingspeak_host.env
[ -f "$ENV" ] || { echo "thingspeak_feed: $ENV not found (copy $ENV.template)"; exit 2; }
get() { sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\(.*\)$/\1/p" "$ENV" | tr -d '\r"'"'"' \t' | head -1; }
CH=$(get THINGSPEAK_CHANNEL_ID); RK=$(get THINGSPEAK_READ_KEY); N=${1:-5}
[[ "$CH" =~ ^[0-9]+$ ]] || { echo "thingspeak_feed: THINGSPEAK_CHANNEL_ID must be a number"; exit 2; }
URL="https://api.thingspeak.com/channels/$CH/feeds.json?results=$N"
[ -n "$RK" ] && URL="$URL&api_key=$RK"
echo "# channel ${CH:0:2}$(printf '%*s' $((${#CH}-2)) '' | tr ' ' '*'), last $N entries, read $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
curl -s --max-time 30 "$URL" | python3 -I -c '
import json, sys
raw = sys.stdin.read()
try:
    d = json.loads(raw)
except ValueError:
    print("thingspeak_feed: no JSON in the response (wrong channel ID or read key?):", raw[:80]); sys.exit(1)
if not isinstance(d, dict) or "feeds" not in d:
    print("thingspeak_feed: unexpected response:", str(d)[:80]); sys.exit(1)
print("last_entry_id:", d.get("channel", {}).get("last_entry_id"))
print("%-9s %-21s %-10s %-10s %-10s" % ("entry_id", "created_at", "field1", "field2", "field3"))
for f in d["feeds"]:
    print("%-9s %-21s %-10s %-10s %-10s" % (f.get("entry_id"), f.get("created_at"), f.get("field1"), f.get("field2"), f.get("field3")))
'
