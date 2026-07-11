/****************************************************************************
 * Contest 2026 team 120 - AI study terminal LVGL demo
 ****************************************************************************/

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <malloc.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/boardctl.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>
#if LV_USE_FREETYPE
#  include <lvgl/src/libs/freetype/lv_freetype.h>
#endif

extern const lv_font_t lv_font_simsun_16_cjk;

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#  include <uv.h>
#endif

#define COLOR_BG          0xf3f6ff
#define COLOR_BG_BOTTOM   0xe9efff
#define COLOR_SURFACE     0xffffff
#define COLOR_SURFACE_ALT 0xf0f4ff
#define COLOR_BORDER      0xdce5f7
#define COLOR_TEXT        0x17213d
#define COLOR_MUTED       0x71809e
#define COLOR_TEAL        0x45c6ad
#define COLOR_BLUE        0x4f7df3
#define COLOR_PURPLE      0x8468f5
#define COLOR_AMBER       0xffa24d
#define COLOR_CORAL       0xee7b9f
#define COLOR_GREEN       0x55bd91
#define COLOR_WHITE       0xffffff

#define TASK_FILE "/data/ai_agent/STUDY_TASKS.md"
#define UI_FONT_FILE "/data/NotoSansSC-Regular.ttf"
#define FOCUS_SECONDS (25 * 60)

struct study_ui_s
{
  lv_obj_t *screen;
  lv_obj_t *tabview;
  lv_obj_t *header_clock;
  lv_obj_t *home_task_count;
  lv_obj_t *home_network;
  lv_obj_t *focus_status;
  lv_obj_t *focus_button_label;
  lv_obj_t *focus_bar;
  lv_obj_t *cpu_value;
  lv_obj_t *memory_value;
  lv_obj_t *network_value;
  lv_obj_t *uptime_value;
  lv_obj_t *task_pending_value;
  lv_obj_t *task_completed_value;
  lv_obj_t *task_file_status;
  lv_obj_t *task_sync_label;
  lv_obj_t *ai_connection;
  lv_obj_t *ai_check_label;
  bool focus_running;
  uint32_t focus_elapsed;
};

static struct study_ui_s g_ui;
static uint32_t g_initial_tab;
static lv_font_t *g_dynamic_cjk_font;

static lv_color_t color(uint32_t value)
{
  return lv_color_hex(value);
}

static const lv_font_t *ui_font(void)
{
  if (g_dynamic_cjk_font != NULL)
    {
      return g_dynamic_cjk_font;
    }

  return &lv_font_simsun_16_cjk;
}

static void initialize_ui_font(void)
{
#if LV_USE_FREETYPE
  lv_freetype_init(128);
  g_dynamic_cjk_font = lv_freetype_font_create(
    UI_FONT_FILE, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 18,
    LV_FREETYPE_FONT_STYLE_NORMAL);
#endif
}

