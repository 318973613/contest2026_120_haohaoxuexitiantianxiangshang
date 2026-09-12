# New Chat Handoff: Validate MiMo TTS Audible Playback

Read `AGENTS.md` first. Work from the device evidence below; do not assume
older status notes are still correct.

## What is flashed now

The user flashed and device-tested:

`D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_minui-wifi-direct-mimo-tts_20260805_256Mnand.img`

SHA-256:
`fd6923b20e69b306aff9d5ac778c63517a8f5ba49c63a4c1c012c92ddd1bf607`

Read `D:\openvela\CURRENT_HARDWARE_STATUS.md` and the matching VM project
document `docs/HARDWARE_TEST_2026-07-23.md` before editing anything.

## Candidate image (flashed; media acceptance failed)

`D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_minui-wifi-media-acceptance_20260805_256Mnand.img`

SHA-256:
`9b76213f2a39ff00acccdf85ab79a332067672964ba9d26f37870a3ea139c28c`

It keeps one delayed
`study_terminal`, starts `mediad`, restores explicit-touch WiFi provisioning,
rejects placeholder AP/IP state, and includes `/data/0.wav` plus `mediatool`
for a local speaker test. It is not device-verified.

## Real device results

1. NSH, framebuffer, touch, and exactly one delayed `study_terminal` are
   stable. After reboot, wait at least 25 seconds; never manually run
   `study_terminal` before its delayed launcher fires.
2. A manual early start plus the delayed start creates two LVGL processes,
   causes `spi1` DMA timeout, then `No screen found` and a data abort. This is
   a duplicate UI-process fault, not proof that a single UI instance is bad.
3. The on-screen WiFi re-scan has a logic defect: it enters `MINUI hang
   isolate`. Do not regard it as an accepted provisioning UI.
4. The old `wlan0 10.0.0.2` condition remains rejected; it is never Internet
   access.
5. The candidate's real WiFi association passed: `wlan0` obtained
   `192.168.10.4` with gateway `192.168.10.1` and non-zero AP hardware state.
   This is genuine association, not the old `10.0.0.2` placeholder.
6. MiMo TTS used `mimo-v2.5-tts`, completed TLS, received `30,764` WAV bytes,
   and decoded `30,720` PCM bytes at 24 kHz. The request path is device-passed
   but audible speech is not yet confirmed because the fallback exits early.
7. MiMo ASR and wake word are still unimplemented and unverified.
8. `mediad` fails before playback. Its board media graph starts with
   `amovie_async`, but the linked FFmpeg reports `No such filter:
   'amovie_async'`, then exits. This does not block the direct TTS fallback.
9. The independent local speaker baseline passed: `nxplayer` produced an
    audible 1 kHz, 48 kHz, three-second tone. The physical audio hardware and
    direct PCM playback are therefore working; the unresolved failure is
    specifically the `mediad` media graph.

## New direct-MiMo-TTS audible-wait candidate (not flashed)

`D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_minui-wifi-direct-mimo-tts-audible-wait_20260805_256Mnand.img`

SHA-256:
`2ca067173513974b5000a78cdeeac6632e2f6e32f1b554ef343cfcae1e9334d8`

This image keeps the known-good single-instance UI and real WiFi path. Its
`audio_playback` code keeps `media_player` as the first choice, then falls
back to the device-verified `nxplayer playraw` route only when `mediad` cannot
open, prepare, or start a Music stream. The previous fallback issued `q`
immediately and could stop audio before it was heard. This candidate waits for
the calculated PCM duration plus 250 ms before sending `q`. It does not claim
to fix `mediad`.

It built and packed successfully. Final image scans found the fallback,
`mediad`, `nxplayer`, and one delayed `study_terminal`; they found no
`openspeech.bytedance.com`, `mimo-v2-flash`, or `mimo-v2-omni`. It has not
been flashed or device-verified.

## Next hardware test, in order

Do not migrate the Windows high-fidelity UI. Validate audible TTS first.

1. Flash the new audible-wait candidate manually after verifying its SHA-256.
   Reboot and wait 25 seconds; do not manually start `study_terminal`.
2. Confirm `ps` shows exactly one `study_terminal`. `mediad` is still expected
   to fail because `amovie_async` is not available.
3. Use the temporary command-line WiFi helper only if the UI re-scan still
   enters `MINUI hang isolate`; do not copy serial output because board driver
   logs may expose connection secrets.
4. Start `ai_agent`, configure the MiMo key locally with hidden input, then
   run `voice_test_speak`. Require `nxplayer fallback playback`, normal audio
   completion logs, and audible speech before marking TTS passed.
5. Keep the MiMo model whitelist strict:
   `mimo-v2.5-pro`, `mimo-v2.5`, `mimo-v2.5-asr`,
   `mimo-v2.5-tts-voiceclone`, `mimo-v2.5-tts-voicedesign`,
   `mimo-v2.5-tts`. Never send a Volcengine request or old MiMo model.
6. Update the status documents with device evidence. Only after audible TTS
   passes may work continue to MiMo ASR, wake word, and then the Windows
   high-fidelity UI migration.

## Device acceptance after the next flash

1. Reboot and wait 25 seconds without manually starting `study_terminal`.
2. Confirm `ps` shows exactly one `study_terminal`.
3. Confirm online state has a real non-placeholder IP. The UI WiFi re-scan is
   a known defect, so command-line setup is temporarily allowed.
4. Make one TLS MiMo TTS request only after the key is configured locally with
   hidden input.
5. Run `voice_test_speak` and require successful fallback, normal playback
   completion, and audible speaker output.
6. Only after these pass, implement MiMo ASR, then the wake word, then migrate
   the Windows high-fidelity LVGL visual design while retaining the proven
   single-instance startup design.

## Constraints

- Do not ask the user to reveal API keys or WiFi passwords.
- Do not commit, push, OTA, or flash on the user's behalf.
- Do not revert existing dirty worktrees or overwrite source files blindly.
- Explain which files will change and why before changing code.
- Record build-verified versus device-verified results separately.
