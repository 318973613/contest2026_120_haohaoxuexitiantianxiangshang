# 主机回归测试（83 组）

这里是技术报告 3.5 节「83 组主机检查」的**夹具与运行脚本**。报告里的数字可以用这套代码复现。

## 为什么要有这一层

真机只能回答「能不能跑」，回答不了「边界条件下会不会出错」。这 83 组检查在 **x86 主机**上
把板端真正参与构建的 C 模块编出来，配上确定性的设备/网络夹具，跑 ASan + UBSan，覆盖真机上
不方便反复制造的场景：时钟未校时、NTP 失败、内存分配失败、HTTP 错误、空 ASR 结果、
超时与取消竞态等。

## 覆盖范围

| 组 | 夹具 | 组数 | 被测源码 |
|---|---|---|---|
| voice | `voice_integration.c` | 23 | `packages/ai_agent/src/voice/voice_channel.c`、`voice_wake.c`、`app/hello_app/voice_ui_bridge.c` |
| chat | `chat_integration.c` | 3 | `app/hello_app/ai_chat_bridge.c` |
| cron | `cron_integration.c` | 23 | `packages/ai_agent/src/infra/cron_service.c`、`src/tools/tool_cron.c` |
| focus | `focus_stats_test.c` | 8 | `app/hello_app/study_focus_stats.c` |
| api | `api_integration.c` | 4 | `packages/ai_agent/src/llm/llm_proxy.c`、`llm_parse.c`、`src/voice/mimo_asr.c` |
| ui | `ui_integration.c` | 11 × 2 | `app/hello_app/study_terminal_main.c`（480×320 与 1280×800 各 11 组）|

合计 **83 组**。

## 运行前提

1. **完整 openvela 工作区**：`nuttx/`、`packages/`、`apps/`、`external/` 都要在。
   本仓是被 manifest `<linkfile>` 挂进工作区的子仓，**单独 clone 本仓跑不起来**。
   工作区不在 `/home/openvela/openvela` 时用 `OPENVELA_ROOT` 指定。
2. **主机工具链**：`gcc`（支持 `-fsanitize=address,undefined`）、`cmake` ≥ 3.16、`python3`。
3. UI 组另外需要工作区自带的 `external/freetype/freetype` 与 `apps/graphics/lvgl/lvgl`。
   渲染快照转 PNG 需要 Pillow；没有则保留 PPM，不影响判定。

## 运行

```bash
export OPENVELA_ROOT=/path/to/openvela   # 工作区根目录，默认 /home/openvela/openvela

# 83 组里的 61 组：voice / chat / cron / focus / api
python3 tests/run_checks.py

# 只跑其中几组
python3 tests/run_checks.py voice cron

# 另外 22 组：UI 两种分辨率各 11 组（首次会编译 LVGL 与 FreeType，较慢）
python3 tests/run_ui_checks.py
```

## 结果在哪

- 逐组输出在标准输出，每组一行 `PASS ...` / `FAIL ...`
- 日志：`tests/out/<name>.log`、`tests/out/renders/ui-<WxH>.log`
- UI 渲染快照：`tests/out/renders/*.png`（无 Pillow 时为 `.ppm`）
- `tests/out/` 已加入 `.gitignore`，不会污染提交

## 边界与诚实说明

- 这些检查跑在 **x86 主机**上，用的是夹具而非真实硬件。它们证明的是**模块逻辑**，
  **不等于真机通过**。真机结论以 [`docs/CURRENT_HARDWARE_STATUS.md`](../docs/CURRENT_HARDWARE_STATUS.md) 为准。
- `tests/include/` 是板端头文件的**最小替身**，只提供被测代码需要的宏与声明。
  定义 `TEST_REAL_AGENT_CONFIG` 时改为直接引用真实的
  `packages/ai_agent/include/agent_config.h`，避免夹具与真实配置漂移。
