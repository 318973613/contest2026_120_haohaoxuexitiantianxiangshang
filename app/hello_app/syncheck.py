#!/usr/bin/env python3
"""Cheap structural check on the patched UI source.

Not a compiler, but it catches the two things a bad patch actually produces:
an unbalanced brace from a botched anchor, and a function that is referenced
but never defined (or defined and never used, which usually means a patch
landed inside the wrong #ifdef arm).
"""
import io
import re
import sys

src = io.open("study_terminal_main.c", encoding="utf-8").read()

s = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
s = re.sub(r"//[^\n]*", "", s)
s = re.sub(r'"(\\.|[^"\\])*"', '""', s)
s = re.sub(r"'(\\.|[^'\\])*'", "''", s)

print("brace balance :", s.count("{") - s.count("}"))
print("paren balance :", s.count("(") - s.count(")"))

names = [
    "refresh_focus_labels",
    "focus_length_event_cb",
    "load_focus_stats",
    "save_focus_stats",
    "focus_round_seconds",
    "focus_round_minutes",
    "focus_day_stamp",
    "focus_json_field",
    "update_ai_rail",
    "first_pending_task",
    "weather_field",
    "clock_is_synced",
]

print()
for n in names:
    has_def = re.search(r"^static [^;=]*\b" + n + r"\s*\(", s, re.M) is not None
    has_macro = re.search(r"^#\s*define\s+" + n + r"\b", s, re.M) is not None
    uses = len(re.findall(r"\b" + n + r"\s*\(", s))
    decls = len(re.findall(r"^static [^;=]*\b" + n + r"\s*\([^;{]*\);", s, re.M))
    flag = ""
    if not (has_def or has_macro):
        flag = "  <== NO DEFINITION"
    elif uses - decls <= 1:
        flag = "  <== defined but never called"
    print("%-22s def=%-5s macro=%-5s refs=%-3d decls=%d%s"
          % (n, has_def, has_macro, uses, decls, flag))

# Anything still referring to the retired constant?
left = len(re.findall(r"\bFOCUS_SECONDS\b", s))
print("\nFOCUS_SECONDS refs:", left)
