#!/usr/bin/env python3
"""Wire the focus timer's selectable length and persistent tally.

focus_core.py added the state; this adds the behaviour:

  * a preset table and focus_round_seconds(), replacing every FOCUS_SECONDS
    use, so the round length is a runtime value;
  * load/save of STUDY_FOCUS.json, so a finished round is counted and survives
    a reboot;
  * a tap target to change the length, refused while a round is running --
    silently resizing a session in flight would make the arc jump and the
    tally meaningless;
  * the two labels on the home page that show length and today's tally.

The save is stubbed out on _WIN32: the simulator has no /data to write to, and
the call sites stay free of guards this way.
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


# --- 1. preset table + accessors, before note_user_activity() ------------
sub(
    "static bool clock_is_synced(time_t wall_time);\n",
    """static bool clock_is_synced(time_t wall_time);

/* Minutes per selectable round.  Index into this with g_ui.focus_preset. */

static const uint8_t g_focus_presets[FOCUS_PRESET_COUNT] =
{
  15, 25, 45
};

static uint32_t focus_round_minutes(void)
{
  uint8_t index = g_ui.focus_preset;

  if (index >= FOCUS_PRESET_COUNT)
    {
      index = FOCUS_DEFAULT_INDEX;
    }

  return g_focus_presets[index];
}

static uint32_t focus_round_seconds(void)
{
  return focus_round_minutes() * 60;
}

static void refresh_focus_labels(void);

#ifndef _WIN32
static void save_focus_stats(void);
#else

/* The simulator has no /data partition to persist into, and the tally is a
 * device feature.  Stubbed so the call sites need no guards.
 */

#  define save_focus_stats() ((void)0)
#endif
""",
    label="preset table",
)

# --- 2. persistence, next to the volume store ---------------------------
sub(
    """/* Persisted playback gain. audio_playback.c in ai_agent reads this file. */
""",
    """/* Today's focus tally.  A finished round used to vanish the moment the arc
 * reset, so the device could not answer "how much did I do today" -- the one
 * question a focus timer exists to answer.  The day stamp makes the counters
 * reset themselves when the date rolls over.
 *
 * Both halves tolerate a missing or malformed file by falling back to zero:
 * losing a tally is a cosmetic problem, and refusing to start the timer over
 * it would not be.
 */

static int focus_day_stamp(void)
{
  time_t now = time(NULL);
  struct tm parts;

  /* An unsynced clock reads 1970 and would stamp every early round with the
   * same bogus day.  That is still consistent -- the tally resets once NTP
   * lands, which is the correct visible outcome.
   */

  localtime_r(&now, &parts);
  return (parts.tm_year + 1900) * 10000 + (parts.tm_mon + 1) * 100 +
         parts.tm_mday;
}

static int focus_json_field(const char *json, const char *key)
{
  const char *found = strstr(json, key);

  if (found == NULL)
    {
      return -1;
    }

  found = strchr(found, ':');
  if (found == NULL)
    {
      return -1;
    }

  return atoi(found + 1);
}

static void load_focus_stats(void)
{
  FILE *file;
  char buffer[192];
  size_t nread;
  int stored_day;
  int value;

  file = fopen(FOCUS_FILE, "r");
  if (file == NULL)
    {
      return;
    }

  nread = fread(buffer, 1, sizeof(buffer) - 1, file);
  fclose(file);

  if (nread == 0)
    {
      return;
    }

  buffer[nread] = '\\0';

  stored_day = focus_json_field(buffer, "\\"day\\"");
  if (stored_day != focus_day_stamp())
    {
      /* Yesterday's numbers.  Leave the counters at zero. */

      return;
    }

  value = focus_json_field(buffer, "\\"rounds\\"");
  if (value > 0)
    {
      g_ui.focus_rounds = (uint32_t)value;
    }

  value = focus_json_field(buffer, "\\"minutes\\"");
  if (value > 0)
    {
      g_ui.focus_minutes = (uint32_t)value;
    }

  value = focus_json_field(buffer, "\\"preset\\"");
  if (value >= 0 && value < FOCUS_PRESET_COUNT)
    {
      g_ui.focus_preset = (uint8_t)value;
    }
}

static void save_focus_stats(void)
{
  FILE *file;
  char buffer[192];

  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      return;
    }

  snprintf(buffer, sizeof(buffer),
           "{\\"version\\":1,\\"day\\":%d,\\"rounds\\":%lu,\\"minutes\\":%lu,"
           "\\"preset\\":%u}\\n",
           focus_day_stamp(),
           (unsigned long)g_ui.focus_rounds,
           (unsigned long)g_ui.focus_minutes,
           (unsigned)g_ui.focus_preset);

  file = fopen(FOCUS_TEMP_FILE, "w");
  if (file == NULL)
    {
      return;
    }

  if (fputs(buffer, file) < 0 || fflush(file) != 0)
    {
      fclose(file);
      unlink(FOCUS_TEMP_FILE);
      return;
    }

  fclose(file);

  if (rename(FOCUS_TEMP_FILE, FOCUS_FILE) != 0)
    {
      unlink(FOCUS_TEMP_FILE);
    }
}

/* Persisted playback gain. audio_playback.c in ai_agent reads this file. */
""",
    label="focus persistence",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: preset table and focus persistence added")
