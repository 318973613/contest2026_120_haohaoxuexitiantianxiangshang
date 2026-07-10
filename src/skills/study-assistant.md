# Study Assistant

Manage learning tasks, review study progress, and schedule rest reminders.

## When to use

Use this Skill when the user asks to:

- add, view, or complete a learning task
- review pending or completed study work
- remind them to rest, drink water, or resume studying later
- check study-assistant files or scheduled reminders

## Data

Store learning tasks in:

```text
/data/ai_agent/STUDY_TASKS.md
```

Each task uses this format:

```text
- [ ] [YYYY-MM-DD HH:MM] task description
- [x] [YYYY-MM-DD HH:MM] completed task description
```

Never write outside `/data/ai_agent/`. Never store passwords, Wi-Fi
credentials, API keys, or tokens in the task file.

## How to use

### Add a learning task

1. Call `get_current_time`.
2. Call `read_file` for `/data/ai_agent/STUDY_TASKS.md`.
3. If the file is missing, create it with the heading `# Study Tasks`.
4. Preserve all existing content and append one unchecked task.
5. Call `write_file` with the complete updated content.
6. Confirm the exact task that was added.

### View learning tasks

1. Call `read_file` for `/data/ai_agent/STUDY_TASKS.md`.
2. Separate pending and completed tasks in the reply.
3. If the file does not exist or contains no tasks, say that clearly.

### Complete a learning task

1. Call `read_file` for `/data/ai_agent/STUDY_TASKS.md`.
2. Find the best exact task-description match.
3. Call `edit_file` to replace only that task's `- [ ]` with `- [x]`.
4. If multiple tasks match, ask the user which one before editing.
5. Confirm the completed task.

### Set a rest reminder

1. Call `get_current_time` and convert the requested delay or time to a UNIX
   timestamp.
2. For a one-time reminder, call `cron_add` with `schedule_type` set to `at`
   and provide `at_epoch` as an integer.
3. For a repeating reminder, call `cron_add` with `schedule_type` set to
   `every` and provide `interval_s` as an integer.
4. Use channel `system` unless the incoming channel requires another value.
5. Confirm the reminder using a human-readable time.

## Examples

User: `添加学习任务：阅读 openvela ai_agent 架构文档`

Use `get_current_time`, `read_file`, and `write_file`, then confirm the task.

User: `提醒我 25 分钟后休息`

Use `get_current_time`, compute the target epoch, then call `cron_add` with
name `study-rest`, `schedule_type` `at`, and message `学习时间到了，休息一下。`.
