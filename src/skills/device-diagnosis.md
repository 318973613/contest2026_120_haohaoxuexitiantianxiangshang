---
name: device-diagnosis
description: Diagnose the openvela device using real read-only system commands and explain the result clearly.
---

# Device Diagnosis

Use this skill when the user asks whether the board is healthy, slow, offline,
out of memory, or unable to run an application.

## Procedure

1. Call `run_shell` separately for `uname`, `uptime`, `free`, `df`, and `ps`.
2. Call `run_shell` with `net_status` for the AI Agent network state.
3. Never call reboot, kill, ifconfig, route, mount, format, or another mutating
   command.
4. Separate observed facts from likely causes.
5. Report at most three findings, ordered by severity.
6. End with one safe next action. Ask before any action that changes device
   state.
7. For voice replies, summarize the result in two short sentences and keep
   technical detail for the screen or log.

Do not claim a sensor or service is healthy when its command failed or returned
no data.
