#!/usr/bin/env python3
"""The city picker's dialog and settings row.

A roller rather than a grid of buttons: eight cities do not fit across 480 px
without shrinking the touch targets below what a finger can hit reliably, and a
roller keeps one large hit area while staying scrollable if the list grows.

The row shows the city currently in effect, so the setting is legible without
opening the dialog -- the previous state of affairs was that nothing on screen
told you which city the weather belonged to.
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


# --- 1. the dialog -------------------------------------------------------
sub(
    """static void wifi_button_event_cb(lv_event_t *event)
{""",
    """/* Current city for display: the stored value, or the service's default when
 * nothing is set yet.  Kept in step with WEATHER_CITY_DEFAULT in
 * weather_service.c -- if that changes, this string should follow.
 */

static void current_city(char *out, size_t out_size)
{
  if (!agent_config_read("weather.city", out, out_size))
    {
      snprintf(out, out_size, "深圳");
    }
}

static void city_roller_confirm_cb(lv_event_t *event)
{
  lv_obj_t *roller = (lv_obj_t *)lv_event_get_user_data(event);
  lv_obj_t *dialog;
  char chosen[32];

  if (roller == NULL)
    {
      return;
    }

  note_user_activity();
  lv_roller_get_selected_str(roller, chosen, sizeof(chosen));

  if (agent_config_write("weather.city", chosen))
    {
      if (g_ui.city_label != NULL)
        {
          lv_label_set_text(g_ui.city_label, chosen);
        }

      /* The reading on screen belongs to the old city until the agent fetches
       * again, so say so rather than letting a stale temperature look current.
       */

      if (g_ui.standby_weather != NULL)
        {
          lv_label_set_text(g_ui.standby_weather, "正在更新");
        }
    }
  else if (g_ui.city_label != NULL)
    {
      lv_label_set_text(g_ui.city_label, "保存失败");
    }

  /* The roller's parent chain ends at the dialog this callback was built for;
   * deleting the button's own parent would leave the roller behind.
   */

  dialog = lv_obj_get_parent(roller);
  if (dialog != NULL)
    {
      lv_obj_delete(dialog);
    }
}

static void city_dialog_cancel_cb(lv_event_t *event)
{
  lv_obj_t *dialog = (lv_obj_t *)lv_event_get_user_data(event);

  note_user_activity();

  if (dialog != NULL)
    {
      lv_obj_delete(dialog);
    }
}

static void city_button_event_cb(lv_event_t *event)
{
  lv_obj_t *dialog;
  lv_obj_t *title;
  lv_obj_t *roller;
  lv_obj_t *buttons;
  lv_obj_t *cancel;
  lv_obj_t *confirm;
  lv_obj_t *label;
  char options[CITY_CHOICE_COUNT * 12];
  char active[32];
  size_t index;
  size_t used = 0;

  (void)event;
  note_user_activity();

  /* One newline-separated string is what lv_roller wants. */

  options[0] = '\\0';
  for (index = 0; index < CITY_CHOICE_COUNT; index++)
    {
      used += (size_t)snprintf(options + used, sizeof(options) - used,
                               index == 0 ? "%s" : "\\n%s",
                               g_city_choices[index]);
      if (used >= sizeof(options))
        {
          break;
        }
    }

  dialog = lv_obj_create(lv_screen_active());
  lv_obj_set_size(dialog, g_compact_layout ? 300 : 340,
                  g_compact_layout ? 210 : 240);
  lv_obj_center(dialog);
  lv_obj_set_style_bg_color(dialog, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dialog, 12, 0);
  lv_obj_set_style_border_width(dialog, 0, 0);
  lv_obj_set_style_shadow_width(dialog, 0, 0);
  lv_obj_set_style_pad_all(dialog, 12, 0);
  lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(dialog, 8, 0);

  title = create_label(dialog, "选择天气城市", ui_font(), COLOR_TEXT);
  lv_obj_set_width(title, LV_PCT(100));

  roller = lv_roller_create(dialog);
  lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller, 3);
  lv_obj_set_width(roller, LV_PCT(100));
  lv_obj_set_style_text_font(roller, ui_font(), 0);
  lv_obj_set_style_bg_color(roller, color(COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_bg_color(roller, color(COLOR_BLUE), LV_PART_SELECTED);
  lv_obj_set_style_text_color(roller, color(COLOR_WHITE), LV_PART_SELECTED);
  lv_obj_set_style_border_width(roller, 0, 0);

  /* Open on the city already in effect, so confirming without scrolling is a
   * no-op instead of silently switching to whatever sits at the top.
   */

  current_city(active, sizeof(active));
  for (index = 0; index < CITY_CHOICE_COUNT; index++)
    {
      if (strcmp(active, g_city_choices[index]) == 0)
        {
          lv_roller_set_selected(roller, (uint16_t)index, LV_ANIM_OFF);
          break;
        }
    }

  buttons = create_group(dialog, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(buttons, LV_PCT(100));
  lv_obj_set_height(buttons, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(buttons, 8, 0);
  lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  cancel = lv_button_create(buttons);
  lv_obj_set_size(cancel, 84, g_compact_layout ? 30 : 34);
  lv_obj_set_style_radius(cancel, 6, 0);
  lv_obj_set_style_bg_color(cancel, color(COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_add_event_cb(cancel, city_dialog_cancel_cb, LV_EVENT_CLICKED,
                      dialog);
  label = create_label(cancel, "取消", ui_font(), COLOR_TEXT);
  lv_obj_center(label);

  confirm = lv_button_create(buttons);
  lv_obj_set_size(confirm, 84, g_compact_layout ? 30 : 34);
  lv_obj_set_style_radius(confirm, 6, 0);
  lv_obj_set_style_bg_color(confirm, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_width(confirm, 0, 0);
  lv_obj_add_event_cb(confirm, city_roller_confirm_cb, LV_EVENT_CLICKED,
                      roller);
  label = create_label(confirm, "确定", ui_font(), COLOR_WHITE);
  lv_obj_center(label);
}

static void wifi_button_event_cb(lv_event_t *event)
{""",
    label="city dialog",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: city dialog added")
