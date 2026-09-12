---
name: daily-study-review
description: Summarize today's learning tasks and recent AI activity, then suggest one next action.
---

# Daily Study Review

Use this skill when the user asks for a daily review, progress summary, or what
to study next.

## Inputs

Read these files when available:

- `/data/ai_agent/STUDY_TASKS.md`
- `/data/ai_agent/UI_STATUS.json`
- `/data/ai_agent/LAST_REPLY.txt`

Missing or invalid files are not errors. State which evidence was unavailable
and continue with the remaining data.

## Output

1. Count completed and unfinished tasks.
2. Mention at most two concrete accomplishments.
3. Identify one unfinished task that should be tackled next.
4. Give one small action that can be completed in 10 to 25 minutes.
5. Keep the spoken version under 80 Chinese characters.
6. When the user asks to save the review, write a redacted summary to
   `/data/ai_agent/STUDY_DAILY_SUMMARY.md`.

Never include API keys, tokens, WiFi credentials, authorization headers, or a
full private conversation transcript.
