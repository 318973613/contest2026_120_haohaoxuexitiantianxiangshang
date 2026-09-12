# WiFi + OTA status (2026-07-22)

## WiFi true-device: PASS

Verified on flashed wifi-ota image, then carried into later images:

- `wlan0` example: `10.28.239.98`
- Associated AP example: `42:a4:f9:fe:ef:61`
- User confirmed on-screen WiFi setup works
- Serial remains stable `nsh>` with `ai_agent --no-cli`
- Root cause of earlier fail: NETINIT placeholder `10.0.0.2` without AP was treated as connected

## OTA path smoke: PASS for /data file update

Short serial commands verified:

```text
curl -o /tmp/o http://10.28.239.150:8080/m
# 13 bytes, content OTA_SMOKE_OK
cp /tmp/o /data/apps/m
cat /data/apps/m
# OTA_SMOKE_OK
```

Meaning:

- Board can HTTP download from LAN host
- `/data/apps/` is writable
- Recommended OTA style: short `curl` then `cp`
- Complex `study-ota.sh` under NuttX `sh` is unreliable (`URL=:` parse issue)

## OTA limits

1. Current UI is still builtin `study_terminal`
2. C/UI/kernel changes still need rebuild + flash
3. Smoke proves `/data` file update, not loadable ELF UI replacement

## Related images

| Image | Role | SHA256 |
|---|---|---|
| wifi-ota | WiFi fix + OTA skeleton (user flashed, WiFi PASS) | `877c567e...` |
| ui-redesign | Windows UI merge + WiFi/touch/task CRUD | `f34ff399...` |

See also:

- `docs/FLASH_WIFI_OTA.md`
- `docs/FLASH_UI_REDESIGN.md`
