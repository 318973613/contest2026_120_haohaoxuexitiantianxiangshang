#!/usr/bin/env python3
"""Make the header's network indicator tell the truth.

The header showed a green WiFi glyph and the word "在线" as literals, so the
top of every screen claimed the device was online even with the radio down --
while the home page's own rail, two centimetres below, correctly said 离线 from
read_network_address().  Two indicators disagreeing is worse than one being
absent, and the one that lies is the one users see on every page.

Both now follow the same signal the rail uses, on the same one-second tick.
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


# --- 1. slots for the two header widgets ---------------------------------
sub(
    """  lv_obj_t *header_clock;
""",
    """  lv_obj_t *header_clock;
  lv_obj_t *header_net_icon;
  lv_obj_t *header_net_text;
""",
    label="header net slots",
)

# --- 2. keep the handles instead of discarding them ----------------------
sub(
    """  create_label(right, LV_SYMBOL_WIFI, &lv_font_montserrat_14, COLOR_GREEN);
  create_label(right, "在线", ui_font(), COLOR_GREEN);""",
    """  /* Were fixed green literals that claimed "在线" regardless of the radio.
   * update_system_widgets() drives both from read_network_address() now, so
   * the header agrees with the home page's network row.
   */

  g_ui.header_net_icon = create_label(right, LV_SYMBOL_WIFI,
                                      &lv_font_montserrat_14, COLOR_MUTED);
  g_ui.header_net_text = create_label(right, "检测中", ui_font(), COLOR_MUTED);""",
    label="header net widgets",
)

# --- 3. drive them from the real signal ---------------------------------
sub(
    """  update_ai_rail(network_online);""",
    """  if (g_ui.header_net_icon != NULL)
    {
      uint32_t net_color = network_online ? COLOR_GREEN : COLOR_CORAL;

      lv_obj_set_style_text_color(g_ui.header_net_icon, color(net_color), 0);
      lv_obj_set_style_text_color(g_ui.header_net_text, color(net_color), 0);
      lv_label_set_text(g_ui.header_net_text,
                        network_online ? "在线" : "离线");
    }

  update_ai_rail(network_online);""",
    label="drive header net",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: header network indicator follows the real link state")
