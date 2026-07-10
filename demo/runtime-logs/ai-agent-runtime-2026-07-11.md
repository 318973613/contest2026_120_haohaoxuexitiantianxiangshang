# ai_agent Runtime Verification

Date: 2026-07-11

Environment: `goldfish-arm64-v8a-ap` QEMU with ai_agent enabled.

## Security

- The API key was entered with hidden input.
- `config_show` displayed a masked value only.
- No API key, authorization header, or tmux key buffer is included here.
- This file contains selected, sanitized evidence rather than a full terminal
  capture.

## Configuration

| Check | Result |
| --- | --- |
| ai_agent prompt | `vela>` |
| MiMo host | `token-plan-cn.xiaomimimo.com` |
| Model | `mimo-v2.5-pro` |
| `config_show` key display | masked |
| `router_status` | returned expected backend |

## Real MiMo Conversation

Command:

```text
ask Output OPENVELA joined MIMO joined OK underscores
```

Selected runtime evidence:

```text
[vela_tls] Handshake OK: TLSv1.2
[llm] Response: 32 bytes text, 0 tool calls, finish=end_turn
[trace] END status=ok iters=1 tools=0 llm_ms=3358 elapsed=3s backend=0
[Agent]: `OPENVELA_JOINED_MIMO_JOINED_OK`
```

Result: PASS. The provider returned a real response successfully.

## Add Learning Task

Command:

```text
ask 添加学习任务：OPENVELA-STUDY-DEMO-0711阅读ai_agent架构
```

Selected runtime evidence:

```text
Tool call: read_file
Executing tool: get_current_time
Tool call: write_file
write_file: /data/ai_agent/STUDY_TASKS.md
[trace] END status=ok iters=3 tools=3
```

Result: PASS.

## View Learning Tasks

Command:

```text
ask 查看学习任务
```

Selected runtime evidence:

```text
Tool call: read_file args={"path":"/data/ai_agent/STUDY_TASKS.md"}
[trace] END status=ok iters=1 tools=1
[Agent]: # Study Tasks
- [ ] [2026-07-11 01:24] 阅读 ai_agent 架构
```

Result: PASS. The task persisted and was returned from the task file.

## One-Shot Rest Reminder

Command:

```text
ask 提醒我30秒后休息
```

Selected runtime evidence:

```text
Tool call: get_current_time args={}
Tool call: cron_add
[tool_cron] cron_add: OK: Added one-shot job
[Agent]: 已设置，30秒后会提醒你休息。
[cron] Cron job firing: 休息提醒
[Agent]: 30秒到了，该休息一下啦！
[cron] Deleting one-shot: 休息提醒
```

Result: PASS. The reminder was created, fired after approximately 30 seconds,
sent an active notification, and deleted itself.
