# Project Progress

Updated: 2026-07-15

## Project

AI study and debugging display terminal based on openvela and ai_agent.

## Route Decision - 2026-07-15

- Quick App development has stopped as the final product route after repeated
  DShanPi UIKit/prebuilt-library compatibility failures produced a running
  `vapp` process but an all-black framebuffer.
- The final product is now the repository-owned native LVGL C application plus
  `ai_agent`.
- Quick App source, RPK files, screenshots, and diagnostic evidence are kept as
  historical work. They are not deleted and are not the next implementation
  target.
- The verified LVGL baseline is `app/hello_app/` at commit `63a55a8`, with five
  pages, Chinese font rendering, QEMU framebuffer captures, live status, task
  file reading, and reminder presentation.
- Initial LVGL/ai_agent integration will use bounded shared files under
  `/data/ai_agent/`. UDP or message queues are deferred until the hardware
  baseline is stable.
- The DShanPi defconfig now disables Quick App, UIKit, QuickJS, and their
  framework-only dependencies; it enables the contest `study_terminal`,
  ILI9341, FT5X06, RGB565 framebuffer, LVGL libuv, and the fonts used by the UI.
- The board startup script now launches `wifi_manager` and native
  `study_terminal` instead of `vapp`.
- The first native LVGL ARM LTO build completed successfully in about 13 minutes
  47 seconds. `nuttx.bin` is 9,961,348 bytes; the ELF contains
  `study_terminal_main` and contains neither `vapp_main` nor
  `xiaozhi_gui_main`.
- Generated `.built`, `.depend`, `Make.dep`, and object files under
  `app/hello_app/` are untracked build artifacts and must not be submitted.
- Official DShanPi `pack` regenerated `nsh.fex`, `res.fex`, and `usrdata.fex`.
  The existing user-local i386 runtime then completed the official `dragon`
  step with `Dragon execute image.cfg SUCCESS`.
- The old board userdata Quick App directory and MiSans fonts were moved, not
  deleted, to `/home/openvela/openvela/_local_backups/quickapp-20260715/`.
  `NotoSansSC-Regular.ttf` was installed at the `/data/` path expected by the
  native LVGL application, with matching source and destination SHA256 values.
- Final native LVGL image:
  `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl_256Mnand.img`.
  Size: 36,743,168 bytes. SHA256:
  `ad652bfcbb1d5f139a401ffba636dc3298eae44e02bc081fb4e4ff6973fe6303`.
- Offline inspection found `study-terminal.sh`, `study_terminal &`, and
  `NotoSansSC-Regular.ttf`; it found no `vapp hap://`, Quick App package name,
  `manifest.json`, or MiSans font marker. The image has not been flashed.
- The next DShanPi defconfig now enables LVGL's official system/performance
  monitor and bottom-right overlay: `CONFIG_LV_USE_SYSMON=y`,
  `CONFIG_LV_USE_PERF_MONITOR=y`, and
  `CONFIG_LV_PERF_MONITOR_ALIGN_BOTTOM_RIGHT=y`.
- A new full LTO build is intentionally deferred until the 480x320 landscape
  layout changes are batched, avoiding a firmware rebuild for every small UI
  adjustment.
- The current application remains builtin. Full NAND images still require the
  verified USB flashing path; ordinary network copy cannot activate them.
  `/data` resources can be updated through ADB or network transfer. ELF/ADB
  support exists in the configuration, but a loadable LVGL application has not
  yet passed symbol-export and hardware loading validation.
- The UI orientation requirement was corrected to 480x320 landscape. Portrait
  framebuffers are rotated clockwise with `LV_DISPLAY_ROTATION_90`; already
  landscape QEMU displays remain unchanged.
- A lightweight boot screen presents the product name and an animated progress
  bar for about 1.6 seconds, then fades into the five-page UI.
- All interface changes must pass a computer-side QEMU build, launch, and
  screenshot review before any hardware packaging or flashing.

## Completed

- GitHub contest repository invitation, fork, and SSH access verified.
- Full `dev-ai-contest-2026` repo workspace synchronized.
- Official `vendor_allwinnertech` synchronized to commit `14bc07b`, which adds
  `boards/r528/r528s3-dshanpi/`.
- DShanPi board defconfig and non-empty boot binary verified in the synchronized
  checkout.
- The upstream 10-file trunk 5.5 patch passed `patch --dry-run -p1` against the
  current `external/opus/opus` and `nuttx` revisions.
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
- A functional openvela Quick App was added under
  `quickapp/hello_quickapp/` while retaining the LVGL implementation as a
  stable fallback.
- The Quick App implements five real views, focus timing, a 10-second reminder,
  persistent tasks, network/battery/device/storage status, velaclaw AI queries,
  and persistent settings.
- The Quick App built successfully on Windows and generated
  `com.openvela.contest2026.studyterminal.debug.1.0.0.rpk`.
