#!/usr/bin/env python3
"""Pick the weather city on screen instead of over a serial console.

Configuring the city required a USB-serial cable and a shell command, which is
not a thing a study terminal's user is going to do. The key still needs the
console -- 32 hex characters is a bad thing to enter by touch, and it is
genuinely a one-time setup step -- but the city is the part that actually
changes (moving, travelling) and it is a short list, so it can be a tap.

The UI writes weather.city into the same config store the agent reads, so a
tap here reaches weather_service.c on its next cycle with no restart. That is
the same file-and-store handoff already used for tasks and volume.
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


# --- 1. the city table + config path -------------------------------------
sub(
    '#  define WEATHER_FILE "/data/ai_agent/WEATHER.json"\n',
    """#  define WEATHER_FILE "/data/ai_agent/WEATHER.json"

/* The agent's own config store.  Writing weather.city here is what makes the
 * on-screen picker reach weather_service.c -- it re-reads the key every cycle,
 * so no restart is needed.
 */

#  define AGENT_CONFIG_DIR "/data/ai_agent/config"
#  define AGENT_CONFIG_PATH AGENT_CONFIG_DIR "/config.json"
""",
    label="config path",
)

# --- 2. widget slot ------------------------------------------------------
sub(
    """  lv_obj_t *wifi_button;
""",
    """  lv_obj_t *wifi_button;
  lv_obj_t *city_label;
""",
    label="city widget slot",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: city picker scaffolding added")
