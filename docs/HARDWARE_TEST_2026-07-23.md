# Current Hardware Status

Date of latest device evidence: 2026-08-05.

## Latest flashed image

`firmware/rtos_nuttx_r528s3-dshanpi_minui-wifi-direct-mimo-tts_20260805_256Mnand.img`

SHA-256: `fd6923b20e69b306aff9d5ac778c63517a8f5ba49c63a4c1c012c92ddd1bf607`

This image is device-passed for the single-instance display baseline, direct
WiFi association, local speaker playback, and a MiMo TTS response. Audible
MiMo TTS is not yet passed because its fallback player exited too early.

## Device results

### Passed

- NSH remains usable after boot.
- The delayed launcher starts exactly one `study_terminal` when it is not also
  launched manually.
- LVGL opens `/dev/fb0` at 480x320 and opens `/dev/input0` successfully.
- Real WiFi association reached `wlan0 192.168.10.4` with a non-zero AP MAC;
  the placeholder `10.0.0.2` state was not accepted.
- `nxplayer` played a 1 kHz tone audibly through the physical speaker.
- MiMo TTS used `mimo-v2.5-tts`, completed TLS, received a WAV response, and
  decoded `30,720` PCM bytes at 24 kHz. This proves the network request path,
  not audible speech.

### Test mistake identified

Starting `study_terminal` manually before the 20-second delayed launcher runs
creates two LVGL processes. They contend for the same framebuffer/SPI device,
then trigger `spi1` DMA timeout, LVGL "No screen found", and a data abort.

For this image: reboot, wait at least 25 seconds, do not manually run
`study_terminal`, then use `ps` to confirm exactly one instance.

### Not passed / not available

- The UI WiFi re-scan path has a logic defect: it enters `MINUI hang isolate`
  instead of remaining in provisioning. Command-line association was used only
  to unblock hardware testing.
- `mediad` still fails at `amovie_async`; it is not repaired by the direct
  playback fallback.
- The fallback sends `q` to `nxplayer` immediately after `playraw`, so audible
  MiMo TTS is not yet confirmed.
- MiMo ASR and the wake word are not implemented or device-verified.

## Candidate acceptance image: build and offline verification passed

`firmware/rtos_nuttx_r528s3-dshanpi_minui-wifi-media-acceptance_20260805_256Mnand.img`

SHA-256: `9b76213f2a39ff00acccdf85ab79a332067672964ba9d26f37870a3ea139c28c`

This image was flashed and partially device-verified. It is superseded for
speaker and TTS acceptance because its media daemon cannot start.

### Build / package / offline evidence

- The UI is the proven direct-load MINUI baseline, with one static page and a
  WiFi overlay. The overlay starts radio work only after a touch; the full
  five-page UI was not included as this test UI.
- `wapi` scan, association, DHCP renew, real AP MAC plus IPv4 verification,
  and `wapi save_config` run in a background worker. The placeholder
  `wlan0 10.0.0.2` without an AP MAC is rejected.
- `mediad` and `mediatool` are enabled and linked. The board script starts
  `mediad &`, waits, then starts exactly one delayed `study_terminal &`.
- `/data/0.wav` is present for a local speaker baseline (16-bit mono, 8 kHz
  WAV). The image does not contain WiFi credentials.
- The image contains the MiMo v2.5 whitelist code, including
  `mimo-v2.5-pro`, `mimo-v2.5`, `mimo-v2.5-asr`,
  `mimo-v2.5-tts-voiceclone`, `mimo-v2.5-tts-voicedesign`, and
  `mimo-v2.5-tts`. Offline scans found no Volcengine host and no
  `mimo-v2-flash` or `mimo-v2-omni` string.

### Still device-pending

- MINUI plus the WiFi overlay must remain stable for at least three minutes.
- WiFi provisioning must obtain a real non-placeholder IPv4 and persist
  credentials without exposing the password.
- `mediad` must accept a local WAV playback through the speaker.
- MiMo TTS must return a successful response and be audible after the API key
  is configured locally with hidden input.
