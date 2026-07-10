# Project Progress

Updated: 2026-07-11

## Project

AI study and debugging display terminal based on openvela and ai_agent.

## Completed

- GitHub contest repository invitation, fork, and SSH access verified.
- Full `dev-ai-contest-2026` repo workspace synchronized.
- Team repository mapped at
  `contest2026_120_haohaoxuexitiantianxiangshang/`.
- Stock `goldfish-arm64-v8a-ap` target compiled and booted in QEMU.
- Reproducible ai_agent configuration compiled with
  `CONFIG_EXAMPLES_AI_AGENT_VELA=y` and
  `CONFIG_FEATURE_SYSTEM_VELACLAW=y`.
- `start-ai-simulator.sh` verified for cold start, an existing outer NSH
  prompt, and an existing `vela>` prompt.
- MiMo configuration verified with a masked key, the expected endpoint, and
  model `mimo-v2.5-pro`.
- A real MiMo conversation completed successfully over TLS.
- Custom `study-assistant.md` Skill loaded from
  `/data/ai_agent/skills/`.
- Learning task creation verified through `get_current_time`, `read_file`,
  and `write_file` tool calls.
- Learning task review verified by reading
  `/data/ai_agent/STUDY_TASKS.md`.
- A 30-second one-shot rest reminder was created with `cron_add`, fired, sent
  an active notification, and deleted itself.
- Sanitized runtime evidence saved under `demo/runtime-logs/`.

## Current Work

- Design and implement the minimum LVGL study-terminal interface.
- Keep the QEMU path reproducible while waiting for the official board BSP.

## Evidence

- `docs/AI_AGENT_RUNTIME.md`: reproducible build, launch, configuration, and
  verification procedure.
- `demo/runtime-logs/ai-agent-runtime-2026-07-11.md`: sanitized runtime
  results with no API key or authorization header.

## External Dependency

The official board code for the 100ASK DShanPixVela-Devkit V1 has not been
verified in the current contest workspace. Do not select, modify, or flash a
similar R528 board configuration. Continue using QEMU until the official BSP
is available and identified from upstream documentation or source.

## Next Verification

1. Build the minimum LVGL interface in the contest repository.
2. Run it in the supported QEMU configuration.
3. Verify the home, system status, task, and AI views without changing public
   `nuttx`, `packages`, or `vendor` repositories.
