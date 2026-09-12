# WiFi fix + OTA baseline image (2026-07-22)

## Image

- Path: `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-ota_20260722_256Mnand.img`
- Size: 36,973,568 bytes
- SHA256: `877c567e366821ce3e74ec5acef322e055513aecf814879e60b2a5be79fe8356`
- Built: 2026-07-22 11:15 UTC (approx)

## What changed

### WiFi
- Root cause: `wlan0` had NETINIT placeholder IP `10.0.0.2` without AP association.
- Old code treated any non-zero IPv4 as connected and hid the setup overlay.
- New code requires **associated AP MAC != 00:00:00:00:00:00** AND a non-zero IPv4.
- Placeholder IP alone is ignored (`not connected: placeholder ip without ap`).
- Logs in `/tmp/study-wifi.log` (no password).

### OTA (minimal skeleton)
- Boot script prefers `/data/apps/study_terminal` if executable, else builtin.
- Helper: `/data/apps/study-ota.sh <url> [sha256]`
- Downloads to `/tmp`, optional hash check, installs to `/data/apps/study_terminal`, restarts UI.
- **Not full loadable-ELF OTA yet**: current UI is still builtin. After flash, file/resource updates and future standalone ELF experiments can use this path. Kernel/LVGL config changes still need flash.

### Console
- Still starts `ai_agent --no-cli &` so NSH owns the serial console.

## Flash (user)

1. Verify SHA256 on Windows:
   ```powershell
   Get-FileHash D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-ota_20260722_256Mnand.img -Algorithm SHA256
   ```
2. Flash with PhoenixSuit / official NAND path.
3. Serial: COM15, 1500000.

## Verify after flash

```text
nsh> ps
nsh> ifconfig wlan0
nsh> cat /tmp/study-wifi.log
nsh> cat /tmp/study-wifi-scan.txt
nsh> wapi show wlan0
```

Success criteria:
1. Screen stays on WiFi setup until real association.
2. Scan list includes iQOO (or nearby APs).
3. Enter password on device (do not share password).
4. `wapi show` AP MAC not all zeros.
5. `wlan0` has a real DHCP IPv4 (not fake unassociated placeholder).
6. Reboot reconnects when config saved.
7. Serial remains usable `nsh>` only.

OTA smoke (after network works):
```text
nsh> ls /data/apps
nsh> cat /data/apps/study-ota.sh
# example after hosting a file:
# sh /data/apps/study-ota.sh http://HOST/study_terminal <sha256>
```

## Not done yet (next)

1. True-device WiFi verification with iQOO.
2. Business features in fixed order: status, tasks, AI sync, reminders, settings.
3. Real loadable ELF + LVGL symbol export if OTA of C UI is required without flash.
