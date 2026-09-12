# WiFi stable image (no UI poll, no ai_agent at boot)

Updated: 2026-07-22

## Why previous hangfix still froze

Serial was completely dead after connect. Throttling alone was not enough.
The LVGL timer still called `wifi_is_connected()` -> `wapi_get_ap` +
`getifaddrs` after link-up. That path can lock up Realtek + UI together.

## This image changes

1. LVGL timer **never** calls wapi/getifaddrs; only consumes background job
   results (scan/connect/reconnect finished flags).
2. Boot no longer probes radio from UI thread.
3. System/network status refresh every 10 seconds.
4. Focus/AI pulse animations disabled.
5. Boot script starts **only** `study_terminal` (no `ai_agent` auto-start).
   After WiFi is stable, manually run: `ai_agent --no-cli &`

## Image

```text
D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_wifi-stable_20260722_256Mnand.img
size: 37284864
SHA256: 55e7a1a08694bab9cb0ebd6f7553c499f2137a6b1fa9646446a2275e304a7137
```

## Flash and verify

1. Flash this image.
2. Power on, wait for UI.
3. Connect WiFi (or wait auto reconnect if saved).
4. Must stay responsive 2-3 minutes.
5. Serial should still accept: `ps`, `ifconfig wlan0`, `cat /tmp/study-wifi.log`
6. Only after stable, run: `ai_agent --no-cli &`

## If still freezes

Next isolation steps:
- Temporarily disable WiFi auto-reconnect job entirely
- Or boot with UI only and configure WiFi via serial commands first
