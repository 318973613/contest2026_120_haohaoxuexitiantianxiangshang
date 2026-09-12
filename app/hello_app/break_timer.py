#!/usr/bin/env python3
"""Add the rest half of the pomodoro cycle.

The timer counted focus and then said "请稍作休息" -- advice with nothing behind
it.  The user does not know how long to rest, gets no signal to come back, and
in practice either skips the break or loses the thread entirely.  A focus timer
that only times focus is half a feature.

A completed round now rolls straight into a 5-minute break on the same arc,
tinted teal so the mode is legible at a glance rather than only readable in the
status line.  When the break ends the arc returns to the focus length and the
device says so out loud if TTS is up -- the user is by definition not looking at
the screen during a break.
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


# --- 1. break length + state ---------------------------------------------
sub(
    "#define FOCUS_PRESET_COUNT 3\n",
    """#define FOCUS_PRESET_COUNT 3

/* Break length.  Fixed at 5 minutes rather than scaled to the round: the
 * point of the break is to be short enough that the user comes back, and a
 * 45-minute round does not warrant a 9-minute one.
 */

#define BREAK_SECONDS (5 * 60)
""",
    label="break length",
)

sub(
    """  uint8_t focus_preset;
""",
    """  uint8_t focus_preset;

  /* True while the arc is counting a break instead of a focus round. */

  bool on_break;
""",
    label="break state",
)

# --- 2. the tick: run the break on the same arc --------------------------
sub(
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
    """  if (g_ui.focus_running)
    {
      uint32_t round_seconds = g_ui.on_break ? BREAK_SECONDS :
                                               focus_round_seconds();

      g_ui.focus_elapsed++;
      if (g_ui.focus_elapsed >= round_seconds)
        {
          g_ui.focus_elapsed = 0;

          if (g_ui.on_break)
            {
              /* Break over.  Back to a focus round, still paused so returning
               * to work stays the user's decision.
               */

              g_ui.on_break = false;
              g_ui.focus_running = false;
              lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE),
                                         LV_PART_INDICATOR);
              lv_label_set_text(g_ui.focus_status, "休息结束，继续下一轮");
              lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
              stop_focus_pulse();

              /* Said aloud because the user is, by design, not watching the
               * screen during a break.
               */

              if (voice_ui_bridge_is_ready() &&
                  !voice_ui_bridge_is_speaking())
                {
                  voice_ui_bridge_speak("休息结束，开始下一轮专注");
                }
            }
          else
            {
              /* Credit the round.  This used to be the point where a completed
               * session was silently thrown away.
               */

              g_ui.focus_rounds++;
              g_ui.focus_minutes += focus_round_minutes();
              save_focus_stats();
              refresh_focus_labels();

              /* Roll into the break automatically.  Leaving it to a second tap
               * is how breaks get skipped.
               */

              g_ui.on_break = true;
              lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_TEAL),
                                         LV_PART_INDICATOR);
              lv_label_set_text(g_ui.focus_status, "休息 5 分钟，起来活动一下");

              if (voice_ui_bridge_is_ready() &&
                  !voice_ui_bridge_is_speaking())
                {
                  voice_ui_bridge_speak("本轮专注完成，休息五分钟");
                }
            }
        }
      else
        {
          progress = g_ui.focus_elapsed * 100 / round_seconds;
          lv_arc_set_value(g_ui.focus_bar, (int32_t)progress);
        }
    }

  remaining = (g_ui.on_break ? BREAK_SECONDS : focus_round_seconds()) -
              g_ui.focus_elapsed;""",
    label="break on the arc",
)

# --- 3. let the user skip a break ----------------------------------------
# The play/pause button gains a third job: during a break, pressing it ends the
# break rather than pausing it. Sitting through a countdown you did not ask for
# is not a rest, and having no way out but waiting is worse.
#
# The skip lives in the paused branch: focus_event_cb toggles focus_running, so
# a press during a running break arrives here with focus_running already false.
sub(
    """      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
      lv_label_set_text(g_ui.focus_status, "已暂停");
      stop_focus_pulse();""",
    """      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
      stop_focus_pulse();

      if (g_ui.on_break)
        {
          /* End the break instead of pausing it.  A rest you are forced to sit
           * through with no way out is not a rest.
           */

          g_ui.on_break = false;
          g_ui.focus_elapsed = 0;
          lv_arc_set_value(g_ui.focus_bar, 0);
          lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE),
                                     LV_PART_INDICATOR);
          lv_label_set_text(g_ui.focus_status, "已跳过休息");
        }
      else
        {
          lv_label_set_text(g_ui.focus_status, "已暂停");
        }""",
    label="skip break",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: breaks run on the focus arc, and can be skipped")
