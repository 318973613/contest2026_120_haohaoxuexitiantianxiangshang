#!/usr/bin/env python3
"""Stop hiding tasks past the fourth.

The list container is already LV_DIR_VER scrollable, so the VISIBLE_TASKS cap
bought nothing -- it just made tasks 5 through 20 invisible, and therefore
impossible to tick off or delete, while the counters on the same page kept
counting them.  A task the user can see in the total but cannot reach is worse
than no list at all.

The cap goes; the loop is bounded by the array as it always should have been.
A row counter now feeds a footer line so a long list says how many are below
the fold rather than leaving the user to guess whether scrolling is possible.
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


# --- 1. retire the cap ----------------------------------------------------
sub(
    "#  define VISIBLE_TASKS 4\n",
    """
/* VISIBLE_TASKS used to cap the rendered rows at 4.  The container scrolls, so
 * the cap only served to strand tasks 5..MAX_TASKS where they could be counted
 * but never ticked off or deleted.  Rows are bounded by MAX_TASKS now.
 */
""",
    label="retire VISIBLE_TASKS",
)

sub(
    "  for (index = 0; index < count && index < VISIBLE_TASKS; index++)\n",
    "  for (index = 0; index < count; index++)\n",
    label="uncap row loop",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: all tasks are reachable")
