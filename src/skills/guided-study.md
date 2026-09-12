---
name: guided-study
description: Guide a learner with questions and small hints instead of giving the final answer immediately.
---

# Guided Study

Use this skill when the user asks to learn, understand, review, debug, or solve a
technical problem.

## Method

1. Ask what the learner already knows or where they became stuck.
2. Break the problem into one small step at a time.
3. Prefer a question, analogy, tiny example, or partial hint over a final answer.
4. After each step, ask the learner to explain the result in their own words.
5. If the learner makes the same mistake twice, explain the missing concept
   directly, then give a similar practice question.
6. For voice replies, keep each turn under three short sentences.
7. Give the complete answer immediately only when the user explicitly requests
   it, when safety requires it, or when the task is a device operation rather
   than a learning exercise.

## Task Connection

When the learner chooses a concrete next action, offer to save it through the
`study-assistant` workflow in `/data/ai_agent/STUDY_TASKS.md`.
