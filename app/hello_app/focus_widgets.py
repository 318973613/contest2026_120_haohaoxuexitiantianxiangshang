#!/usr/bin/env python3
"""Add the two focus widgets and load the tally at startup.

The length control and the tally readout have code behind them now but no
presence on screen, and load_focus_stats() is never called so a reboot still
shows zero even though the file on disk says otherwise.

The controls go on the home page's timer column, under the arc: that is where
the user already looks to read the countdown, so the length they are about to
commit to and the count of what they have finished belong in the same glance.
The unused FOCUS_SECONDS define goes too -- focus_round_seconds() replaced
every use of it.
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


# --- 1. drop the now-unused constant -------------------------------------
sub(
    """/* Selectable session lengths.  25 minutes stays the default because it is
 * what the previous fixed value used and what most users expect; 15 suits a
 * single exercise set and 45 a longer reading block.
 */
""",
    """/* Selectable session lengths, in g_focus_presets.  25 minutes stays the
 * default because it is what the previous fixed FOCUS_SECONDS used and what
 * most users expect; 15 suits a single exercise set and 45 a longer reading
 * block.  focus_round_seconds() replaced every use of the old constant.
 */
""",
    label="preset comment",
)

# --- 2. the length control, under the arc --------------------------------
sub(
    """  g_ui.focus_button_label = create_label(button, LV_SYMBOL_PLAY,
                                         &lv_font_montserrat_14,
                                         COLOR_WHITE);
  lv_obj_center(g_ui.focus_button_label);
""",
    """  g_ui.focus_button_label = create_label(button, LV_SYMBOL_PLAY,
                                         &lv_font_montserrat_14,
                                         COLOR_WHITE);
  lv_obj_center(g_ui.focus_button_label);

  /* Round length, tappable to cycle 15/25/45.  The length was previously a
   * compile-time constant with no control at all.
   */

  {
    lv_obj_t *length_chip = lv_obj_create(timer_box);

    lv_obj_remove_flag(length_chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(length_chip, 66, 20);
    lv_obj_set_style_radius(length_chip, 10, 0);
    lv_obj_set_style_bg_color(length_chip, color(COLOR_BLUE), 0);
    lv_obj_set_style_bg_opa(length_chip, LV_OPA_20, 0);
    lv_obj_set_style_border_width(length_chip, 0, 0);
    lv_obj_set_style_pad_all(length_chip, 0, 0);
    lv_obj_add_flag(length_chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(length_chip, focus_length_event_cb, LV_EVENT_CLICKED,
                        NULL);

    g_ui.focus_length_label = create_label(length_chip, "25 分钟", ui_font(),
                                           COLOR_BLUE);
    lv_obj_center(g_ui.focus_length_label);
  }
""",
    label="length control widget",
)

# --- 3. today's tally, on the task strip ---------------------------------
sub(
    """  create_label(task_progress, "进度", ui_font(), COLOR_MUTED);
  g_ui.home_task_ratio = create_label(task_progress, "0/0",
                                      &lv_font_montserrat_12, COLOR_BLUE);
  (void)task_icon;""",
    """  create_label(task_progress, "进度", ui_font(), COLOR_MUTED);
  g_ui.home_task_ratio = create_label(task_progress, "0/0",
                                      &lv_font_montserrat_12, COLOR_BLUE);

  /* Today's finished rounds.  Completed sessions were counted nowhere and
   * shown nowhere, so the device could not answer the one question a focus
   * timer exists to answer.
   */

  {
    lv_obj_t *tally = create_group(task_strip, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_size(tally, g_compact_layout ? 92 : 112, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(tally, 2, 0);
    lv_obj_set_flex_align(tally, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);
    create_label(tally, "今日专注", ui_font(), COLOR_MUTED);
    g_ui.focus_stat_value = create_label(tally, "今天还没开始", ui_font(),
                                         COLOR_TEXT);
    lv_label_set_long_mode(g_ui.focus_stat_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_ui.focus_stat_value, LV_PCT(100));
    lv_obj_set_style_text_align(g_ui.focus_stat_value, LV_TEXT_ALIGN_RIGHT, 0);
  }

  refresh_focus_labels();
  (void)task_icon;""",
    label="tally widget",
)

# --- 4. seed the default preset and restore the tally --------------------
# memset() zeroes focus_preset, which is the 15-minute slot, not the 25-minute
# default the old constant used.  load_focus_stats() then overrides both the
# preset and the counters if today's file exists.
sub(
    """  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.focus_elapsed = 0;
  g_ui.on_standby = true;
  g_ui.idle_seconds = 0;
""",
    """  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.focus_elapsed = 0;

  /* memset leaves focus_preset at slot 0 (15 minutes); the default round is
   * 25, so name the slot explicitly rather than relying on table order.
   */

  g_ui.focus_preset = FOCUS_DEFAULT_INDEX;
  g_ui.on_standby = true;
  g_ui.idle_seconds = 0;

#ifndef _WIN32

  /* Restore today's tally and the chosen length.  Without this a reboot showed
   * zero even with the numbers sitting on disk.
   */

  load_focus_stats();
#endif
""",
    label="seed preset and load tally",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: focus widgets added")