static void object_reset(lv_obj_t *obj)
{
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *create_group(lv_obj_t *parent, lv_flex_flow_t flow)
{
  lv_obj_t *group = lv_obj_create(parent);

  object_reset(group);
  lv_obj_set_layout(group, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(group, flow);
  return group;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t text_color)
{
  lv_obj_t *label = lv_label_create(parent);

  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color(text_color), 0);
  return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int32_t height)
{
  lv_obj_t *panel = lv_obj_create(parent);

  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(panel, LV_PCT(100));
  lv_obj_set_height(panel, height);
  lv_obj_set_style_bg_color(panel, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_shadow_color(panel, color(0x9fb4de), 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_10, 0);
  lv_obj_set_style_shadow_width(panel, 14, 0);
  lv_obj_set_style_shadow_ofs_y(panel, 4, 0);
  return panel;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, uint32_t bg_color,
                                      const char *text,
                                      lv_event_cb_t callback)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label;

  lv_obj_set_height(button, 46);
  lv_obj_set_width(button, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(button, color(bg_color), 0);
  lv_obj_set_style_bg_color(button, color(COLOR_SURFACE_ALT), LV_STATE_PRESSED);
  lv_obj_set_style_radius(button, 8, 0);
  lv_obj_set_style_pad_left(button, 18, 0);
  lv_obj_set_style_pad_right(button, 18, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);

  label = create_label(button, text, ui_font(), COLOR_WHITE);
  lv_obj_center(label);
  return label;
}

static lv_obj_t *create_metric_card(lv_obj_t *parent, uint32_t accent,
                                    const char *eyebrow, const char *value,
                                    const char *detail,
                                    lv_obj_t **value_label)
{
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_t *marker;

  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(card, 1);
  lv_obj_set_height(card, LV_PCT(100));
  lv_obj_set_flex_grow(card, 1);
  lv_obj_set_style_bg_color(card, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_set_style_shadow_color(card, color(0xa4b8dd), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_width(card, 12, 0);
  lv_obj_set_style_shadow_ofs_y(card, 3, 0);

  marker = lv_obj_create(card);
  object_reset(marker);
  lv_obj_set_size(marker, 34, 4);
  lv_obj_set_style_bg_color(marker, color(accent), 0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(marker, 2, 0);

  create_label(card, eyebrow, ui_font(), COLOR_MUTED);
  *value_label = create_label(card, value, ui_font(), COLOR_TEXT);
  create_label(card, detail, ui_font(), COLOR_MUTED);
  return card;
}

static bool read_network_address(char *buffer, size_t buffer_size)
{
  struct ifaddrs *interfaces = NULL;
  struct ifaddrs *item;
  bool found = false;

  if (getifaddrs(&interfaces) != 0)
    {
      return false;
    }

  for (item = interfaces; item != NULL; item = item->ifa_next)
    {
      struct sockaddr_in *address;
      char ip[INET_ADDRSTRLEN];

      if (item->ifa_addr == NULL || item->ifa_addr->sa_family != AF_INET ||
          (item->ifa_flags & IFF_LOOPBACK) != 0)
        {
          continue;
        }

      address = (struct sockaddr_in *)item->ifa_addr;
      if (inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip)) == NULL)
        {
          continue;
        }

      snprintf(buffer, buffer_size, "%s  %s", item->ifa_name, ip);
      found = true;
      break;
    }

  freeifaddrs(interfaces);
  return found;
}

static void read_task_counts(int *pending, int *completed)
{
  FILE *file;
  char line[256];

  *pending = 0;
  *completed = 0;
  file = fopen(TASK_FILE, "r");
  if (file == NULL)
    {
      return;
    }

  while (fgets(line, sizeof(line), file) != NULL)
    {
      if (strncmp(line, "- [ ]", 5) == 0)
        {
          (*pending)++;
        }
      else if (strncmp(line, "- [x]", 5) == 0 ||
               strncmp(line, "- [X]", 5) == 0)
        {
          (*completed)++;
        }
    }

  fclose(file);
}

static void update_task_widgets(void)
{
  int pending;
  int completed;

  read_task_counts(&pending, &completed);
  lv_label_set_text_fmt(g_ui.task_pending_value, "%d", pending);
  lv_label_set_text_fmt(g_ui.task_completed_value, "%d", completed);
  lv_label_set_text_fmt(g_ui.home_task_count, "%d 项待办", pending);

  if (pending == 0 && completed == 0)
    {
      lv_label_set_text(g_ui.task_file_status, "暂时没有已保存任务");
    }
  else
    {
      lv_label_set_text_fmt(g_ui.task_file_status, "共 %d 项任务",
                            pending + completed);
    }
}

static void format_uptime(char *buffer, size_t buffer_size, time_t seconds)
{
  long hours = (long)(seconds / 3600);
  long minutes = (long)((seconds % 3600) / 60);
  long secs = (long)(seconds % 60);

  snprintf(buffer, buffer_size, "%02ld:%02ld:%02ld", hours, minutes, secs);
}

static void update_system_widgets(void)
{
  struct timespec uptime;
  struct mallinfo memory;
  char uptime_text[32];
  char memory_text[32];
  char network_text[64];
  char clock_text[16];
  time_t wall_time;
  struct tm local_time;
  long free_kb;

  clock_gettime(CLOCK_MONOTONIC, &uptime);
  format_uptime(uptime_text, sizeof(uptime_text), uptime.tv_sec);
  lv_label_set_text(g_ui.uptime_value, uptime_text);

  memory = mallinfo();
  free_kb = memory.fordblks / 1024;
  if (free_kb >= 1024)
    {
      snprintf(memory_text, sizeof(memory_text), "%ld MB", free_kb / 1024);
    }
  else
    {
      snprintf(memory_text, sizeof(memory_text), "%ld KB", free_kb);
    }
  lv_label_set_text(g_ui.memory_value, memory_text);

  if (read_network_address(network_text, sizeof(network_text)))
    {
      lv_label_set_text(g_ui.network_value, network_text);
      lv_label_set_text(g_ui.home_network, "已连接");
      lv_label_set_text(g_ui.ai_connection, "网络正常，MiMo 已就绪");
    }
  else
    {
      lv_label_set_text(g_ui.network_value, "离线");
      lv_label_set_text(g_ui.home_network, "离线");
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
    }

  wall_time = time(NULL);
  localtime_r(&wall_time, &local_time);
  strftime(clock_text, sizeof(clock_text), "%H:%M", &local_time);
  lv_label_set_text(g_ui.header_clock, clock_text);
}

static void focus_event_cb(lv_event_t *event)
{
  (void)event;
  g_ui.focus_running = !g_ui.focus_running;

  if (g_ui.focus_running)
    {
      lv_label_set_text(g_ui.focus_button_label, "暂停");
      lv_label_set_text(g_ui.focus_status, "专注进行中，请保持当前任务");
    }
  else
    {
      lv_label_set_text(g_ui.focus_button_label, "继续");
      lv_label_set_text(g_ui.focus_status, "已暂停，进度已经保留");
    }
}

static void task_sync_event_cb(lv_event_t *event)
{
  (void)event;
  update_task_widgets();
  lv_label_set_text(g_ui.task_sync_label, "已同步");
}

static void ai_check_event_cb(lv_event_t *event)
{
  char network_text[64];

  (void)event;
  if (read_network_address(network_text, sizeof(network_text)))
    {
      lv_label_set_text(g_ui.ai_connection, "网络正常，MiMo 已就绪");
      lv_label_set_text(g_ui.ai_check_label, "在线");
    }
  else
    {
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
      lv_label_set_text(g_ui.ai_check_label, "重试");
    }
}

static void update_timer_cb(lv_timer_t *timer)
{
  static uint8_t task_update_tick;
  uint32_t progress;

  (void)timer;
  update_system_widgets();

  if (g_ui.focus_running)
    {
      g_ui.focus_elapsed++;
      if (g_ui.focus_elapsed >= FOCUS_SECONDS)
        {
          g_ui.focus_elapsed = FOCUS_SECONDS;
          g_ui.focus_running = false;
          lv_label_set_text(g_ui.focus_status, "本轮专注完成，请稍作休息");
          lv_label_set_text(g_ui.focus_button_label,
                            "再来一轮");
        }

      progress = g_ui.focus_elapsed * 100 / FOCUS_SECONDS;
      lv_bar_set_value(g_ui.focus_bar, (int32_t)progress, LV_ANIM_ON);
    }

  task_update_tick++;
  if (task_update_tick >= 5)
    {
      task_update_tick = 0;
      update_task_widgets();
    }
}

static void configure_page(lv_obj_t *page)
{
  lv_obj_set_style_bg_color(page, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 20, 0);
  lv_obj_set_style_pad_row(page, 14, 0);
  lv_obj_set_layout(page, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
}

static void create_header(lv_obj_t *parent)
{
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_t *left;
  lv_obj_t *right;
  lv_obj_t *dot;

  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, 74);
  lv_obj_set_style_bg_color(header, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(header, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_left(header, 24, 0);
  lv_obj_set_style_pad_right(header, 24, 0);
  lv_obj_set_style_pad_top(header, 12, 0);
  lv_obj_set_style_pad_bottom(header, 12, 0);
  lv_obj_set_layout(header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  left = create_group(header, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(left, LV_PCT(70), LV_PCT(100));
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(left, "AI 智能学习与调试终端", ui_font(), COLOR_TEXT);
  create_label(left, "openvela + ai_agent", &lv_font_montserrat_12,
               COLOR_MUTED);

  right = create_group(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_height(right, 36);
  lv_obj_set_width(right, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(right, color(COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(right, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(right, 1, 0);
  lv_obj_set_style_radius(right, 6, 0);
  lv_obj_set_style_pad_left(right, 12, 0);
  lv_obj_set_style_pad_right(right, 12, 0);
  lv_obj_set_style_pad_column(right, 9, 0);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  dot = lv_obj_create(right);
  object_reset(dot);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_bg_color(dot, color(COLOR_GREEN), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dot, 4, 0);
  create_label(right, "AI 在线", ui_font(), COLOR_TEXT);
  g_ui.header_clock = create_label(right, "--:--", &lv_font_montserrat_12,
                                   COLOR_MUTED);
}

static void create_home_page(lv_obj_t *page)
{
  lv_obj_t *hero;
  lv_obj_t *hero_top;
  lv_obj_t *metrics;
  lv_obj_t *button_row;
  lv_obj_t *unused;

  configure_page(page);

  hero = create_panel(page, 236);
  lv_obj_set_layout(hero, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(hero, 12, 0);

  hero_top = create_group(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(hero_top, LV_PCT(100));
  lv_obj_set_height(hero_top, 30);
  lv_obj_set_flex_align(hero_top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(hero_top, "今日重点", ui_font(), COLOR_BLUE);
  create_label(hero_top, "25 分钟专注", ui_font(), COLOR_MUTED);

  create_label(hero, "阅读 ai_agent 架构", ui_font(), COLOR_TEXT);
  g_ui.focus_status = create_label(hero, "准备就绪，一次完成一件事",
                                   ui_font(), COLOR_MUTED);

  g_ui.focus_bar = lv_bar_create(hero);
  lv_obj_set_size(g_ui.focus_bar, LV_PCT(100), 8);
  lv_bar_set_range(g_ui.focus_bar, 0, 100);
  lv_bar_set_value(g_ui.focus_bar, 18, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_ui.focus_bar, color(COLOR_SURFACE_ALT),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_ui.focus_bar, color(COLOR_TEAL),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_ui.focus_bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(g_ui.focus_bar, 4, LV_PART_INDICATOR);

  button_row = create_group(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_flex_grow(button_row, 1);
  lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
  g_ui.focus_button_label = create_action_button(button_row, COLOR_TEAL,
                                                 "开始专注",
                                                 focus_event_cb);

  metrics = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(metrics, LV_PCT(100), 128);
  lv_obj_set_style_pad_column(metrics, 14, 0);
  create_metric_card(metrics, COLOR_AMBER, "休息提醒", "25 min",
                     "主动提醒已就绪", &unused);
  create_metric_card(metrics, COLOR_BLUE, "学习任务", "0 项待办",
                     "来自 ai_agent", &g_ui.home_task_count);
  create_metric_card(metrics, COLOR_GREEN, "网络状态", "检测中",
                     "QEMU eth0", &g_ui.home_network);

  hero = create_panel(page, 76);
  lv_obj_set_layout(hero, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(hero, "最新事件", ui_font(), COLOR_MUTED);
  create_label(hero, "休息提醒已经成功送达", ui_font(), COLOR_TEXT);
  create_label(hero, "提醒正常", ui_font(), COLOR_GREEN);
}

static void create_status_page(lv_obj_t *page)
{
  lv_obj_t *metrics;
  lv_obj_t *panel;
  lv_obj_t *row;
  lv_obj_t *value;

  configure_page(page);
  create_label(page, "系统状态", ui_font(), COLOR_TEXT);
  create_label(page, "实时设备运行信息", ui_font(), COLOR_MUTED);

  metrics = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(metrics, LV_PCT(100), 170);
  lv_obj_set_style_pad_column(metrics, 14, 0);
  create_metric_card(metrics, COLOR_CORAL, "CPU", "ARM64 / 1 核",
                     "Goldfish 虚拟处理器", &g_ui.cpu_value);
  create_metric_card(metrics, COLOR_BLUE, "可用内存", "-- MB",
                     "实时空闲堆", &g_ui.memory_value);
  create_metric_card(metrics, COLOR_GREEN, "网络", "检测中",
                     "活动 IPv4 接口", &g_ui.network_value);
  create_metric_card(metrics, COLOR_AMBER, "运行时间", "00:00:00",
                     "系统持续运行", &g_ui.uptime_value);

  panel = create_panel(page, 190);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 0, 0);
  create_label(panel, "运行组件", ui_font(), COLOR_TEXT);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "openvela 内核", ui_font(), COLOR_MUTED);
  value = create_label(row, "运行中", ui_font(), COLOR_GREEN);
  lv_obj_set_width(value, 90);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "ai_agent", &lv_font_montserrat_14, COLOR_MUTED);
  value = create_label(row, "已就绪", ui_font(), COLOR_TEAL);
  lv_obj_set_width(value, 90);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "显示后端", ui_font(), COLOR_MUTED);
  value = create_label(row, "LVGL 9.2", &lv_font_montserrat_12, COLOR_BLUE);
  lv_obj_set_width(value, 90);
}

static void create_task_row(lv_obj_t *parent, bool completed,
                            const char *title, const char *detail)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *marker;
  lv_obj_t *text;

  lv_obj_set_size(row, LV_PCT(100), 64);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_left(row, 2, 0);
  lv_obj_set_style_pad_right(row, 2, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  marker = lv_obj_create(row);
  object_reset(marker);
  lv_obj_set_size(marker, 18, 18);
  lv_obj_set_style_bg_color(marker,
                            color(completed ? COLOR_GREEN : COLOR_SURFACE_ALT),
                            0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(marker,
                                color(completed ? COLOR_GREEN : COLOR_MUTED), 0);
  lv_obj_set_style_border_width(marker, 2, 0);
  lv_obj_set_style_radius(marker, 4, 0);

  text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_height(text, LV_PCT(100));
  lv_obj_set_width(text, 1);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(text, title, ui_font(),
               completed ? COLOR_MUTED : COLOR_TEXT);
  create_label(text, detail, ui_font(), COLOR_MUTED);
}

static void create_tasks_page(lv_obj_t *page)
{
  lv_obj_t *summary;
  lv_obj_t *list;
  lv_obj_t *button_row;
  lv_obj_t *unused;

  configure_page(page);
  create_label(page, "学习任务", ui_font(), COLOR_TEXT);
  create_label(page, "由 Study Assistant Skill 保存", ui_font(), COLOR_MUTED);

  summary = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(summary, LV_PCT(100), 128);
  lv_obj_set_style_pad_column(summary, 14, 0);
  create_metric_card(summary, COLOR_AMBER, "待完成", "0",
                     "尚未勾选", &g_ui.task_pending_value);
  create_metric_card(summary, COLOR_GREEN, "已完成", "0",
                     "已经勾选", &g_ui.task_completed_value);
  create_metric_card(summary, COLOR_BLUE, "提醒", "可用",
                     "主动提醒已验证", &unused);

  list = create_panel(page, 260);
  lv_obj_set_layout(list, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 0, 0);
  create_task_row(list, false, "完成传感器测试",
                  "保存在 STUDY_TASKS.md");
  create_task_row(list, true, "验证 QEMU 与 MiMo",
                  "TLS 和模型回复测试通过");
  create_task_row(list, true, "测试主动休息提醒",
                  "30 秒单次提醒测试通过");

  button_row = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_height(button_row, 48);
  lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  g_ui.task_file_status = create_label(button_row, "正在读取任务文件",
                                       ui_font(), COLOR_MUTED);
  g_ui.task_sync_label = create_action_button(button_row, COLOR_BLUE,
                                              "同步",
                                              task_sync_event_cb);
}

static void create_ai_page(lv_obj_t *page)
{
  lv_obj_t *provider;
  lv_obj_t *provider_top;
  lv_obj_t *stats;
  lv_obj_t *button_row;
  lv_obj_t *unused;

  configure_page(page);
  create_label(page, "AI 助手", ui_font(), COLOR_TEXT);
  create_label(page, "模型、工具与 Skill 运行状态", ui_font(), COLOR_MUTED);

  provider = create_panel(page, 230);
  lv_obj_set_layout(provider, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(provider, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(provider, 12, 0);

  provider_top = create_group(provider, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(provider_top, LV_PCT(100));
  lv_obj_set_height(provider_top, 34);
  lv_obj_set_flex_align(provider_top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(provider_top, "XIAOMI MIMO", &lv_font_montserrat_12,
               COLOR_TEAL);
  create_label(provider_top, "模型通道 0", ui_font(), COLOR_MUTED);

  create_label(provider, "mimo-v2.5-pro", &lv_font_montserrat_32, COLOR_TEXT);
  create_label(provider, "token-plan-cn.xiaomimimo.com",
               &lv_font_montserrat_14, COLOR_MUTED);
  g_ui.ai_connection = create_label(provider, "正在检查连接",
                                    ui_font(), COLOR_GREEN);
  create_label(provider, "最近一次验证回复：3.3 秒", ui_font(), COLOR_MUTED);

  stats = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(stats, LV_PCT(100), 128);
  lv_obj_set_style_pad_column(stats, 14, 0);
  create_metric_card(stats, COLOR_BLUE, "工具", "36",
                     "内置工具注册表", &unused);
  create_metric_card(stats, COLOR_AMBER, "SKILLS", "11",
                     "含 Study Assistant", &unused);
  create_metric_card(stats, COLOR_GREEN, "最近测试", "通过",
                     "模型与提醒验证", &unused);

  button_row = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_height(button_row, 50);
  lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  g_ui.ai_check_label = create_action_button(button_row, COLOR_GREEN,
                                             "检查连接",
                                             ai_check_event_cb);
}

static void create_setting_row(lv_obj_t *parent, const char *title,
                               const char *detail, bool enabled)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *toggle;

  lv_obj_set_size(row, LV_PCT(100), 70);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_width(text, LV_PCT(78));
  lv_obj_set_height(text, LV_PCT(100));
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(text, title, ui_font(), COLOR_TEXT);
  create_label(text, detail, ui_font(), COLOR_MUTED);

  toggle = lv_switch_create(row);
  lv_obj_set_size(toggle, 52, 28);
  lv_obj_set_style_bg_color(toggle, color(COLOR_SURFACE_ALT), LV_PART_MAIN);
  lv_obj_set_style_bg_color(toggle, color(COLOR_BLUE),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (enabled)
    {
      lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }
}

static void create_settings_page(lv_obj_t *page)
{
  lv_obj_t *panel;
  lv_obj_t *info;

  configure_page(page);
  create_label(page, "设置", ui_font(), COLOR_TEXT);
  create_label(page, "调整终端的常用行为", ui_font(), COLOR_MUTED);

  panel = create_panel(page, 310);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 0, 0);
  create_setting_row(panel, "主动提醒", "允许学习与休息提醒", true);
  create_setting_row(panel, "状态自动刷新", "每秒更新设备信息", true);
  create_setting_row(panel, "页面动画", "使用简短切换动画", false);
  create_setting_row(panel, "调试信息", "显示开发阶段诊断内容", false);

  info = create_panel(page, 104);
  lv_obj_set_layout(info, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(info, 8, 0);
  create_label(info, "设备信息", ui_font(), COLOR_TEXT);
  create_label(info, "openvela AI 学习终端 · QEMU 演示版", ui_font(),
               COLOR_MUTED);
}

static void create_ui(void)
{
  lv_obj_t *tabview;
  lv_obj_t *tabbar;
  lv_obj_t *home;
  lv_obj_t *status;
  lv_obj_t *tasks;
  lv_obj_t *ai;
  lv_obj_t *settings;

  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.focus_elapsed = FOCUS_SECONDS * 18 / 100;

  g_ui.screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_ui.screen, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(g_ui.screen, &lv_font_montserrat_16, 0);
  lv_obj_set_layout(g_ui.screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_ui.screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(g_ui.screen, 0, 0);
  lv_obj_set_style_pad_row(g_ui.screen, 0, 0);

  create_header(g_ui.screen);

  tabview = lv_tabview_create(g_ui.screen);
  lv_obj_set_width(tabview, LV_PCT(100));
  lv_obj_set_height(tabview, 1);
  lv_obj_set_flex_grow(tabview, 1);
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tabview, 62);
  lv_obj_set_style_bg_color(tabview, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tabview, 0, 0);

  tabbar = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_style_bg_color(tabbar, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(tabbar, color(COLOR_MUTED), 0);
  lv_obj_set_style_text_font(tabbar, ui_font(), 0);
  lv_obj_set_style_text_color(tabbar, color(COLOR_BLUE),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabbar, color(COLOR_SURFACE_ALT),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER,
                          LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabbar, color(COLOR_TEAL),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabbar, 3,
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tabbar, LV_BORDER_SIDE_TOP,
                               LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabbar, 0, 0);

  home = lv_tabview_add_tab(tabview, "首页");
  status = lv_tabview_add_tab(tabview, "状态");
  tasks = lv_tabview_add_tab(tabview, "任务");
  ai = lv_tabview_add_tab(tabview, "AI");
  settings = lv_tabview_add_tab(tabview, "设置");

  create_home_page(home);
  create_status_page(status);
  create_tasks_page(tasks);
  create_ai_page(ai);
  create_settings_page(settings);

  lv_tabview_set_active(tabview, g_initial_tab, LV_ANIM_OFF);

  update_task_widgets();
  update_system_widgets();
  lv_timer_create(update_timer_cb, 1000, NULL);
  lv_screen_load(g_ui.screen);
}

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void run_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t info;
  void *data;

  uv_loop_init(loop);
  memset(&info, 0, sizeof(info));
  info.loop = loop;
  info.disp = result->disp;
  info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  info.uindev = result->utouch_indev;
#endif
#ifdef CONFIG_LV_USE_NUTTX_MOUSE
  info.mouse_indev = result->mouse_indev;
#endif

  data = lv_nuttx_uv_init(&info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}
#endif

int main(int argc, char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  uv_loop_t ui_loop;
  memset(&ui_loop, 0, sizeof(ui_loop));
#endif

  g_initial_tab = 0;
  if (argc > 1)
    {
      if (strcmp(argv[1], "status") == 0)
        {
          g_initial_tab = 1;
        }
      else if (strcmp(argv[1], "tasks") == 0)
        {
          g_initial_tab = 2;
        }
      else if (strcmp(argv[1], "ai") == 0)
        {
          g_initial_tab = 3;
        }
      else if (strcmp(argv[1], "settings") == 0)
        {
          g_initial_tab = 4;
        }
    }

  if (lv_is_initialized())
    {
      fprintf(stderr, "study_terminal: LVGL is already initialized\n");
      return 1;
    }

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
  boardctl(BOARDIOC_INIT, 0);
#endif

  lv_init();
  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      fprintf(stderr, "study_terminal: display initialization failed\n");
      lv_deinit();
      return 1;
    }

  initialize_ui_font();
  create_ui();

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  run_uv_loop(&ui_loop, &result);
#else
  while (true)
    {
      uint32_t idle = lv_timer_handler();
      usleep((idle == 0 ? 1 : idle) * 1000);
    }
#endif

  lv_nuttx_deinit(&result);
  lv_deinit();
  return 0;
}
