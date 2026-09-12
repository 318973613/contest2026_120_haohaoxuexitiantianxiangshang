# UI Reference Images

These five images are the approved visual baseline for the 3.5-inch AI study
terminal. Keep the original files unchanged. Implementation should reproduce
the UI shown inside the device screen, not the surrounding poster or hardware
mockup.

| File | Screen | SHA256 |
| --- | --- | --- |
| `home.png` | Home dashboard | `947c3cc5b78c44ed48d02fc7ae5fb19c88e3cb6de72448d9a96b9e3f3701e9b8` |
| `ai-assistant.png` | AI assistant | `7f3c4b8bc4a12b6f30372d15025d9a4e494a7a282afc67404a49943a93ea6662` |
| `system-status.png` | System status | `2b48d0adb55e3dbc8fac0c5483c8a422eb35513c3ea641c028b0d3ce442e453c` |
| `settings.png` | Settings | `088884e911323b10a1e892c2042d94a8fa0ab10b4b6b2099f905a19934fd5355` |
| `study-tasks.png` | Study tasks | `ef23983a1dd1360c17ec095cb75dadb9e6a86f787b23d3565bf1103d30090523` |

All source images are PNG files at `1086 x 1448` pixels. They are design
references only and are not intended to be compiled into the firmware.

## Persistent Rules

- Target a 3.5-inch touch display first.
- Preserve the pale blue-white surface, dark navy text, blue primary accent,
  and restrained violet, mint, and orange status accents.
- Keep the top status bar, compact card grid, and bottom navigation hierarchy.
- Use the five named screens as the acceptance baseline for later screenshots.
- The official DShanPi source config uses a 320x480 RGB565 framebuffer. Keep the
  LVGL layout adaptive until physical orientation and touch mapping are
  verified on the real board.
