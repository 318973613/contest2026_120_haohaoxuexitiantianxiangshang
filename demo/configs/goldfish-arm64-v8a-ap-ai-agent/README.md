# Goldfish ARM64 ai_agent Configuration

This directory contains the reproducible QEMU configuration used to verify
ai_agent without modifying a public openvela repository.

## Source

The `defconfig` file is copied without modification from:

```text
packages/ai_agent/defconfigs/goldfish-arm64-v8a-ap/goldfish-arm64-v8a-ap_defconfig
```

Source branch: `dev-ai-contest-2026`.

Verified options include:

```text
CONFIG_EXAMPLES_AI_AGENT_VELA=y
CONFIG_FEATURE_SYSTEM_VELACLAW=y
```

## Build

Run from the openvela workspace root:

```bash
./build.sh \
  contest2026_120_haohaoxuexitiantianxiangshang/demo/configs/goldfish-arm64-v8a-ap-ai-agent/ \
  --cmake -j4
```

Expected output directory:

```text
cmake_out/demo_goldfish-arm64-v8a-ap-ai-agent
```

## Run

Run from the contest repository root:

```bash
./demo/scripts/start-ai-simulator.sh
```

The script creates or reuses tmux session `openvela-ai-agent`, waits for the
outer `goldfish-armv8a-ap>` prompt, starts `ai_agent`, and returns only after
the `vela>` prompt is available.

## MiMo Configuration

Configure a newly generated key only from an interactive SSH terminal:

```bash
./demo/scripts/configure-mimo.sh
```

The script uses hidden input, transfers the key through a temporary tmux stdin
buffer, deletes that buffer, clears the visible pane and scrollback, and
verifies that `config_show` displays only a masked key.

## Evidence

See `docs/AI_AGENT_RUNTIME.md` and
`demo/runtime-logs/ai-agent-runtime-2026-07-11.md`.

## Hardware Boundary

This configuration is QEMU-only. It must not be treated as the BSP for the
100ASK DShanPixVela-Devkit V1 or used to flash physical hardware.
