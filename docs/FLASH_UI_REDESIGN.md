# UI redesign merge - ready to flash

Updated: 2026-07-22

## Image

- Path: `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_ui-redesign_20260722_256Mnand.img`
- Size: 37,288,960 bytes
- SHA256: `f34ff399dfcd140450870f67ccf60167efea307a030996dd26d57a3c61c72716`
- Built: 2026-07-22 12:54 UTC approx

## What was merged

- Windows redesign UI from `D:\openvela\_staging\lvgl-ui-redesign-win`
- Board WiFi setup (`wifi_setup_init`)
- FT5X06 landscape touch alignment
- Task CRUD: add / complete / clean / sync + live rows from `STUDY_TASKS.md`
- Wallpaper: `/data/study-terminal-wallpaper.rgb565`
- Kept `ai_agent --no-cli` and OTA helper path

## Offline checks

- `nsh.fex`: `wifi_setup_init`, `task action=add`, placeholder log
- `usrdata.fex`: wallpaper, `study-ota.sh`, `ai_agent --no-cli`

## After flash verify

1. New visual home / status / tasks / AI / settings
2. WiFi still works and reconnects
3. Task page: add / complete / clean / sync writes `/data/ai_agent/STUDY_TASKS.md`
4. Touch mapping still correct
5. Single `study_terminal` instance only

## Notes

- Not commit / not push
- Source on Ubuntu: `contest.../app/hello_app/study_terminal_main.c`
- Merge tools: `D:\openvela\_staging\ui-merge\`
