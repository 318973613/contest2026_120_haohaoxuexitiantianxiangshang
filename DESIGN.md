# Design

## Source of truth

- Status: Active
- Last refreshed: 2026-07-11
- Primary product surfaces: Home, System Status, Study Tasks, AI Assistant,
  Settings
- Evidence reviewed: `assets/ui-reference/*.png`, `docs/PROGRESS.md`,
  `docs/AI_AGENT_RUNTIME.md`, `src/skills/study-assistant.md`

The five images under `assets/ui-reference/` are the approved visual baseline.
Match the interface inside the device screen; ignore the promotional poster
background and rendered device shell.

## Brand

- Personality: calm, capable, focused, trustworthy
- Trust signals: visible system health, explicit tool activity, persistent task
  state, clear reminder status
- Avoid: marketing layouts, oversized headings, dark gaming themes, decorative
  blobs, dense developer-only diagnostics, and ambiguous icon-only actions

## Product goals

- Goals: make study tasks, device health, AI help, and reminders readable at a
  glance on a 3.5-inch touch screen
- Non-goals: desktop administration, full chat history management, physical BSP
  adaptation before the official board code is available
- Success signals: five screens are reachable, legible, stable, and demonstrable
  in QEMU without modifying public openvela repositories

## Personas and jobs

- Primary persona: a beginner learning embedded development and debugging
- User jobs: see today's focus, check device health, review tasks, ask AI for
  help, and configure essential terminal behavior
- Key context: short interactions on a small desk-side display

## Information architecture

- Primary navigation: bottom tabs for Home, Status, Tasks, and AI
- Settings entry: top-right gear; Settings may show a fifth active navigation
  item as in the approved reference
- Core screens: Home, System Status, Study Tasks, AI Assistant, Settings
- Content hierarchy: title/status bar, primary work area, persistent navigation

## Design principles

1. Glanceable first: the most important state must be readable without scrolling.
2. Small-screen discipline: prefer short labels, stable card sizes, and large
   touch targets over decorative content.
3. Honest system state: loading, offline, tool-running, success, and error states
   must be explicit.
4. Reference fidelity: new UI decisions should preserve the approved images'
   hierarchy and tone rather than introduce a new visual theme.

## Visual language

- Color: cool white and pale blue surfaces; navy text; vivid blue primary;
  violet AI, mint success, and orange warning accents
- Typography: built-in LVGL fonts initially; Chinese-capable font asset must be
  added before final hardware delivery
- Spacing/layout rhythm: 8 px base unit, 12-16 px card padding, compact gaps
- Shape/radius/elevation: 8 px or smaller card radius where practical, subtle
  borders and restrained shadows, no nested decorative cards
- Motion: short page transition and progress updates only; no continuous ambient
  animation on the 3.5-inch device
- Imagery/iconography: LVGL symbols or project-owned bitmap icons; no remote
  assets at runtime

## Components

- Existing components to reuse: LVGL labels, buttons, bars, arcs, switches,
  text areas, and flex/grid layouts
- New components: top status bar, metric tile, task row, AI message bubble,
  tool activity panel, bottom navigation item, settings row
- Variants and states: default, active, pressed, loading, success, warning,
  error, disabled
- Token ownership: colors, spacing, radii, and typography live in the
  project-owned study-terminal source

## Accessibility

- Target standard: readable small-screen interface with strong contrast and
  touch targets of at least 40 logical pixels on the QEMU baseline
- Keyboard/focus behavior: preserve LVGL group/focus compatibility where input
  devices support it
- Contrast/readability: dark text on light surfaces; color is never the only
  status signal
- Screen-reader semantics: not available in the current embedded stack; use
  explicit visible labels
- Reduced motion: all functions remain usable with transitions disabled

## Responsive behavior

- Supported devices: 3.5-inch physical target and current QEMU framebuffer
- Layout adaptations: compute sizes from `LV_HOR_RES` and `LV_VER_RES`; keep
  navigation and status bars fixed while the content grid adapts
- Touch/hover differences: touch-first; hover is not required

The official DShanPixVela-Devkit V1 panel resolution is still unknown. Do not
hard-code an assumed physical resolution or borrow a similar R528 BSP.

## Interaction states

- Loading: show a short status label and progress indicator
- Empty: explain that no tasks or reminders exist and expose the next action
- Error: show a plain-language reason and a retry action
- Success: use mint accent plus confirmation text
- Disabled: reduce contrast but retain readable labels
- Offline/slow network: keep local task and system pages usable; mark AI as
  offline or reconnecting

## Content voice

- Tone: concise, supportive, and factual
- Terminology: use `首页`, `状态`, `任务`, `AI`, and `设置` consistently
- Microcopy rules: one action per label; avoid technical acronyms unless they
  are the information being inspected, such as CPU or Wi-Fi

## Implementation constraints

- Framework/styling system: repository-owned C application using upstream LVGL
- Design-token constraints: no new public framework or public repository change
- Performance constraints: static layouts, bounded object count, no large
  reference PNGs compiled into firmware
- Compatibility constraints: QEMU first; physical board work waits for the
  official DShanPixVela-Devkit V1 BSP
- Test/screenshot expectations: build successfully, launch in QEMU, capture all
  five screens, and compare against `assets/ui-reference/`

## Open questions

- [ ] Confirm the official 3.5-inch panel resolution from the upstream BSP.
- [ ] Confirm touch controller and input mapping from the upstream BSP.
- [ ] Select and license a compact Chinese LVGL font before final hardware demo.
- [ ] Decide whether the first QEMU milestone uses static demo data or live
  ai_agent/task status for every card.
