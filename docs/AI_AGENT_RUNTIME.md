# ai_agent Runtime Guide

Verified: 2026-07-11

## Scope

This guide reproduces the current QEMU-based ai_agent demo. It does not select
or flash a physical board configuration.

## Environment

- Workspace: `/home/openvela/openvela`
- Contest repository:
  `/home/openvela/openvela/contest2026_120_haohaoxuexitiantianxiangshang`
- Branch: `feat/study-terminal-foundation`
- Build output: `cmake_out/demo_goldfish-arm64-v8a-ap-ai-agent`
- Managed tmux session: `openvela-ai-agent`

The emulator may require this host library path:

```bash
export LD_LIBRARY_PATH="$HOME/.local/libcxxabi/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

`start-ai-simulator.sh` applies that path automatically.

## Build

From the workspace root:

```bash
./build.sh \
  contest2026_120_haohaoxuexitiantianxiangshang/demo/configs/goldfish-arm64-v8a-ap-ai-agent/ \
  --cmake -j4
```

## Start

From the contest repository root:

```bash
./demo/scripts/start-ai-simulator.sh
```

Expected success output ends with:

```text
ai_agent simulator is ready: openvela-ai-agent
```

The script handles these states:

1. No tmux session: start QEMU, detect the outer NSH prompt, run `ai_agent`.
2. Existing `goldfish-armv8a-ap>` prompt: run `ai_agent`.
3. Existing `vela>` prompt: return immediately.

## Configure MiMo Securely

Run only in an interactive SSH terminal:

```bash
./demo/scripts/configure-mimo.sh
```

Enter a newly generated key at the hidden prompt. Do not put the key in a
command, source file, log, screenshot, or chat message.

Verified runtime values:

- Base URL: `https://token-plan-cn.xiaomimimo.com/v1`
- Request path: `/v1/chat/completions`
- Model: `mimo-v2.5-pro`

The script verifies `config_show` and `router_status` without printing the full
key. `config_show` is implemented upstream to show four prefix characters and
`****` only.

## CLI Usage

The `vela>` prompt accepts commands. Chat messages must use `ask`; bare text is
treated as an unknown command.

Examples used during verification:

```text
ask Output OPENVELA joined MIMO joined OK underscores
ask 添加学习任务：OPENVELA-STUDY-DEMO-0711阅读ai_agent架构
ask 查看学习任务
ask 提醒我30秒后休息
```

## Verified Results

- TLS handshake and a real MiMo response completed successfully.
- Task creation called `get_current_time`, `read_file`, and `write_file`.
- Task review read `/data/ai_agent/STUDY_TASKS.md` and returned the pending
  task `阅读 ai_agent 架构`.
- The reminder called `get_current_time` and `cron_add`.
- The one-shot reminder fired after about 30 seconds, sent an active CLI
  notification, and was deleted automatically.

Sanitized evidence is stored in:

```text
demo/runtime-logs/ai-agent-runtime-2026-07-11.md
```

## Known Notes

- QEMU may log an initial `ifup eth0 failed` warning before reporting address
  `10.0.2.15`; the verified run subsequently connected successfully.
- Model wording is nondeterministic. Judge success from the completed LLM
  trace, tool calls, persisted task data, and reminder firing rather than an
  exact response sentence.
- The physical target is the official
  `vendor/allwinnertech/boards/r528/r528s3-dshanpi/` BSP. Its patch, build,
  package, and hardware behavior remain separate from this verified QEMU
  procedure; do not substitute another R528 configuration.
