#!/usr/bin/env python3
"""Drive the AI page rail from real bridge state, and drop the dead variable.

The widgets exist after fix_ai_page.py but nothing writes to them.  This hooks
them into update_system_widgets(), which already runs once a second and has
network_online in hand.

Rows and their sources:
  服务   ai_chat_bridge_agent_running()   -- is the agent process alive
  语音   voice_ui_bridge_is_ready() / wake_is_running() / is_speaking()
  请求   ai_chat_bridge_is_busy() + pending_sec()  -- request in flight and age

On _WIN32 there are no bridges, so the rows say so rather than guessing.
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


# --- 1. the rail's dead variable ------------------------------------------
# Nothing takes its address any more; leaving it would warn under -Wunused.
sub("  lv_obj_t *unused;\n", "", label="drop unused decl")

# --- 2. drive the rail, right after the network block ---------------------
sub(
    """  else
    {
      lv_label_set_text(g_ui.network_value, "离线");
      lv_label_set_text(g_ui.home_network, "离线");
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
    }
""",
    """  else
    {
      lv_label_set_text(g_ui.network_value, "离线");
      lv_label_set_text(g_ui.home_network, "离线");
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
    }

  update_ai_rail(network_online);
""",
    label="call update_ai_rail",
)

# --- 3. the driver itself, ahead of update_system_widgets -----------------
sub(
    """static void update_system_widgets(void)
{""",
    '''/* Fill in the AI page's status rail.
 *
 * Every row here answers a question the user actually has when a reply does
 * not come back: is the service up, can it hear me, and is it working on
 * something right now.  Anything the device cannot measure is left out --
 * this rail used to carry a tool count and a latency figure that were both
 * invented.
 */

static void update_ai_rail(bool network_online)
{
  const char *service_text;
  uint32_t service_color;
  const char *voice_text;
  uint32_t voice_color;
  char request_text[24];
  uint32_t request_color;
  bool agent_up;

#ifdef _WIN32
  /* The simulator has no agent and no audio path, so say that plainly
   * instead of showing states that cannot occur here.
   */

  agent_up = false;
  service_text = "模拟器";
  service_color = COLOR_MUTED;
  voice_text = "不可用";
  voice_color = COLOR_MUTED;
  snprintf(request_text, sizeof(request_text), "--");
  request_color = COLOR_MUTED;
#else
  agent_up = ai_chat_bridge_agent_running();

  if (!agent_up)
    {
      service_text = "未运行";
      service_color = COLOR_CORAL;
    }
  else if (!network_online)
    {
      /* The agent is alive but every model call will fail, which is a
       * different problem from the agent being down and deserves its own
       * wording.
       */

      service_text = "无网络";
      service_color = COLOR_AMBER;
    }
  else
    {
      service_text = "就绪";
      service_color = COLOR_GREEN;
    }

  if (!voice_ui_bridge_is_ready())
    {
      voice_text = "未就绪";
      voice_color = COLOR_MUTED;
    }
  else if (voice_ui_bridge_is_speaking())
    {
      voice_text = "播放中";
      voice_color = COLOR_BLUE;
    }
  else if (voice_ui_bridge_wake_is_running())
    {
      voice_text = "待唤醒";
      voice_color = COLOR_GREEN;
    }
  else
    {
      voice_text = "空闲";
      voice_color = COLOR_TEXT;
    }

  if (ai_chat_bridge_is_busy())
    {
      /* Showing the age of the in-flight request is the one number worth
       * having: it tells a waiting user whether the thing is still thinking
       * or has quietly died.
       */

      snprintf(request_text, sizeof(request_text), "%ds",
               ai_chat_bridge_pending_sec());
      request_color = COLOR_AMBER;
    }
  else
    {
      snprintf(request_text, sizeof(request_text), "空闲");
      request_color = COLOR_TEXT;
    }
#endif

  if (g_ui.ai_service_value != NULL)
    {
      lv_label_set_text(g_ui.ai_service_value, service_text);
      lv_obj_set_style_text_color(g_ui.ai_service_value,
                                  color(service_color), 0);
    }

  if (g_ui.ai_voice_value != NULL)
    {
      lv_label_set_text(g_ui.ai_voice_value, voice_text);
      lv_obj_set_style_text_color(g_ui.ai_voice_value, color(voice_color), 0);
    }

  if (g_ui.ai_request_value != NULL)
    {
      lv_label_set_text(g_ui.ai_request_value, request_text);
      lv_obj_set_style_text_color(g_ui.ai_request_value,
                                  color(request_color), 0);
    }

  /* The rail header lamp mirrors the service row, since that is the one that
   * decides whether anything on this page works at all.
   */

  if (g_ui.ai_rail_state != NULL)
    {
      lv_label_set_text(g_ui.ai_rail_state, service_text);
      lv_obj_set_style_text_color(g_ui.ai_rail_state, color(service_color), 0);
    }

  if (g_ui.ai_subtitle != NULL)
    {
      const char *subtitle;

#ifdef _WIN32
      subtitle = "预览模式";
#else
      if (!agent_up)
        {
          subtitle = "AI 服务未运行";
        }
      else if (ai_chat_bridge_is_busy())
        {
          subtitle = "正在思考";
        }
      else if (voice_ui_bridge_is_speaking())
        {
          subtitle = "正在回答";
        }
      else if (voice_ui_bridge_wake_is_running())
        {
          subtitle = "说\\"你好小米\\"唤醒";
        }
      else
        {
          subtitle = "点击麦克风提问";
        }
#endif

      lv_label_set_text(g_ui.ai_subtitle, subtitle);
    }
}

static void update_system_widgets(void)
{''',
    label="update_ai_rail definition",
)

# --- 4. forward declaration ---------------------------------------------
sub(
    "static void update_standby_widgets(void);\n",
    "static void update_standby_widgets(void);\nstatic void update_ai_rail(bool network_online);\n",
    label="forward decl",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: AI rail driven from bridge state")
