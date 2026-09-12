# Documentation

Project design, verified procedures, hardware notes, and submission material
belong in this directory.

Do not store API keys, passwords, Wi-Fi credentials, or GitHub tokens here.

## Where to start

**`CURRENT_HARDWARE_STATUS.md` is the single authority for what has actually been
verified.** It separates four different conclusions that are easy to confuse:
source exists / built and packed / flashed / verified on real hardware. Read it
before trusting any other document here.

## Current status and round records

- `CURRENT_HARDWARE_STATUS.md` - authoritative acceptance status, current
  candidate, and the list of things that must not be re-attempted
- `successful_builds.md` - build / pack / ELF pairing record for every candidate
- `flash-checklist-wake-word-20260913.md` - flash and on-device test checklist
  for the current wake-word candidate, including what cannot be tested without a
  valid model key
- `submission-20260913.md` - contest submission record (PR, compliance checks,
  document-sync follow-up)

### Round records

Newest first. Each one holds the full change list, boundaries, and acceptance
steps for that round.

- `wake-word-20260912.md` - unified wake phrase `你好，openvela`; ASCII case
  folding; brand wake word removed
- `ui-consistency-20260912.md` - study-tools pages aligned with the home screen
  visual language; study report tab
- `study-offline-20260912.md` - offline study tools: reminder centre, offline
  chime, five-minute snooze, focus goal, seven-day records
- `voice-dialogue-20260911.md` - continuous voice dialogue and reminder linkage
- `voice-reply-20260906.md` - touch and TTS ownership fix after an AI reply
- `voice-touch-20260905.md` - voice / touch candidate

## Reference documents

- `HARDWARE_BSP.md` - official DShanPi BSP source, sync evidence, patch
  boundary, and hardware bring-up checklist
- `FILE_PROTOCOL.md` - atomic file formats shared by the native LVGL app,
  `ai_agent`, and `study-assistant`
- `AI_AGENT_RUNTIME.md` - `ai_agent` process, channels, and runtime behaviour
- `PROGRESS.md` - chronological project progress
- `FLASH_WIFI_OTA.md`, `FLASH_WIFI_STABLE.md`, `FLASH_WIFI_HANGFIX.md`,
  `FLASH_UI_REDESIGN.md` - earlier flash notes and verify checklists
- `HARDWARE_TEST_2026-07-23.md`, `STATUS_WIFI_OTA.md` - historical test records
