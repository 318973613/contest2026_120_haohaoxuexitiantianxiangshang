#!/usr/bin/env python3
"""Wire the city picker into the settings page and drop the last fake weather.

create_action_row's button says "打开" and it returns the button, not a label,
so the current city could not be shown through it. The row is built directly
here instead: the value belongs on screen, since the whole point is knowing
which city the temperature refers to without opening a dialog.

Also clears the two remaining fabricated literals in create_standby_screen --
"晴转多云" and "26C" were the seed values, so an unconfigured device flashed a
plausible forecast for one second on every wake before the timer replaced it
with the honest "天气未配置".
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


# --- 1. honest seed values for the standby weather -----------------------
sub(
    '  g_ui.standby_weather = create_label(weather_row, "晴转多云", ui_font(),',
    '  g_ui.standby_weather = create_label(weather_row, "天气未配置", ui_font(),',
    label="standby weather seed",
)

sub(
    '  g_ui.standby_temp = create_label(weather_row, "26C",',
    '  g_ui.standby_temp = create_label(weather_row, "--",',
    label="standby temp seed",
)

# --- 2. the settings row -------------------------------------------------
CITY_ROW = '''  /* Weather city.  Built inline rather than through create_action_row()
   * because that helper hard-codes "打开" on its button and returns the
   * button, leaving nowhere to show the value -- and the value is the point:
   * without it nothing on screen says which city the temperature is for.
   */

  {
    lv_obj_t *city_row = create_group(panel, LV_FLEX_FLOW_ROW);
    lv_obj_t *city_text = create_group(city_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *city_button;
    lv_obj_t *city_button_label;
    char city_now[32];

    lv_obj_set_size(city_row, LV_PCT(100), g_compact_layout ? 34 : 70);
    lv_obj_set_style_border_color(city_row, color(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(city_row, 1, 0);
    lv_obj_set_style_border_side(city_row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_column(city_row, g_compact_layout ? 8 : 12, 0);
    lv_obj_set_flex_align(city_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_width(city_text, 1);
    lv_obj_set_flex_grow(city_text, 1);
    lv_obj_set_height(city_text, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(city_text, g_compact_layout ? 0 : 2, 0);
    lv_obj_set_flex_align(city_text, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    create_label(city_text, "天气城市", ui_font(), COLOR_TEXT);

    current_city(city_now, sizeof(city_now));
    g_ui.city_label = create_label(city_text, city_now, ui_font(),
                                   COLOR_MUTED);

    city_button = lv_button_create(city_row);
    lv_obj_set_size(city_button, g_compact_layout ? 60 : 84,
                    g_compact_layout ? 24 : 32);
    lv_obj_set_style_radius(city_button, 6, 0);
    lv_obj_set_style_bg_color(city_button, color(COLOR_TEAL), 0);
    lv_obj_set_style_shadow_width(city_button, 0, 0);
    lv_obj_set_style_pad_all(city_button, 0, 0);
    lv_obj_add_event_cb(city_button, city_button_event_cb, LV_EVENT_CLICKED,
                        NULL);
    city_button_label = create_label(city_button, "更改", ui_font(),
                                     COLOR_WHITE);
    lv_obj_center(city_button_label);
  }

'''

ANCHOR = '  g_ui.wifi_button = create_action_row(panel, "WiFi 配置",'

sub(ANCHOR, CITY_ROW + ANCHOR, label="city settings row")

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: city row in settings, standby seeds honest")
