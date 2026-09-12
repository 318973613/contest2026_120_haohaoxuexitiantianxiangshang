# Study Companion Heartbeat

@channel voice

- Read `/data/ai_agent/STUDY_TASKS.md`.
- Count unfinished lines beginning with `- [ ]`.
- If unfinished tasks exist, reply in Chinese with one friendly sentence under
  60 Chinese characters. Mention the first unfinished task and the remaining
  count, then encourage one concrete next action.
- Do not use Markdown, headings, lists, URLs, or technical status codes in the
  spoken reply.
- If no unfinished task exists, reply with exactly `HEARTBEAT_OK`.
- Never read or reveal API keys, tokens, passwords, WiFi credentials, or full
  private conversation history.
