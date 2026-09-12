#!/usr/bin/env python3
"""Stop the volume slider snapping back to its old position.

I added LV_EVENT_RELEASED to play a preview sample, and subscribed the same
callback to both events. That was the bug: the callback rewrites the label and
the file on every invocation, and on RELEASED lv_event_get_target() does not
reliably hand back the slider itself, so the value read there can differ from
where the user actually let go. The stale value then went to disk, and the next
read put the knob back.

Split the two jobs: VALUE_CHANGED owns the value (label + file), RELEASED only
plays the sample. lv_event_get_target_obj() is used for the widget lookup so a
part-level target cannot be mistaken for the slider.
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


# --- 1. the value path stays on VALUE_CHANGED only -----------------------
sub(
    """static void volume_slider_event_cb(lv_event_t *event)
{
  lv_obj_t *slider = lv_event_get_target(event);
  int32_t value = lv_slider_get_value(slider);
  FILE *file;
  char buffer[64];

  note_user_activity();""",
    """static void volume_slider_event_cb(lv_event_t *event)
{
  /* g_ui.volume_slider rather than the event target: on RELEASED the target
   * can be a part of the slider, and lv_slider_get_value() on that does not
   * return where the user let go -- which is how the knob ended up snapping
   * back to the previous position.
   */

  lv_obj_t *slider = g_ui.volume_slider;
  int32_t value;
  FILE *file;
  char buffer[64];

  if (slider == NULL)
    {
      return;
    }

  value = lv_slider_get_value(slider);
  note_user_activity();

  if (lv_event_get_code(event) == LV_EVENT_RELEASED)
    {
      /* Sample only.  The value was already committed by the VALUE_CHANGED
       * pass, and writing it again from here is what corrupted it.
       */

      if (!voice_ui_bridge_is_ready())
        {
          lv_label_set_text_fmt(g_ui.volume_label, "音量  %d%%  (语音未就绪)",
                                (int)value);
        }
      else if (!voice_ui_bridge_is_speaking())
        {
          voice_ui_bridge_speak("音量已调整");
        }

      return;
    }""",
    label="volume value path",
)

# --- 2. drop the old preview block at the end ----------------------------
sub(
    """  /* Let the user hear the level they just chose.  Only on release: this
   * callback also runs for LV_EVENT_VALUE_CHANGED, which fires once per pixel
   * of drag and would queue a synthesis request for each one.
   */

  if (lv_event_get_code(event) == LV_EVENT_RELEASED)
    {
      if (!voice_ui_bridge_is_ready())
        {
          lv_label_set_text_fmt(g_ui.volume_label, "音量  %d%%  (语音未就绪)",
                                (int)value);
        }
      else if (voice_ui_bridge_is_speaking())
        {
          /* Something is already playing; that is the sample. */
        }
      else
        {
          voice_ui_bridge_speak("音量已调整");
        }
    }
}""",
    "}",
    label="remove trailing preview block",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: slider keeps the position the user chose")
