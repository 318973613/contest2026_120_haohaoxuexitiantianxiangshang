#!/usr/bin/env python3
"""Replace the fixed round length at its use sites and surface the tally.

Three things left after focus_core.py and focus_impl.py:

  * the tick still divides by the FOCUS_SECONDS constant, so the arc and the
    countdown ignore the chosen length;
  * a completed round still evaporates -- nothing increments the tally;
  * the user has no control to change the length and nowhere to read the tally.

The length control sits on the timer column next to the arc, and refuses while
a round is running: resizing a session in flight would make the arc jump
backwards and put the recorded minutes out of step with what was actually done.
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


# --- 1. the tick: honour the chosen length, count the finished round ------
sub(
    """  if (g_ui.focus_running)
    {
      g_ui.focus_elapsed++;
      if (g_ui.focus_elapsed >= FOCUS_SECONDS)
        {
          g_ui.focus_elapsed = FOCUS_SECONDS;
          g_ui.focus_running = false;
          lv_label_set_text(g_ui.focus_status, "本轮专注完成，请稍作休息");
          lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
          stop_focus_pulse();
        }

      progress = g_ui.focus_elapsed * 100 / FOCUS_SECONDS;
      lv_arc_set_value(g_ui.focus_bar, (int32_t)progress);
    }

  remaining = FOCUS_SECONDS - g_ui.focus_elapsed;""",
    """  if (g_ui.focus_running)
    {
      uint32_t round_seconds = focus_round_seconds();

      g_ui.focus_elapsed++;
      if (g_ui.focus_elapsed >= round_seconds)
        {
          g_ui.focus_elapsed = round_seconds;
          g_ui.focus_running = false;

          /* Credit the round.  This used to be the point where a completed
           * session was silently thrown away.
           */

          g_ui.focus_rounds++;
          g_ui.focus_minutes += focus_round_minutes();
          save_focus_stats();
          refresh_focus_labels();

          lv_label_set_text(g_ui.focus_status, "本轮专注完成，请稍作休息");
          lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
          stop_focus_pulse();
        }

      progress = g_ui.focus_elapsed * 100 / round_seconds;
      lv_arc_set_value(g_ui.focus_bar, (int32_t)progress);
    }

  remaining = focus_round_seconds() - g_ui.focus_elapsed;""",
    label="tick: round length and tally",
)

# --- 2. label refresh + the length control -------------------------------
sub(
    """static void focus_event_cb(lv_event_t *event)
{""",
    """/* Keep the length and tally labels in step with the state.  Called after a
 * round completes, after the length changes, and once at startup.
 */

static void refresh_focus_labels(void)
{
  if (g_ui.focus_length_label != NULL)
    {
      lv_label_set_text_fmt(g_ui.focus_length_label, "%lu 分钟",
                            (unsigned long)focus_round_minutes());
    }

  if (g_ui.focus_stat_value != NULL)
    {
      if (g_ui.focus_rounds == 0)
        {
          lv_label_set_text(g_ui.focus_stat_value, "今天还没开始");
        }
      else
        {
          lv_label_set_text_fmt(g_ui.focus_stat_value, "%lu 轮 / %lu 分钟",
                                (unsigned long)g_ui.focus_rounds,
                                (unsigned long)g_ui.focus_minutes);
        }
    }
}

/* Cycle the round length.  Refused mid-session: shrinking the round under a
 * running timer makes the arc jump backwards, and the minutes credited at the
 * end would no longer match what the user actually sat through.
 */

static void focus_length_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();

  if (g_ui.focus_running)
    {
      lv_label_set_text(g_ui.focus_status, "专注中不能改时长");
      return;
    }

  g_ui.focus_preset = (uint8_t)((g_ui.focus_preset + 1) % FOCUS_PRESET_COUNT);
  g_ui.focus_elapsed = 0;
  lv_arc_set_value(g_ui.focus_bar, 0);
  refresh_focus_labels();
  save_focus_stats();

  lv_label_set_text_fmt(g_ui.focus_status, "已设为 %lu 分钟",
                        (unsigned long)focus_round_minutes());
}

static void focus_event_cb(lv_event_t *event)
{""",
    label="refresh + length control",
)

# --- 3. restart a finished round instead of re-crediting it ---------------
# focus_elapsed was never cleared anywhere.  After a round completed it stayed
# at the full length, so the next press of play satisfied the completion test
# on the very first tick -- crediting another round for one second of work.
sub(
    """  g_ui.focus_running = !g_ui.focus_running;

  if (g_ui.focus_running)
    {
      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PAUSE);""",
    """  g_ui.focus_running = !g_ui.focus_running;

  if (g_ui.focus_running)
    {
      /* Starting from a finished round begins a new one.  Without this the
       * elapsed count is still at the full length and the next tick would
       * immediately "complete" the round again, crediting the tally for a
       * second of work.
       */

      if (g_ui.focus_elapsed >= focus_round_seconds())
        {
          g_ui.focus_elapsed = 0;
          lv_arc_set_value(g_ui.focus_bar, 0);
        }

      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PAUSE);""",
    label="restart finished round",
)

# --- 4. start at zero, not at a decorative 18% ---------------------------
# The arc was seeded 18% full purely so the boot screen looked busy, which
# quietly stole 4.5 minutes from the first session the user ran.
sub(
    "  g_ui.focus_elapsed = FOCUS_SECONDS * 18 / 100;\n",
    """  g_ui.focus_elapsed = 0;
""",
    label="no fake initial progress",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: round length honoured, tally credited, control added")