- Official MiSans fonts were deployed to QEMU `/data/font/`; complete Chinese
  rendering was verified from an RGB565 framebuffer capture.
- Real emulator touch events verified Home, System Status, Study Tasks, and the
  Complete Next Task action. The status page reported Emulator-Vela,
  NuttX 0.0.0, 1280x800, and 233.4 MB available storage.
- The velaclaw request, quickapp message queue, and reply bridge were verified.
  The current userdata has no available LLM backend, so the latest UI has not
  yet been revalidated against a real MiMo response.

## Current Work

- Run the existing QEMU `study_terminal` baseline for fast UI iteration before
  making further layout changes.
- After explicit confirmation, flash the native LVGL image and verify non-black
  output, five-page navigation, Chinese text, touch mapping, task-file reading,
  and reminder state.

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

## Hardware Bring-up Status

The official upstream target is now present at
`vendor/allwinnertech/boards/r528/r528s3-dshanpi/` on commit `14bc07b`. Its
README identifies the 100ASK R528S3 DShanPi competition board with a 3.5-inch
SPI display and capacitive touch. Source configuration uses a 320x480 RGB565
framebuffer with ILI9341 and FT5X06 enabled.

The required `dshanpi_for_trunk5.5.patch` is now applied locally to `nuttx` and
`external/opus`. The official DShanPi target has compiled and a packable image
has been generated and inspected offline. It has not been flashed or tested on
hardware. Do not substitute another R528 board configuration, and do not treat
the source display settings as physical validation.

## Next Verification

1. Cold-start QEMU, deploy `/tmp/study-terminal-final.rpk`, and launch
   `ai_agent &` followed by
   `vapp hap://app/com.openvela.contest2026.studyterminal`. Do not restart
   vapp inside the same QEMU session because the second LVGL initialization can
   assert.
2. Re-enter the MiMo configuration locally without recording the key, then
   verify that the full AI reply renders as scrollable text.
3. Verify the 10-second reminder, task text input/add, settings switches, and
   persistence across a cold restart.
4. Save final framebuffer evidence, then commit and push
   `feat/high-fidelity-study-ui`.
5. Record clean public-repository revisions, apply the official DShanPi patch,
   and inspect all resulting diffs.
6. Compile the official `r528s3-dshanpi/configs/nsh` target before packing or
   flashing.
7. Verify display orientation and touch mapping, then revalidate all five
   responsive pages at the source 320x480 target size.
8. Export and sanitize the official AI Coding logs.

## 2026-07-13 Windows High-Fidelity UI Update

