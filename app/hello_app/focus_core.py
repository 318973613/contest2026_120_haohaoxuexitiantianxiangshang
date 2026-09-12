#!/usr/bin/env python3
"""Make the focus timer configurable and give it a memory.

Two defects it addresses:

  * FOCUS_SECONDS was a fixed 25 minutes with no way to change it, so a user
    who wants a short session or a long one has no route to it.
  * A finished round was announced and then discarded -- nothing counted it,
    nothing wrote it down, so every reboot erased the day's work.  A study
    device whose whole point is focus should be able to say how much you did.

The store follows the STUDY_VOLUME.json pattern already in this file: small
flat JSON, temp file plus rename, hand-rolled field extraction rather than
linking a parser into the UI.  It also keeps a day stamp, so the count resets
on its own when the date rolls over instead of accumulating forever.
"""
import io
import sys

PATH = "study_terminal_main.c"

with io.open(PATH, encoding="utf-8") as f:
    src = f.read()


def sub(old, new, want=1, label=""):
    global src
    n = src.count(old)
    if n != want:
        sys.exit("ABORT: %d occurrences (want %d) of %s" % (n, want, label or old[:60]))
    src = src.replace(old, new)


# --- 1. paths -------------------------------------------------------------
sub(
    '#  define VOLUME_TEMP_FILE "/data/ai_agent/STUDY_VOLUME.json.tmp"\n',
    '#  define VOLUME_TEMP_FILE "/data/ai_agent/STUDY_VOLUME.json.tmp"\n'
    '#  define FOCUS_FILE "/data/ai_agent/STUDY_FOCUS.json"\n'
    '#  define FOCUS_TEMP_FILE "/data/ai_agent/STUDY_FOCUS.json.tmp"\n',
    label="focus file paths",
)

# --- 2. duration presets --------------------------------------------------
sub(
    "#define FOCUS_SECONDS (25 * 60)\n",
    """/* Selectable session lengths.  25 minutes stays the default because it is
 * what the previous fixed value used and what most users expect; 15 suits a
 * single exercise set and 45 a longer reading block.
 */

#define FOCUS_PRESET_COUNT 3
#define FOCUS_DEFAULT_INDEX 1
""",
    label="focus presets",
)

# --- 3. runtime state ----------------------------------------------------
sub(
    """  bool focus_running;
""",
    """  bool focus_running;
  uint8_t focus_preset;
  uint32_t focus_rounds;
  uint32_t focus_minutes;
  lv_obj_t *focus_length_label;
  lv_obj_t *focus_stat_value;
""",
    label="focus state members",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: focus paths, presets and state added")