- MiMo ASR and wake word remain unimplemented and unverified.

### Device evidence after flashing the candidate

- The delayed launcher kept exactly one `study_terminal` process alive. No
  duplicate LVGL/SPI fault was observed.
- WiFi provisioning connected to a real access point. `wlan0` reported
  hardware address `88:49:2d:48:25:f7`, IPv4 `192.168.10.4`, and gateway
  `192.168.10.1`; this is not the rejected placeholder `10.0.0.2` state.
- `mediad` was not running after boot. Manually starting it failed while
  loading `/etc/media/graph.conf`: `No such filter: 'amovie_async'`, followed
  by `audio_graph init failed` and `media daemon exit`.

The failure is inside the media graph/FFmpeg integration, before any speaker
output, MiMo TTS request, ASR, or wake-word operation. Do not claim local
audio or MiMo TTS passed from this image.

### Local speaker baseline: passed

`nxplayer` played a 1 kHz tone at 48 kHz for three seconds and the tone was
audible on the physical board. The logs show `sunxi_audio_start`, then normal
`sunxi_audio_stop` and `playback exit`. The non-fatal `AUDIO_TYPE_FEATURE`
messages refer to unsupported audio-control features and did not stop
playback.

This proves the speaker, amplifier, PCM driver, and direct local playback
path. It does not repair or validate `mediad`, MiMo TTS, ASR, or the wake
word.

## Candidate direct-MiMo-TTS audible-wait image: build and offline verification passed

`firmware/rtos_nuttx_r528s3-dshanpi_minui-wifi-direct-mimo-tts-audible-wait_20260805_256Mnand.img`

SHA-256: `2ca067173513974b5000a78cdeeac6632e2f6e32f1b554ef343cfcae1e9334d8`

Size: `37,720,064` bytes.

This candidate changes only the direct TTS fallback timing. It sends `playraw`,
waits for the calculated PCM duration plus 250 ms, then sends `q` to
`nxplayer`. It retains the proven single delayed `study_terminal`, real WiFi
path, and `mediad` startup attempt. `mediad` remains a known failure.

### Build / package / offline evidence

- ARM DShanPi build completed successfully after compiling
  `src/voice/audio_playback.c` and completing the final link.
- Official `pack` completed, and official `dragon` completed with
  `Dragon execute image.cfg SUCCESS`.
- The final Windows `.img` contains the direct playback fallback log strings,
  `study_terminal`, `mediad`, `nxplayer`, exactly one `mediad &`, and exactly
  one delayed `study_terminal &`.
- The final `.img` scan found no `openspeech.bytedance.com`, `mimo-v2-flash`,
  or `mimo-v2-omni`. MiMo requests remain limited to the v2.5 route.
- The board seed `wapi.conf` is empty of credentials.

### Device-pending

- This image has not been flashed. It is build- and offline-verified only.
- `mediad` remains a known failure; this image does not claim to repair it.
- MiMo TTS request and decoding are device-passed on the preceding image;
  audible speech remains pending until this timing fix is flashed and tested.
- MiMo ASR and wake word remain unimplemented and unverified.

## Device acceptance after flashing the candidate

1. Verify the candidate SHA-256, flash it manually, reboot, wait 25 seconds,
   and do not manually start `study_terminal`.
2. Use `ps` to confirm exactly one `study_terminal`; `mediad` may exit because
   `amovie_async` remains unavailable.
3. Use the command-line WiFi helper only while the UI re-scan defect remains.
   A real AP MAC and non-placeholder IPv4 are required before calling the
   board connected. Treat the serial log as sensitive.
4. Start `ai_agent` manually, configure the MiMo key locally with hidden
   input, then run `voice_test_speak`. Require `nxplayer fallback playback`,
   normal `sunxi_audio_stop`/`playback exit`, and audible speech before marking
   TTS passed.

No API key, WiFi password, or Authorization value is recorded in this file.
