#!/usr/bin/env python3
"""Replace the AI page's fabricated status rail with real bridge state.

Everything in that rail was a literal: "在线", "模型 v2.5", "工具 36",
"延迟 3.3", "麦克风 开", plus the copy lines "正在聆听" and
"已生成 4 个学习重点".  None of it was ever computed, and two of the numbers
(tool count, latency) are not values this UI has any way to know.

The bridges do expose real state -- agent liveness, request in flight and its
age, voice channel readiness, wake word, TTS activity -- so the rail is rebuilt
around those.  Rows that cannot be backed by anything are dropped rather than
faked: three honest rows beat five invented ones.
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


# --- 1. struct members for the rows that now update ----------------------
sub(
    """  lv_obj_t *ai_connection;
""",
    """  lv_obj_t *ai_connection;
  lv_obj_t *ai_rail_state;
  lv_obj_t *ai_service_value;
  lv_obj_t *ai_voice_value;
  lv_obj_t *ai_request_value;
  lv_obj_t *ai_subtitle;
  lv_obj_t *ai_recent_text;
""",
    label="struct: ai page widgets",
)

# --- 2. the copy lines ---------------------------------------------------
sub(
    """  create_label(voice_copy, "AI 助手", ui_font(), COLOR_BLUE);
  create_label(voice_copy, "正在聆听", ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(voice_copy, "今天复习 ai_agent 架构", ui_font(),
                   COLOR_MUTED);
    }""",
    """  create_label(voice_copy, "AI 助手", ui_font(), COLOR_BLUE);

  /* Was the fixed string "正在聆听", which claimed the mic was open even with
   * the agent down.  Now it names what the bridge is actually doing.
   */

  g_ui.ai_subtitle = create_label(voice_copy, "待唤醒", ui_font(), COLOR_TEXT);
  lv_label_set_long_mode(g_ui.ai_subtitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.ai_subtitle, LV_PCT(100));

  if (!g_compact_layout)
    {
      create_label(voice_copy, "按住麦克风说话", ui_font(), COLOR_MUTED);
    }""",
    label="ai page: subtitle",
)

# --- 3. the rail -------------------------------------------------------
sub(
    """  create_label(rail_header, "在线", ui_font(), COLOR_GREEN);
  create_dark_status_row(rail, LV_SYMBOL_DIRECTORY, "模型", "v2.5",
                         &unused);
  create_dark_status_row(rail, LV_SYMBOL_SETTINGS, "工具", "36", &unused);
  create_dark_status_row(rail, LV_SYMBOL_REFRESH, "延迟", "3.3", &unused);
  create_dark_status_row(rail, LV_SYMBOL_AUDIO,
                         g_compact_layout ? "麦克" : "麦克风", "开",
                         &unused);""",
    """  g_ui.ai_rail_state = create_label(rail_header, "检测中", ui_font(),
                                    COLOR_MUTED);

  /* Three rows the device can actually answer.  The previous "工具 36" and
   * "延迟 3.3" had no source at all -- the UI does not enumerate the agent's
   * tool registry and never measured a round trip -- so they are gone rather
   * than reworded.
   */

  create_dark_status_row(rail, LV_SYMBOL_SETTINGS, "服务", "--",
                         &g_ui.ai_service_value);
  create_dark_status_row(rail, LV_SYMBOL_AUDIO,
                         g_compact_layout ? "语音" : "语音通道", "--",
                         &g_ui.ai_voice_value);
  create_dark_status_row(rail, LV_SYMBOL_REFRESH, "请求", "--",
                         &g_ui.ai_request_value);""",
    label="ai page: rail rows",
)

# --- 4. the "recent reply" line ---------------------------------------
sub(
    """  create_label(recent_copy, "最近回复", ui_font(), COLOR_MUTED);
  {
    lv_obj_t *recent_text = create_label(recent_copy,
                                         g_compact_layout ? "已生成 4 个重点" :
                                         "已生成 4 个学习重点",
                                         ui_font(), COLOR_TEXT);
    lv_label_set_long_mode(recent_text, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(recent_text, LV_PCT(100));
  }""",
    """  create_label(recent_copy, "最近回复", ui_font(), COLOR_MUTED);
  {
    /* Held a made-up summary ("已生成 4 个学习重点").  It now shows the tail of
     * the last real reply, or says there has not been one yet.
     */

    g_ui.ai_recent_text = create_label(recent_copy, "还没有对话记录",
                                       ui_font(), COLOR_TEXT);
    lv_label_set_long_mode(g_ui.ai_recent_text, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_ui.ai_recent_text, LV_PCT(100));
  }""",
    label="ai page: recent reply",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: AI page widgets created, ready to be driven")
