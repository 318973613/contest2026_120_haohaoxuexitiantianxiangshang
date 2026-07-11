# Study Terminal LVGL App

This directory is the manifest-mapped application location for the contest
project. The original `hello_app_main.c` template is retained, while the
build files now register the real `study_terminal` command.

## Screens

- Home: current focus, task count, next break, and AI state.
- Status: CPU architecture, heap, network address, and uptime.
- Tasks: task-file counts and study workflow state.
- AI: MiMo provider status, model, tools, and custom Skill count.
- Settings: reminders, live refresh, animation, and debug switches.

The interface uses FreeType with the project-owned
`assets/fonts/NotoSansSC-Regular.ttf` file for complete Chinese rendering. If
the runtime font is missing, the application falls back to the built-in LVGL
CJK font so it can still start.

## Build

The contest QEMU defconfig enables:

```text
CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL=y
```

Build from the openvela workspace root:

```bash
export PATH="$PWD/prebuilts/tools/linux/x86_64:$PWD/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/.local/libcxxabi/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cmake --build cmake_out/demo_goldfish-arm64-v8a-ap-ai-agent -j1
```

After QEMU reaches the outer NSH prompt, prepare the Chinese font once:

```bash
contest2026_120_haohaoxuexitiantianxiangshang/demo/scripts/install-study-ui-font.sh
```

At the outer NSH prompt, run:

```text
study_terminal
```

For deterministic demo capture, an initial tab can be selected:

```text
study_terminal status
study_terminal tasks
study_terminal ai
study_terminal settings
```

This application is currently verified on QEMU only. It is not a physical
board BSP or flashing configuration.