- The visual source of truth is now the five reference crops under
  `D:\openvela\_staging\high-fidelity-crops\`.
- A temporary whole-image overlay experiment was rejected and fully removed.
  The current implementation uses real Quick App components and remains
  interactive.
- The Windows `vela-watch-5.0` runtime scales fixed Quick App coordinates by
  about 1.5. The root layout is now constrained to an `853x533` logical canvas,
  which maps to the 1280x800 framebuffer without clipping.
- The component-based Home view now fits the framebuffer with two primary
  panels, a system strip, two quick actions, and four bottom navigation items.
- The System Status view now uses a stability banner, four independent metric
  cards, and three lower information panels. This replaces the clipped
  two-column table layout.
- Windows runtime evidence reported WiFi, Emulator-Vela, NuttX 12.3.0,
  1280x800, 255.9 MB total storage, and 149.9 MB available storage.
- `refreshStatus()` now checks the existence of network, battery, device, and
  individual methods before calling them. Missing battery support displays
  `不可用` without preventing later device and storage queries.
- AI calls and voice controls now detect a missing `system.velaclaw` feature
  and return a friendly message instead of remaining busy indefinitely.
- Root-level task and AI text inputs were moved immediately after the top bar
  in DOM order because the native input layer ignored late style overrides and
  previously rendered below the framebuffer. Final task/AI placement still
  requires a cold-start screenshot pass.
- Voice UI sends `__VELACLAW_VOICE_START__` and
  `__VELACLAW_VOICE_STOP__` through the existing velaclaw ask API. The required
  ai_agent public-repository dispatch implementation is not yet applied and
  must remain separate from the contest repository.
- `npm run build` succeeds. The expected toolkit warning for the unrecognized
  build-time feature `system.velaclaw` remains.
- The latest `index.ux` and `manifest.json` were synchronized to the Ubuntu
  contest checkout on branch `feat/high-fidelity-study-ui`.
- No commit or push was performed.

### Immediate Next Steps

1. Cold-start the Windows simulator and finish task input placement and task
   add/complete/remove verification.
2. Finish AI input, voice button, and missing-velaclaw fallback verification.
3. Capture all five component-rendered pages and compare them against the
   reference crops. Do not use full-screen static image overlays.
4. Implement ASR dispatch only in the `packages/ai_agent` public repository,
   then verify it in QEMU with a locally entered MiMo key.
5. Update final evidence and documentation before requesting permission to
   commit or push.

## 2026-07-14 Five-Page Font and Layout Pass

- Each page was tested separately through a build, cold simulator start, and
  screenshot: Home, System Status, Study Tasks, AI Assistant, and Settings.
- Unsupported glyphs such as `⌁`, `▣`, `◎`, `♢`, `◆`, `✦`, `⌂`, `›`,
  `◇`, `✓`, and the full-width plus sign were removed from the UI.
- The top bar now uses stable `WiFi` and battery text chips. Bottom navigation
  uses Chinese characters and ASCII text that render correctly with MiSans.
- The Settings page uses dedicated 20-percent navigation items so all five
  destinations fit inside the 1280x800 framebuffer.
- Home, Status, AI, and Settings screenshots show no missing-glyph boxes or
  incoherent overlap.
- The Windows 5.0 image creates native text inputs but does not consistently
  composite them into the framebuffer. Study Tasks and AI now include visible
  Quick App input shells at the same positions while retaining real input
  controls and `onchange` handlers.
- The obsolete unsupported `z-index` declaration was removed; the runtime no
  longer reports `Unknown style - zIndex`.
- The final Windows `npm run build` succeeded. The only remaining expected
  build warning is the toolkit's unknown `system.velaclaw` feature.
- The default page was restored to Home, and the latest `index.ux` and
  `manifest.json` were synchronized to the Ubuntu contest checkout.
- No commit or push was performed.

### Remaining Verification

1. Manually focus the task and AI input regions and confirm that the retained
   native inputs update their bound values.
2. Recheck task actions, reminder behavior, settings persistence, and cold
   restart state.
3. Deploy the latest RPK to QEMU and verify MiSans, velaclaw, and ASR there.

## 2026-07-15 DShanPi Image Build

- All nine Quick App ARMv7A prebuilt libraries were verified as real `ar`
  archives. `libapps_vapp.a` contains the ARM `vapp_main` symbol.
- The official DShanPi `scripts/Make.defs` now links the same nine libraries as
  the upstream Gemini implementation, inside
  `--whole-archive/--no-whole-archive`.
- The DShanPi configuration uses Newlib math and LVGL Vector Graphics. The
  legacy generic `CONFIG_LIBM=y` selection was removed because it took
  precedence over `CONFIG_LIBM_NEWLIB=y` and omitted QuickJS math symbols.
- A targeted LVGL clean temporarily produced an incomplete `libapps.a` because
  unchanged packages kept their `.built` markers. Running the standard
  `make -C apps clean` followed by one unified rebuild restored the complete
  application archive.
- The final ARM LTO build completed with exit code 0. `nuttx.bin` is
  12,108,944 bytes, and the linked ELF contains `416ad1a0 T vapp_main`.
- The firmware no longer fit the original 10 MiB DShanPi bootloader partition.
  Following the official same-SoC Gemini layout, the bootloader partition was
  increased from 20,480 to 32,768 sectors. The usrdata partition was reduced
  by the same 12,288 sectors, preserving the total partition size.
- The DShanPi pack flow prefers board-specific `data/usrdata` over the common
  `UDISK` fallback. The unpacked Quick App and both MiSans fonts were therefore
  copied into the DShanPi board-specific usrdata input before the final pack.
- The resulting `usrdata.fex` is 14,086,144 bytes. Pack logs list the Quick App
  manifest, page bundle, five page assets, `MiSansW_Regular.ttf`, and
  `MiSansW_Demibold.ttf` as written YAFFS objects.
- The official Allwinner `dragon` tool is a 32-bit i386 executable. The Ubuntu
  VM did not provide `/lib/ld-linux.so.2`, and sudo was not used. Ubuntu's
  official `libc6-i386`, `lib32stdc++6`, and `lib32gcc-s1` packages were
  downloaded and unpacked under
  `/home/openvela/.cache/openvela-dragon-i386/` for a user-local runtime.
- The final image is:
  `vendor/allwinnertech/lichee/out/r528s3/dshanpi_nand/rtos_nuttx_r528s3-dshanpi_uart0_256Mnand.img`
- Image size: 37,015,552 bytes. Build time: 2026-07-15 05:57 UTC. SHA256:
  `cc948846c1cdd0dad638e98139447bc3485759ac365e775a3bc7ae53c690c09d`.
- Both `usrdata.fex` and the final image contain the package name,
  `manifest.json`, both MiSans font names, and page asset names, confirming the
  latest Quick App payload is present.
- No board was detected, so no flashing or hardware validation was performed.
  No commit or push was performed. Public BSP, NuttX, packaging, and board-data
  changes must remain separate from the contest repository submission.

## 2026-07-15 DShanPi Study UI Autostart Image

- `CONFIG_XIAOZHI_GUI_APP` is disabled. Control Center remains compiled only
  because WiFi and audio commands depend on its `ipc_endpoint_create_udp`
  implementation; it is not launched at boot.
- The DShanPi ROMFS `rcS` now launches `/data/study-terminal.sh`. That script
  starts `wifi_manager`, waits two seconds, then runs
  `vapp hap://app/com.openvela.contest2026.studyterminal`.
