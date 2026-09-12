#!/usr/bin/env python3
"""Show the last real AI answer on the AI page's "最近回复" line.

The widget exists but nothing writes to it, so it sits on its placeholder
forever.  ai_reply_timer_cb() is where a final answer lands, so the line is
filled there -- interim progress notes are skipped, since "稍等，处理中" is not
a reply worth remembering.

The answer is also kept in a buffer so the line survives the page being torn
down and rebuilt: create_ai_page() runs again on layout changes, and without
this the text would silently revert to the placeholder.
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


# --- 1. storage for the last answer --------------------------------------
# Guarded, because only the board has a bridge that produces replies; on
# _WIN32 the variable would sit unused and warn.
sub(
    "static void update_ai_rail(bool network_online);\n",
    """static void update_ai_rail(bool network_online);

#ifndef _WIN32

/* Last final answer, kept so the AI page can restore the line after a
 * rebuild.  Only the head of a long reply fits the one-line label, and that
 * is all this needs to hold.
 */

static char g_last_reply[96];
#endif
""",
    label="g_last_reply storage",
)

# --- 2. record it when a final answer arrives ----------------------------
sub(
    """      got_final = true;

      /* Speak the answer. The bridge queues it on a worker thread, so this
       * returns immediately and the UI keeps redrawing.
       */

      voice_ui_bridge_speak(text);""",
    """      got_final = true;

      /* Remember it for the AI page's "最近回复" line, which previously showed
       * a fixed made-up summary.
       */

      snprintf(g_last_reply, sizeof(g_last_reply), "%s", text);

      if (g_ui.ai_recent_text != NULL)
        {
          lv_label_set_text(g_ui.ai_recent_text, g_last_reply);
        }

      /* Speak the answer. The bridge queues it on a worker thread, so this
       * returns immediately and the UI keeps redrawing.
       */

      voice_ui_bridge_speak(text);""",
    label="record last reply",
)

# --- 3. restore it when the page is (re)built ----------------------------
sub(
    """    g_ui.ai_recent_text = create_label(recent_copy, "还没有对话记录",
                                       ui_font(), COLOR_TEXT);""",
    """#ifdef _WIN32
    const char *recent_seed = "还没有对话记录";
#else
    const char *recent_seed = g_last_reply[0] != '\\0' ?
                              g_last_reply : "还没有对话记录";
#endif

    g_ui.ai_recent_text = create_label(recent_copy, recent_seed,
                                       ui_font(), COLOR_TEXT);""",
    label="restore last reply",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: 最近回复 shows the real last answer")
