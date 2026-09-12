# Voice Companion QEMU Evidence - 2026-07-16

This file records sanitized development evidence. It does not contain API keys,
tokens, passwords, WiFi credentials, authorization headers, or private full
conversation transcripts.

## Build

- Target: `goldfish-arm64-v8a-ap-ai-agent`, 480x320 QEMU.
- Initial voice-wake integration build: 129/129 steps, success.
- Final incremental build: 43/43 steps, 55 seconds, success.
- Final `nuttx.bin`: 14,987,264 bytes, timestamp 2026-07-16 15:35 UTC.

## Wake State Machine

```text
vela> voice_wake_status
Wake listener: stopped; phrase: 小维同学; mode: cloud ASR fallback

vela> voice_wake_test 小维同学
Wake match: yes

vela> voice_wake_test 你好
Wake match: no

vela> voice_wake_start
Wake listener needs local ASR configuration. Run set_volc_asr without sharing credentials.
```

Result: PASS for command registration, phrase matching, configuration gating,
and stopped-state safety. Real microphone and speaker behavior cannot be proven
in QEMU.

## Proactive Reminder Routing

After installing the contest heartbeat template:

```text
[heartbeat] Triggered agent check on channel=voice
[agent] Processing message from voice:heartbeat
```

Result: PASS for voice-channel routing. No MiMo or TTS credentials were loaded
in this cold QEMU userdata, so no model response or audio playback was claimed.

## Task CRUD

Real emulator touch events invoked the native LVGL buttons:

```text
study_terminal: task action=add result=ok
- [ ] 新的学习任务 1

study_terminal: task action=complete result=ok
- [x] 新的学习任务 1

study_terminal: task action=delete result=ok
```

Result: PASS. The task file was empty after delete-completed. Framebuffers were
non-black and the runtime showed no assert, panic, abort, or application crash.

## Model and Official Logs

- This phase did not make a MiMo request because the rebuilt QEMU userdata had
  no local model credentials.
- The previously verified MiMo runtime evidence remains in
  `demo/runtime-logs/ai-agent-runtime-2026-07-11.md`.
- Official AI Coding logs are separate and still require final export through
  `contest-log-collector` into `logs/318973613/`, followed by a sensitive-data
  scan before submission.

## Hardware Boundary

- Windows detected `USB-Enhanced-SERIAL CH343 (COM15)`.
- The current session did not provide a readable NSH prompt at the checked baud
  rates.
- No firmware was generated or flashed in this phase. Speaker, microphone,
  Volcengine ASR/TTS, and real wake behavior remain unverified on DShanPi.