- The ARM LTO link completed successfully. `nuttx.bin` is 12,088,432 bytes.
  The ELF contains `416ab8a0 T vapp_main` and does not contain
  `xiaozhi_gui_main`.
- The official DShanPi `envsetup.sh`, `lunch_nuttx r528s3-dshanpi`, and `pack`
  flow regenerated the partition images. The existing user-local i386 runtime
  then completed the official `dragon image.cfg sys_partition_for_dragon.fex`
  step.
- The new image is 36,999,168 bytes, generated at 2026-07-15 06:56 UTC.
  SHA256: `84162a5cd8ab0a1549c4f336090f276d30a674533ae6fe8e755ac0cca7cd7d1f`.
- Offline checks found the contest package name, `manifest.json`, both MiSans
  fonts, `study-terminal.sh`, and the target `vapp` command in the image. The
  old `sh /data/xiaozhi.sh &` boot command was not found.
- No commit, push, or flashing was performed. Hardware display, touch, and
  runtime behavior still require validation on a physical DShanPi.

## 2026-07-15 DShanPi Long Package Name Fix

- A physical-board boot log was captured from COM15 at 1500000 baud. The new
  firmware launched `/data/study-terminal.sh`; `vapp` opened the 320x480
  RGB565 framebuffer and `/dev/input0`. XiaoZhi did not start.
- `ps` showed the contest `vapp` process still running, but accessing the full
  application directory returned NuttX error 36. `ls /data/app` showed the
  directory truncated to `com.openvela.contest2026.studyt/`.
- The package name is 38 characters while the DShanPi configuration used
  `CONFIG_NAME_MAX=32`. The DShanPi `defconfig` and generated configuration
  were minimally updated to `CONFIG_NAME_MAX=64`.
- The ARM LTO rebuild succeeded. `nuttx.bin` is 12,088,432 bytes. The ELF
  contains `416ab9a0 T vapp_main` and no `xiaozhi_gui_main`.
- A third image was generated at 2026-07-15 07:22 UTC. It is 36,999,168 bytes.
  SHA256: `7330c8125d7ed8803ff3e30cd065d0a83375be0eebd245dfb0b72677f51ec715`.
- Offline checks found the full package name, manifest, both MiSans fonts,
  study-terminal boot script, and target `vapp` command. The old XiaoZhi boot
  command was absent. No commit, push, or flashing was performed; the third
  image still requires physical-board validation.

## 2026-07-15 DShanPi Quick App LVGL Color Depth Fix

- After flashing the third image, the complete application directory became
  accessible and NuttX error 36 disappeared. The contest `vapp` process stayed
  alive, but it did not draw a page.
- The complete 307,200-byte 320x480 RGB565 `/dev/fb0` buffer was captured over
  COM15 and reconstructed as an image. It was fully black; the physical LCD
  noise was stale panel data left when no valid page frame was submitted.
- After temporarily stopping `vapp`, the official `fb` example displayed its
  test pattern correctly. This verifies the ILI9341, SPI update path, RGB565
  framebuffer, and physical LCD.
- Official Gemini and other Quick App configurations use
  `CONFIG_LV_COLOR_DEPTH_32=y`, while DShanPi used 16-bit LVGL. The linked
  Quick App/UIKit prebuilts follow the official Gemini environment, making
  the internal LVGL color-depth mismatch the next concrete incompatibility.
- DShanPi was minimally changed to 32-bit internal LVGL while retaining the
  320x480 RGB565 framebuffer and `CONFIG_NAME_MAX=64`.
- The fourth ARM build, official pack flow, and dragon encapsulation succeeded.
  The image is 36,999,168 bytes, generated at 2026-07-15 07:48 UTC. SHA256:
  `3f7940c3653e3cbaa60c2c0fcc87d74e98794a22eceab7d4a954b6c51bc054d7`.
- No commit, push, or flashing was performed for this fourth image.

## 2026-07-16 Native LVGL Landscape and Performance Baseline

- Quick App remains archived as historical work. The final route is the native
  `study_terminal` LVGL C application plus `ai_agent` and `study-assistant`.
- Runtime orientation handling was added. A portrait 320x480 framebuffer is
  rotated clockwise to 480x320 with LVGL display rotation; an already-landscape
  QEMU display is left unchanged.
