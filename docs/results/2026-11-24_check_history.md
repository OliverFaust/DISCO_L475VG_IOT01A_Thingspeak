# History check and pre-push hook (2026-11-24)

**What was added:**
- `tools/check_history.sh`: searches every commit and every commit message for the values in the
  git-ignored secrets files. It prints counts only.
- `tools/git-hooks/pre-push`: runs it on the commits being pushed, and stops the push on a password
  or a key.
- docs/01, 1.6: how to install the hook, and what to do if the check finds something.

`tools/check_secrets.sh` (files before a commit) is unchanged.

Tested on Ubuntu 25.04 with bash 5.2 and git 2.48.1. Windows (Git Bash) was not tested.

## On this repository

`tools/check_history.sh` with the real secrets files, on `main` at `b82e743` plus this branch's
working tree:

```
check_history: clean (9 value(s) checked in 72 commit(s): no password or key in files or messages)
```

- **72 commits:** all local branches, tags and remote-tracking branches, including
  `test/cmsis-driver-pr64` and `mbed-os-legacy`.
- **9 values:** 3 SSIDs, 3 passwords, 1 write key, the read key and the channel ID.
- **Run time:** under 1 s.

The hook was installed in this clone, and it ran on the push of this branch.

## With fake secrets, in a scratch clone

The test made a scratch clone of this repository and created fake secrets files:
- an SSID and a password;
- a second, commented-out password;
- the write key left as the template's placeholder;
- a fake read key;
- channel ID `1234567`.

It pushed to a local bare repository. Full transcript: `2026-11-24_check_history/fake_secrets_transcript.txt`.

| # | Case | Result |
|---|---|---|
| 1 | No secrets files | "nothing to check against", exit 2 |
| 2 | Fake secrets, none committed | clean, exit 0. The placeholder write key is skipped. Warning: channel ID found (see below). |
| 3 | Clean push through the hook | push succeeds |
| 4 | Commented-out password committed and deleted in the next commit; SSID in a file; read key in a commit message; a MAC address in `docs/results/` | ERROR for the password (still in the history) and for the read key (message). Warnings for the SSID, the channel ID and the MAC. Exit 1. No value is printed. |
| 5 | Only the history before the bad commits (`tools/check_history.sh HEAD~3`) | clean, exit 0 |
| 6 | Push of the bad commits through the hook | "pre-push: secrets found in the history; push stopped". The remote branch stays at the clean commit. |
| 7 | Deleting a remote branch (no commits pushed) | not checked, push succeeds |

**The channel ID warning in 2, 3, 5:** `1234567` is the example channel ID in `docs/02-thingspeak.md`,
`tools/thingspeak_feed.sh` and `tools/thingspeak_host.env.template`. The warning is therefore correct.
With the real channel ID there is no hit (above). The channel ID is matched as a whole word only, so
that it is not found inside longer numbers.
