# Assets

Store project-owned visual and audio resources here.

## UI References

`ui-reference/` contains the five user-approved visual baselines for the
3.5-inch study terminal: Home, AI Assistant, System Status, Settings, and Study
Tasks. These full-size PNG files are design references and must not be compiled
into the firmware.

See `ui-reference/README.md` for page mapping, checksums, and persistent design
rules. The implementation contract is maintained in the repository root
`DESIGN.md`.

## Fonts

`fonts/NotoSansSC-Regular.ttf` provides complete Simplified Chinese rendering
for the LVGL FreeType path. `fonts/OFL.txt` and `fonts/README.txt` preserve its
upstream license and attribution information.

## Runtime Screenshots

`screenshots/` contains five real 1280x800 QEMU framebuffer captures: Home,
System Status, Study Tasks, AI Assistant, and Settings. They are verification
evidence, not firmware backgrounds.

Rules:

- Keep only files required to build or demonstrate the project.
- Record the source and license of third-party assets before submission.
- Preserve approved reference images byte-for-byte.
- Use optimized project-owned icons or LVGL symbols in firmware instead of the
  full reference posters.
