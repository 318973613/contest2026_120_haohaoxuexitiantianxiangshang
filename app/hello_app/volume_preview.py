#!/usr/bin/env python3
"""Let the volume slider be heard, not just seen.

The slider wrote the gain and audio_playback.c applied it on the next
playback -- correct, but unusable: the user drags to 30%, hears nothing, and
has no way to judge the level until the assistant happens to speak. Every
device with a volume control offers a test tone for exactly this reason.

Release-only, because LV_EVENT_VALUE_CHANGED fires continuously while dragging
and would queue a synthesis request per pixel. The label and the file still
update live so the number tracks the finger; only the sample waits for release.
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


# --- 1. play a sample once the finger lifts ------------------------------
sub(
    """  if (rename(VOLUME_TEMP_FILE, VOLUME_FILE) != 0)
    {
      unlink(VOLUME_TEMP_FILE);
    }
}""",
    """  if (rename(VOLUME_TEMP_FILE, VOLUME_FILE) != 0)
    {
      unlink(VOLUME_TEMP_FILE);
    }

  /* Let the user hear the level they just chose.  Only on release: this
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
    label="volume preview",
)

# --- 2. subscribe to the release event -----------------------------------
sub(
    """    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,""",
    """    /* RELEASED as well as VALUE_CHANGED: the first plays the sample, the
     * second keeps the label tracking the finger.
     */

    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,""",
    label="release subscription",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: volume slider plays a sample on release")