- A lightweight approximately 1.6-second startup screen, progress bar, and fade
  into the main UI were added. Compact layout rules are enabled at resolutions
  up to 600x400.
- LVGL's upstream system monitor and performance monitor are enabled for QEMU
  and DShanPi. The 480x320 QEMU run displayed approximately 33-36 FPS and
  0-1 percent CPU in the lower-right corner.
- The bottom tab bar reserves 152 pixels for the performance monitor. The Home
  page screenshot confirms that the monitor no longer overlaps the Settings
  tab and that the first screen fits without scrolling.
- The latest complete QEMU incremental build finished all 91 Ninja steps and
  printed `#### build completed successfully (56 seconds) ####`. The generated
  `nuttx.bin` is 14,983,168 bytes with timestamp 2026-07-16 13:45 UTC.
- Verified screenshots are stored locally at:
  `D:\openvela\_staging\lvgl-route-switch\screenshots\study-home-compact-152.bmp`
  and `study-status-480x320.bmp`.
- The Status page still clips its lower component panel at 480x320. The user
  explicitly paused visual refinement and requested that product functionality
  be completed first. A proposed compact Status patch was not applied after the
  editing tool failed, so the source remains at the screenshot baseline.
- No new DShanPi image containing these changes was generated. The earlier
  native image with SHA256 beginning `ad652bfc` predates landscape, performance,
  and compact-layout work and must not be treated as the latest image.
- No flashing, commit, or push was performed.

## 2026-07-16 Function-First Handoff

- The next implementation boundary is a small file protocol under
  `/data/ai_agent/`: `STUDY_TASKS.md`, `UI_STATUS.json`, `LAST_REPLY.txt`,
  `REMINDERS.json`, and `STUDY_SETTINGS.json`.
- Writers must use a temporary file followed by atomic rename. Readers must
  tolerate missing, partial, or invalid data and keep the last valid state or a
  safe default. Runtime evidence must not contain credentials or private full
  conversations.
- Required implementation order is: real system status, task CRUD, AI status
  and latest-reply synchronization, active reminders, then settings
  persistence.
- Every completed function requires a 480x320 QEMU incremental build, runtime
  interaction, framebuffer capture, and log check before moving to the next.
- DShanPi already enables ELF and BINFS and provides wget/curl, so a loadable
  ELF delivered over WiFi is a plausible later fast-update path. It still needs
  symbol-export, lifecycle, version-check, and rollback validation and must not
  block the contest feature path.

## 2026-07-16 Real System Status Verified

- Added `docs/FILE_PROTOCOL.md` before feature work. It defines the five files
  under `/data/ai_agent/`, temporary-file plus atomic-rename writes, invalid-data
  fallback, update frequency, field limits, and credential redaction rules.
- Replaced the Status page's hard-coded CPU text and process-heap estimate with
  NuttX system data. `uname()` supplies the runtime architecture, `sysinfo()`
  supplies core count, available system memory, and uptime, and `getifaddrs()`
  supplies the active non-loopback IPv4 interface.
- Interfaces that are down or still have `0.0.0.0` are now treated as offline.
  The Home and AI connection labels follow the same live network result.
- The 480x320 QEMU incremental build recompiled
  `study_terminal_main.c`, completed 45/45 steps, and reported
  `#### build completed successfully (28 seconds) ####`.
- After cold boot, the existing font installer restored
  `/data/NotoSansSC-Regular.ttf`. The Status page then showed `arm64 / 1 核`,
  `107 MB`, `eth0 10.0.2.15`, and a continuously increasing uptime.
- Live refresh was exercised in one running app instance: `ifdown eth0` changed
  the network card to `离线`; `ifup eth0` restored `eth0 10.0.2.15` without
  restarting the app.
- Verified framebuffer evidence is stored locally in
  `D:\openvela\_staging\lvgl-route-switch\screenshots\study-status-ifdown.bmp`
  and `study-status-ifup.bmp`. Both are 480x320, non-black, and render Chinese.
- Runtime logs contained no assert, panic, abort, segmentation fault, or app
  crash. The expected LVGL driver messages remained informational.
- System status is complete. The next allowed feature is task CRUD synchronized
  with `/data/ai_agent/STUDY_TASKS.md`; AI synchronization, reminders, and
  settings persistence have not been started.
- No commit, push, DShanPi image generation, flashing, or visual refinement was
  performed.

## 2026-07-16 Task CRUD and Voice Companion Foundation

- Task management now reads and renders real entries from
  `/data/ai_agent/STUDY_TASKS.md`. The native LVGL page provides add, complete,
  delete-completed, and sync actions.
- Task writes use `/data/ai_agent/STUDY_TASKS.md.tmp`, `fflush()`, `fsync()`,
  `fclose()`, and atomic `rename()` replacement. Missing files safely produce
  an empty task list.
