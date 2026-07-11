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
- Five-page LVGL study terminal implemented with Home, System Status, Study
  Tasks, AI Assistant, and Settings views.
- Light blue-white visual system and bottom navigation verified at 1280x800 in
  QEMU.
- Live heap, network, uptime, task counts, focus timer, and AI status preserved
  in the redesigned interface.
- NotoSansSC FreeType rendering verified with complete Simplified Chinese text;
  font license files are stored under `assets/fonts/`.
- Full single-thread CMake build completed successfully and generated
  `vela_ap.elf` after installing the required `mtools` package.
- Five real framebuffer screenshots saved under `assets/screenshots/`.

## Current Work

- Keep the QEMU demo reproducible while waiting for the official board BSP.
- Connect the verified ai_agent conversation stream to the AI screen.

## Submission Work Still Required

- Export the official AI Coding conversation logs with
  `contest-log-collector` into `logs/318973613/`.
- Verify the exported `manifest.json` and Codex JSONL files.
- Scan every exported log for API keys, authorization headers, passwords, and
  other sensitive values before committing it.

The sanitized files under `demo/runtime-logs/` are runtime verification
evidence. They do not replace the competition-required AI Coding logs under
`logs/`.

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

1. Export and sanitize the official AI Coding logs.
2. Connect live AI conversation text to the verified AI page.
3. Revalidate the responsive layout when the official board resolution and BSP
   become available.
