# WiFi hangfix image

Updated: 2026-07-22

## Symptom

Board freezes shortly after WiFi connects (serial unresponsive).

## Likely cause

After link-up, UI timer called `wifi_is_connected()` every 500ms, which
opens sockets and calls `wapi_get_ap` + `getifaddrs` while Realtek WiFi
worker threads are also busy. Combined with 12KB connect worker stack and
1Hz system/network refresh, this can hang the board.

## Fixes in this image

1. `wifi_setup.c`
   - Worker stack 12KB -> 24KB
   - While background WiFi job is active, skip link polling
   - Link poll every ~2.5s instead of 500ms
   - Offline auto-reprovision threshold adjusted to match slower poll
2. `study_terminal_main.c`
   - System/network widget refresh every 3s instead of every 1s

## Image

- Path: `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_wifi-hangfix_20260722_256Mnand.img`
- Size: 37288960
- SHA256: `d490645640450cea30eddd007cb4046d9b404906c4ef7d8ad9aa6b9683e206e5`

## After flash verify

1. Boot stays alive
2. Connect WiFi (or auto reconnect)
3. After connect, screen remains responsive for at least 2-3 minutes
4. Serial `nsh>` still accepts `ps` / `ifconfig wlan0`
5. `/tmp/study-wifi.log` has `job connect success` or `connected ip=...`

## Notes

- Not commit / not push
- If still hangs, next step is move connect/scan fully off LVGL thread and
  reduce/remove concurrent getifaddrs during DHCP