- 480x320 QEMU touch injection exercised the complete CRUD cycle. The file
  changed from `- [ ] 新的学习任务 1` to `- [x] 新的学习任务 1`, then became
  empty after the clear action. Runtime logs reported `result=ok` for add,
  complete, and delete.
- Framebuffer evidence is stored locally as `study-tasks-after-add.bmp`,
  `study-tasks-after-complete.bmp`, and `study-tasks-after-clear.bmp` under
  `D:\openvela\_staging\lvgl-route-switch\screenshots\`.
- Added a first voice-wake service to the separate public `packages/ai_agent`
  worktree. The wake phrase is `小维同学`. It enters a half-duplex dialogue,
  waits for the AI's spoken response, accepts follow-up speech, and returns to
  wake listening when the dialogue ends.
- No compatible local R528 keyword-spotting model was present in the synced
  source. The first implementation is explicitly labeled a cloud-ASR fallback;
  it is disabled until the user locally configures Volcengine ASR credentials.
  Missing credentials now cause a clean refusal rather than a retry loop.
- QEMU verified `voice_wake_test 小维同学` as a match and `voice_wake_test 你好`
  as no match. `voice_wake_start` without credentials printed a local setup
  instruction and remained stopped.
- Heartbeat now supports `@channel voice`. The contest heartbeat template reads
  unfinished study tasks and requests a short Chinese spoken reminder. Internal
  `HEARTBEAT_OK` and generic backend errors are suppressed instead of spoken.
- Added installable `guided-study`, `device-diagnosis`, and
  `daily-study-review` Skills plus one installer script. They use existing
  read-only tools and redact credentials and private conversation history.
- The final QEMU incremental build completed 43/43 steps in 55 seconds.
  `nuttx` is 188,338,064 bytes and `nuttx.bin` is 14,987,264 bytes, timestamped
  2026-07-16 15:35 UTC.
- COM15 is present as a CH343 serial device, but the current board session did
  not expose a readable NSH prompt. The new firmware was not flashed, so
  microphone, speaker, ASR, TTS, and real wake-word behavior remain pending
  explicit user-approved flashing and local credential setup.
- Public `packages/ai_agent` changes remain separate from the contest repository.
  Existing `.built` and `.depend` deletion state was preserved and not cleaned.
- No commit, push, DShanPi image generation, or flashing was performed.

## 2026-07-19 Native LVGL, AI, and Voice DShanPi Image

- Enabled the native DShanPi `ai_agent` application alongside the existing
  `study_terminal`. The board launcher now starts `wifi_manager`, `ai_agent`,
  and `study_terminal` in the background.
- Added `voice_wake.c` to the legacy Make build and enabled `SYSTEM_POPEN`,
  which are required by the R528 build path. Unused Feishu, Weixin, MQTT,
  Node, and MCP channels remain disabled.
- Installed `study-assistant`, `guided-study`, `device-diagnosis`, and
  `daily-study-review` into board usrdata. The heartbeat template is present
  at both the runtime heartbeat path and the compatibility config path.
- The ARM LTO build completed successfully. `nuttx.bin` is 10,146,564 bytes;
  the ELF exports `ai_agent_main` and `study_terminal_main` and does not export
  `vapp_main` or `xiaozhi_gui_main`. LVGL remains configured for 16-bit color.
- Official `pack` regenerated `nsh.fex`, `res.fex`, and `usrdata.fex`. The
  existing user-level i386 runtime executed the official `dragon` tool with
  `Dragon execute image.cfg SUCCESS`.
- Final Windows image:
  `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl-ai-voice_256Mnand_20260719.img`.
  It is 36,961,280 bytes with SHA256
  `c86c1692b961a921722d7129b333f67cc25f5fe0d64b07cbcdaff3b01ef5fbc6`.
- Ubuntu and Windows hashes match. Offline inspection found the launcher,
  four study skills, heartbeat files, Noto Sans SC font, and voice wake CLI
  commands. No Quick App or Xiaozhi autostart command was found.
- The image has not been flashed. Microphone, speaker, ASR, TTS, physical wake
  behavior, display rotation, and touch mapping still require explicit
  user-approved hardware testing. No commit or push was performed.

## 2026-07-22 Touch, Refresh, and WiFi Hardware Result

- Enabled FT5X06 X/Y swapping and inverted the resulting landscape X
  coordinate. The user confirmed that physical touch now matches the display.
- Made the SPI LCD framebuffer frequency configurable and selected 24 MHz for
  DShanPi instead of the previous hard-coded 8 MHz.
- Restored the logo and progress-bar startup sequence without the expensive
  multi-frame full-screen fade on the compact display.
- Added a native LVGL WiFi provisioning module with scanning, AP selection,
  password keyboard, background connection, persistence, boot reconnect, and
  an offline provisioning trigger. ARM LTO, pack, and dragon completed.
- Generated image
  `rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-touch-perf_20260722_256Mnand.img`,
  size 36,965,376 bytes, SHA256
  `45d4e1fe210ab73348ae2ac04112a8afea3c1ed2d4d8d93ecca56937935c604c`.
- The user flashed this image. Touch remained correct, but WiFi provisioning
  failed on hardware: scanning and connection did not complete, the expected
  provisioning screen did not remain visible, and the app entered Home.
- WiFi is therefore build-complete but not functionally verified. Work is
  paused at the user's request. The next session must first collect read-only
  startup evidence for the feature guard, initialization call, overlay order,
  worker thread, `wapi scan wlan0`, and scan-result file before changing code.
- No commit or push was performed. The assistant did not initiate flashing.

## 2026-07-22 Serial Console Input Contention Fix

- Hardware reset produced an `nsh>` prompt, but input was unreliable and the
  console alternated between `vela>` and `nsh>`. Source inspection confirmed
  that the background AI agent CLI continuously reads `stdin` while NSH reads
  the same `/dev/console`.
- This is the confirmed direct cause of the serial command loss. The legacy
  continuous full-frame SPI LCD thread is not started, framebuffer updates use
  dirty areas, and UART0 has a 4096-byte RX DMA buffer, so LVGL resource usage
  is not the established root cause.
- Added `--no-cli` handling to the separate public `packages/ai_agent`
  worktree. It skips only `nsh_commands_start()` and preserves the AI, network,
  tool, voice, and heartbeat services.
- Changed the DShanPi launcher to start `ai_agent --no-cli &`, leaving NSH as
  the sole serial input consumer. Running `ai_agent` without the option keeps
  the existing interactive CLI behavior.
- The incremental ARM LTO build completed successfully. `nuttx.bin` is
  10,150,660 bytes and the ELF contains both application entry points,
  `--no-cli`, and the `stdin CLI disabled` boot marker.
- Official partition generation completed and the existing user-level i386
  runtime executed the official `dragon` tool successfully. Offline checks
  found `ai_agent --no-cli &` in `usrdata.fex` and the new option and boot
  marker in `nsh.fex`.
- Generated Windows image:
  `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-console-fix_20260722_256Mnand.img`.
  It is 36,965,376 bytes with SHA256
  `b870c19008752484a0a7638b5dfc2f95b70c130d2daacac6931753976b13aea4`.
- The new image has not been flashed. Serial exclusivity and WiFi provisioning
  remain unverified on hardware. The next step is to verify stable NSH input,
  then collect redacted `ps`, `ifconfig wlan0`, WiFi log, and scan-file
  evidence before making any WiFi code change. OTA remains blocked.
- No commit or push was performed. The assistant did not initiate flashing.

## 2026-07-22 WiFi true-device + OTA skeleton

- WiFi on device PASS: wlan0 10.28.239.98, AP 42:a4:f9:fe:ef:61, setup page usable.
- Image: rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-ota_20260722_256Mnand.img
  SHA256 877c567e366821ce3e74ec5acef322e055513aecf814879e60b2a5be79fe8356
- OTA helper present at /data/apps/study-ota.sh; curl available.
- Full loadable-ELF app OTA not proven. First smoke interrupted by long serial
  command wrapping and dual study_terminal crash.
- wifi_setup.c log spam fixed in source (log only on connect state change);
  not yet re-flashed.
- See docs/STATUS_WIFI_OTA.md and docs/FLASH_WIFI_OTA.md.

### OTA smoke on true device (2026-07-22 late)

- curl download PASS: curl -o /tmp/o http://10.28.239.150:8080/m got 13 bytes OTA_SMOKE_OK
- install PASS: copied to /data/apps/m, content verified
- helper present: /data/apps/study-ota.sh
- still builtin UI; full loadable ELF OTA not claimed
- details: docs/STATUS_WIFI_OTA.md

## 2026-07-22 UI redesign merge

- Merged Windows redesign from `_staging/lvgl-ui-redesign-win` into board
  `app/hello_app/study_terminal_main.c`.
- Retained true-device features: `wifi_setup_init`, FT5X06 landscape touch
  invert, task CRUD (add/complete/clean/sync), `ai_agent --no-cli`.
- Wallpaper asset installed to board usrdata as
  `/data/study-terminal-wallpaper.rgb565`.
- ARM LTO build PASS; official pack + i386 dragon PASS.
- Generated image:
  `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_ui-redesign_20260722_256Mnand.img`
  size 37288960, SHA256
  `f34ff399dfcd140450870f67ccf60167efea307a030996dd26d57a3c61c72716`.
- Offline checks found `wifi_setup_init`, `task action=add`, wallpaper,
  `study-ota.sh`, and `ai_agent --no-cli`.
- Not yet user-flashed for UI redesign visual confirmation.
- Docs: `docs/FLASH_UI_REDESIGN.md`, updated `docs/STATUS_WIFI_OTA.md`.
- No commit / no push.

## 2026-07-22 WiFi hangfix

- Symptom: board freezes shortly after WiFi association/DHCP.
- Mitigation image built:
  
tos_nuttx_r528s3-dshanpi_wifi-hangfix_20260722_256Mnand.img
  size 37288960, SHA256
  d490645640450cea30eddd007cb4046d9b404906c4ef7d8ad9aa6b9683e206e5.
- Changes: wifi worker stack 24KB; skip link poll during active WiFi job;
  poll link every ~2.5s; system/network UI refresh every 3s.
- See docs/FLASH_WIFI_HANGFIX.md.
- Not commit / not push; needs true-device reflash verification.

## 2026-07-23 UI Route and Startup Script Correction

- The delivery route is the repository-owned native LVGL C five-page application.
- There are two distinct Windows UI artifacts: the Quick App prototype is
  historical only, while `_staging/lvgl-ui-redesign-win` is a native LVGL
  redesign candidate and is not a Quick App runtime.
- The current `full-lvgl-delay20` diagnostic image temporarily uses the older
  native LVGL five-page baseline. It does not contain the Windows native LVGL
  redesign wallpaper and updated layout, so it is not the final visual build.
- The min-ui build was diagnostic only. It proved that framebuffer, LVGL init,
  touch input, and a delayed 20-second native UI start are stable on hardware.
- The no-screen boot symptom was traced to `CONFIG_NSH_LINELEN=64`: a long
  shell comment was split and its trailing `ng` was executed as a command.
  The launcher now keeps every line below the NSH limit.
- The current full-LVGL delay image starts only `study_terminal`. It does not
  claim WiFi, `ai_agent`, or voice-wake hardware verification.
- Voice and ASR code are present in the build, but microphone capture, wake word,
  cloud ASR/TTS, continuous conversation, and proactive voice reminders remain
  pending explicit true-device validation.
- The next UI build should re-merge the Windows native LVGL redesign while
  retaining the short launcher, 20-second delay, and verified touch mapping.
  It must not restore the Quick App runtime.
- No commit, push, OTA, or assistant-initiated flashing was performed.

## 2026-09-11 连续语音对话与主页提醒联动

- 接续另一任务未完成的候选，实现“你好小米”统一唤醒、同句短问题保留、本地 150 ms 提示音、唤醒自动开页、等待/播报期间禁止重新录音和迟到回复协调。
- 交互模型默认 `mimo-v2.5`、关闭思考、最多 768 tokens；ASR/TTS 继续使用各自专用模型。私有预置配置的两处聊天模型同时切换，凭据与其他设置保持原值。
- 主页显示最近一个本地提醒的真实截止时间，可异步取消；原子持久化失败会回滚，独立专注统计不被覆盖。小屏聊天内容可滚动，最近回复保持 UTF-8 字符完整。
- 200 个基线文件校验无意外变动，仅同步 24 个候选文件；128 个对象及构建标记已备份后移走，未删除 libapps.a。旧镜像和 ELF 仍可恢复。
- 38 组主机 ASan/UBSan 检查通过，含实际 LVGL/FreeType 的 480×320 和 1280×800 渲染；这些不是板端验收。15 个改动 C 文件确认重编，ARM make 退出 0。
- 官方 Dragon SUCCESS / pack finish；ELF 按板级 objcopy 参数导出的完整 bin 与 nsh.fex 一致；YAFFS 还原启动脚本、音频及配置后核验；Windows/VM 哈希一致。
- 新镜像：`D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_voice-dialogue-prekeyed-candidate_20260911_256Mnand.img`，40,824,832 bytes，SHA-256 `af1655aee2f72607a67cb37601dfa74a5717dd5c0f458a0a7b39d3d9bfce54ce`。
- 配对 ELF：`D:\openvela\archive\nuttx-20260911-voice-dialogue-candidate.elf`，68,584,104 bytes，SHA-256 `9bb192d148da9ba5cb36e5f40a3b193d0b7e3c360f5484256f9df848e8ce6a11`。
- VM 快照：`/home/openvela/img_backups/voice-dialogue-candidate_20260911_155223`；构建核验细节、测试和待验清单见 `docs/voice-dialogue-20260911.md`。
- 用户确认上一轮镜像可运行、识别较准，但回复/界面联动未完成验收。本版未烧录，不承诺固定回复耗时。镜像含预置凭据，不进入比赛仓或外发；公共 ai_agent 源码改动不混入专属仓。本次未 commit、未 push。
- 2026-09-12 用户告知 MiMo Key 已过期。本镜像仍沿用原配置，主机检查不调用 MiMo 云端；后续先在本地更新有效凭据，不索取聊天中的明文 Key。真实 ASR、聊天与 TTS 暂不能按有效服务验收。
