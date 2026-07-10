# Source

This directory contains project-owned source and ai_agent extensions for the
AI study and debugging display terminal.

## Current Content

- `skills/study-assistant.md`: manages learning tasks and one-shot or
  repeating rest reminders.
- `skills/README.md`: documents the runtime Skill installation location.

The Study Assistant stores tasks in:

```text
/data/ai_agent/STUDY_TASKS.md
```

It uses built-in ai_agent tools rather than modifying public openvela code:

- `get_current_time`
- `read_file`
- `write_file`
- `edit_file`
- `cron_add`

Build-facing openvela applications remain under `app/` and are mapped into the
openvela build tree by the contest manifest.

Do not place API keys or device credentials in this directory. Do not place
modifications to public `nuttx`, `packages`, or `vendor` repositories here.
