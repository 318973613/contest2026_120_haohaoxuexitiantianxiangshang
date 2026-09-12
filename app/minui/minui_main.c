/****************************************************************************
 * Study Terminal V3 - 参考 Windows 版设计的完整 UI
 *
 * 基线来源: minui_main.c.BASELINE_B_GOOD (镜像 B, 颜色正常且不崩溃)
 *
 * 从 B 继承的两条硬约束, 不要违反:
 *   1. 所有背景一律 LV_OPA_COVER。RGB565 下多层半透明叠加会明显色偏,
 *      这正是镜像 A 颜色不对、B 改成不透明后就正常的原因。
 *   2. 不用 LV_SYMBOL_*。那些是 FontAwesome 码点, 在本板上会触发
 *      alpha-blend 绘制任务并暴露 G2D 中断里的信号量竞争。全用 ASCII。
 *
 * 配色与布局取自 _staging/lvgl-ui-redesign-win/app/study_terminal_main.c,
 * 文案暂时全英文。
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <lvgl/lvgl.h>
#include <lvgl/src/drivers/nuttx/lv_nuttx_entry.h>

#ifdef CONFIG_BOARDCTL
#include <sys/boardctl.h>
#endif

/****************************************************************************
 * 配色 - 与 Windows 版 study_terminal_main.c 一致
 ****************************************************************************/

#define COLOR_BG          0xdceff3
#define COLOR_SURFACE     0xffffff
#define COLOR_SURFACE_ALT 0xe7f1f4
#define COLOR_TEXT        0x14212b
#define COLOR_MUTED       0x61727d
#define COLOR_TEAL        0x0f9f8f
#define COLOR_BLUE        0x246bfd
#define COLOR_PURPLE      0x7158d9
#define COLOR_AMBER       0xeaa126
#define COLOR_CORAL       0xdc5d67
#define COLOR_GREEN       0x209765
#define COLOR_WHITE       0xffffff
#define COLOR_DARK        0x0a1b2f
#define COLOR_DARK_ALT    0x17314f
#define COLOR_ARC_TRACK   0xd3deea
#define COLOR_TABBAR_TXT  0x9eb0c2
#define COLOR_TABBAR_SEL  0x75a0ff
#define COLOR_BORDER_SOFT 0xc9dde4

/* 面板边框用浅色描边代替阴影 —— 阴影在 RGB565 上就是一圈脏边 */
#define COLOR_PANEL_EDGE  0xeaf3f6

/* ASCII 图标: 不用 LV_SYMBOL_*, 见文件头说明 */
#define ICON_HOME     "H"
#define ICON_STATUS   "="
#define ICON_TASKS    "L"
#define ICON_AI       "A"
#define ICON_SETTINGS "S"
#define ICON_PLAY     ">"
#define ICON_PAUSE    "||"
#define ICON_CPU      "C"
#define ICON_MEM      "M"
#define ICON_NET      "W"
#define ICON_TIME     "T"

#define FOCUS_TOTAL_SEC (25 * 60)

/****************************************************************************
 * UI 状态
 ****************************************************************************/

struct study_ui_s
{
  lv_obj_t *screen;
  lv_obj_t *tabview;
  lv_obj_t *header_clock;

  /* Home */
  lv_obj_t *focus_time;
  lv_obj_t *focus_arc;
  lv_obj_t *focus_status;
  lv_obj_t *focus_button_label;
  lv_obj_t *home_cpu;
  lv_obj_t *home_memory;
  lv_obj_t *home_network;
  lv_obj_t *home_uptime;
  lv_obj_t *home_task_count;

  /* Status */
  lv_obj_t *status_cpu;
  lv_obj_t *status_cpu_bar;
  lv_obj_t *status_memory;
  lv_obj_t *status_memory_bar;
  lv_obj_t *status_uptime;

  /* Tasks */
  lv_obj_t *task_pending;
  lv_obj_t *task_done;
  lv_obj_t *task_bar;

  /* AI */
  lv_obj_t *ai_state;

  bool     focus_running;
  uint32_t focus_elapsed;
  uint32_t tick;
};

static struct study_ui_s g_ui;
static bool g_compact;

#ifdef CONFIG_FT5X06_SWAPXY
static lv_indev_read_cb_t g_touch_read_cb;
#endif

/****************************************************************************
 * 小工具
 ****************************************************************************/

static lv_color_t color(uint32_t hex_value)
{
  return lv_color_hex(hex_value);
}

static const lv_font_t *font_sm(void)
{
  return &lv_font_montserrat_12;
}

static const lv_font_t *font_md(void)
{
  return &lv_font_montserrat_14;
}

static const lv_font_t *font_lg(void)
{
  return &lv_font_montserrat_16;
}

static const lv_font_t *font_xl(void)
{
  return &lv_font_montserrat_20;
}

/* 去掉一个容器的所有装饰, 只留布局能力 */
static void object_reset(lv_obj_t *obj)
{
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_pad_row(obj, 0, 0);
  lv_obj_set_style_pad_column(obj, 0, 0);
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

/* 白卡片。背景不透明, 用描边代替阴影 */
static lv_obj_t *create_panel(lv_obj_t *parent, int32_t height)
{
  lv_obj_t *panel = lv_obj_create(parent);

  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(panel, LV_PCT(100));
  lv_obj_set_height(panel, height);
  lv_obj_set_style_bg_color(panel, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, color(COLOR_PANEL_EDGE), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, g_compact ? 8 : 14, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);

  return panel;
}

/* 深色面板, 用于 Home 右侧状态栏 */
static lv_obj_t *create_dark_panel(lv_obj_t *parent)
{
  lv_obj_t *panel = lv_obj_create(parent);

  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(panel, color(COLOR_DARK), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, g_compact ? 7 : 10, 0);
  lv_obj_set_style_pad_row(panel, 3, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

  return panel;
}

/* 彩色小方块图标, 代替 LV_SYMBOL 徽标 */
static lv_obj_t *create_icon_badge(lv_obj_t *parent, const char *text,
                                   uint32_t accent, int32_t size)
{
  lv_obj_t *badge = lv_obj_create(parent);
  lv_obj_t *label;

  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(badge, size, size);
  lv_obj_set_style_bg_color(badge, color(accent), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(badge, 0, 0);
  lv_obj_set_style_radius(badge, 6, 0);
  lv_obj_set_style_pad_all(badge, 0, 0);
  lv_obj_set_style_shadow_width(badge, 0, 0);

  label = create_label(badge, text, font_sm(), COLOR_WHITE);
  lv_obj_center(label);

  return badge;
}

/* 深色面板里的一行: [图标] 名称 ........ 值 */
static void create_dark_status_row(lv_obj_t *parent, const char *icon,
                                   const char *name, const char *value,
                                   uint32_t accent, lv_obj_t **value_label)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *dot;

  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, g_compact ? 18 : 22);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  dot = lv_obj_create(row);
  object_reset(dot);
  lv_obj_set_size(dot, 4, 4);
  lv_obj_set_style_bg_color(dot, color(accent), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dot, 2, 0);

  create_label(row, name, font_sm(), COLOR_TABBAR_TXT);
  (void)icon;

  /* 弹性占位, 把值推到右边 */
  {
    lv_obj_t *spacer = lv_obj_create(row);
    object_reset(spacer);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
  }

  *value_label = create_label(row, value, font_sm(), COLOR_WHITE);
}

/* 顶部有色条的指标卡, 横向等分 */
static lv_obj_t *create_metric_card(lv_obj_t *parent, uint32_t accent,
                                    const char *eyebrow, const char *value,
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
  lv_obj_set_style_border_color(card, color(COLOR_PANEL_EDGE), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, g_compact ? 6 : 10, 0);
  lv_obj_set_style_pad_row(card, 4, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

  marker = lv_obj_create(card);
  object_reset(marker);
  lv_obj_set_size(marker, 22, 3);
  lv_obj_set_style_bg_color(marker, color(accent), 0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(marker, 2, 0);

  create_label(card, eyebrow, font_sm(), COLOR_MUTED);
  *value_label = create_label(card, value, font_lg(), COLOR_TEXT);
  lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(*value_label, LV_PCT(100));

  return card;
}

/* 细进度条 */
static lv_obj_t *create_thin_bar(lv_obj_t *parent, uint32_t accent)
{
  lv_obj_t *bar = lv_bar_create(parent);

  lv_obj_set_width(bar, LV_PCT(100));
  lv_obj_set_height(bar, 6);
  lv_obj_set_style_radius(bar, 3, 0);
  lv_obj_set_style_bg_color(bar, color(COLOR_ARC_TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, color(accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);

  return bar;
}

/* 页标题: [彩块] 标题 + 副标题 */
static void create_page_heading(lv_obj_t *page, const char *icon,
                                uint32_t accent, const char *title,
                                const char *subtitle)
{
  lv_obj_t *head = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_t *copy;

  lv_obj_set_width(head, LV_PCT(100));
  lv_obj_set_height(head, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(head, 8, 0);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  create_icon_badge(head, icon, accent, g_compact ? 22 : 26);

  copy = create_group(head, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(copy, 1);
  lv_obj_set_height(copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(copy, 1);
  lv_obj_set_style_pad_row(copy, 1, 0);

  create_label(copy, title, font_md(), COLOR_TEXT);
  if (!g_compact)
    {
      create_label(copy, subtitle, font_sm(), COLOR_MUTED);
    }
}

/* 页面容器统一配置 */
static void configure_page(lv_obj_t *page)
{
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_layout(page, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(page, g_compact ? 8 : 12, 0);
  lv_obj_set_style_pad_row(page, g_compact ? 6 : 10, 0);
}

/****************************************************************************
 * 事件回调
 ****************************************************************************/

static void focus_event_cb(lv_event_t *e)
{
  LV_UNUSED(e);

  g_ui.focus_running = !g_ui.focus_running;

  if (g_ui.focus_button_label != NULL)
    {
      lv_label_set_text(g_ui.focus_button_label,
                        g_ui.focus_running ? ICON_PAUSE : ICON_PLAY);
    }

  if (g_ui.focus_status != NULL)
    {
      lv_label_set_text(g_ui.focus_status,
                        g_ui.focus_running ? "Focusing" : "Paused");
      lv_obj_set_style_text_color(g_ui.focus_status,
                                  color(g_ui.focus_running
                                        ? COLOR_GREEN : COLOR_MUTED), 0);
    }
}

static void focus_reset_event_cb(lv_event_t *e)
{
  LV_UNUSED(e);

  g_ui.focus_running = false;
  g_ui.focus_elapsed = 0;

  if (g_ui.focus_time != NULL)
    {
      lv_label_set_text(g_ui.focus_time, "25:00");
    }

  if (g_ui.focus_arc != NULL)
    {
      lv_arc_set_value(g_ui.focus_arc, 0);
    }

  if (g_ui.focus_button_label != NULL)
    {
      lv_label_set_text(g_ui.focus_button_label, ICON_PLAY);
    }

  if (g_ui.focus_status != NULL)
    {
      lv_label_set_text(g_ui.focus_status, "Ready");
      lv_obj_set_style_text_color(g_ui.focus_status, color(COLOR_MUTED), 0);
    }
}

/****************************************************************************
 * Home 页
 ****************************************************************************/

static void create_home_page(lv_obj_t *page)
{
  lv_obj_t *top;
  lv_obj_t *hero;
  lv_obj_t *hero_copy;
  lv_obj_t *timer_box;
  lv_obj_t *button;
  lv_obj_t *rail;
  lv_obj_t *rail_head;
  lv_obj_t *strip;
  lv_obj_t *strip_copy;
  lv_obj_t *strip_right;
  int32_t   arc_size = g_compact ? 76 : 88;

  configure_page(page);

  /* 上半部: 左边专注卡, 右边深色状态栏 */
  top = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(top, LV_PCT(100));
  lv_obj_set_height(top, 1);
  lv_obj_set_flex_grow(top, 1);
  lv_obj_set_style_pad_column(top, g_compact ? 6 : 10, 0);

  /* --- 专注卡 --- */
  hero = create_panel(top, LV_PCT(100));
  lv_obj_set_width(hero, 1);
  lv_obj_set_flex_grow(hero, 3);
  lv_obj_set_layout(hero, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(hero, 6, 0);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  hero_copy = create_group(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(hero_copy, 1);
  lv_obj_set_height(hero_copy, LV_PCT(100));
  lv_obj_set_flex_grow(hero_copy, 1);
  lv_obj_set_style_pad_row(hero_copy, 3, 0);
  lv_obj_set_flex_align(hero_copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  create_label(hero_copy, "Today's Focus", font_sm(), COLOR_BLUE);
  create_label(hero_copy, "Deep Work", font_lg(), COLOR_TEXT);
  if (!g_compact)
    {
      create_label(hero_copy, "One task at a time", font_sm(), COLOR_MUTED);
    }
  g_ui.focus_status = create_label(hero_copy, "Ready", font_sm(), COLOR_MUTED);
  lv_label_set_long_mode(g_ui.focus_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.focus_status, LV_PCT(100));

  /* 圆环计时器 */
  timer_box = create_group(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(timer_box, arc_size + 8, LV_PCT(100));
  lv_obj_set_flex_align(timer_box, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  g_ui.focus_arc = lv_arc_create(timer_box);
  lv_obj_set_size(g_ui.focus_arc, arc_size, arc_size);
  lv_obj_remove_flag(g_ui.focus_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_style(g_ui.focus_arc, NULL, LV_PART_KNOB);
  lv_arc_set_rotation(g_ui.focus_arc, 270);
  lv_arc_set_bg_angles(g_ui.focus_arc, 0, 360);
  lv_arc_set_range(g_ui.focus_arc, 0, 100);
  lv_arc_set_value(g_ui.focus_arc, 0);
  lv_obj_set_style_arc_width(g_ui.focus_arc, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_ui.focus_arc, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_ui.focus_arc, color(COLOR_ARC_TRACK),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_color(g_ui.focus_arc, color(COLOR_BLUE),
                             LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(g_ui.focus_arc, LV_OPA_TRANSP, LV_PART_MAIN);

  g_ui.focus_time = create_label(g_ui.focus_arc, "25:00", font_xl(),
                                 COLOR_TEXT);
  lv_obj_center(g_ui.focus_time);

  /* 开始/暂停按钮 */
  button = lv_button_create(timer_box);
  lv_obj_set_size(button, 34, 34);
  lv_obj_set_style_radius(button, 17, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_DARK_ALT), LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, focus_event_cb, LV_EVENT_CLICKED, NULL);
  g_ui.focus_button_label = create_label(button, ICON_PLAY, font_md(),
                                         COLOR_WHITE);
  lv_obj_center(g_ui.focus_button_label);

  /* --- 深色状态栏 --- */
  rail = create_dark_panel(top);
  lv_obj_set_width(rail, 1);
  lv_obj_set_height(rail, LV_PCT(100));
  lv_obj_set_flex_grow(rail, 2);

  rail_head = create_group(rail, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(rail_head, LV_PCT(100));
  lv_obj_set_height(rail_head, g_compact ? 18 : 22);
  lv_obj_set_flex_align(rail_head, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(rail_head, "System", font_sm(), COLOR_WHITE);
  create_label(rail_head, "OK", font_sm(), COLOR_GREEN);

  create_dark_status_row(rail, ICON_CPU, "CPU", "--", COLOR_TEAL,
                         &g_ui.home_cpu);
  create_dark_status_row(rail, ICON_MEM, "Mem", "--", COLOR_BLUE,
                         &g_ui.home_memory);
  create_dark_status_row(rail, ICON_NET, "Net", "--", COLOR_GREEN,
                         &g_ui.home_network);
  create_dark_status_row(rail, ICON_TIME, "Up", "--:--", COLOR_AMBER,
                         &g_ui.home_uptime);

  /* 下方任务条 */
  strip = create_panel(page, g_compact ? 44 : 52);
  lv_obj_set_layout(strip, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(strip, g_compact ? 6 : 8, 0);
  lv_obj_set_style_pad_column(strip, 8, 0);
  lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  create_icon_badge(strip, ICON_TASKS, COLOR_BLUE, g_compact ? 22 : 26);

  strip_copy = create_group(strip, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(strip_copy, 1);
  lv_obj_set_height(strip_copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(strip_copy, 1);
  lv_obj_set_style_pad_row(strip_copy, 1, 0);
  create_label(strip_copy, "Study Tasks", font_sm(), COLOR_MUTED);
  g_ui.home_task_count = create_label(strip_copy, "2 pending", font_md(),
                                      COLOR_TEXT);
  lv_label_set_long_mode(g_ui.home_task_count, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.home_task_count, LV_PCT(100));

  strip_right = create_group(strip, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(strip_right, 48, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(strip_right, 1, 0);
  lv_obj_set_flex_align(strip_right, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  create_label(strip_right, "Done", font_sm(), COLOR_MUTED);
  create_label(strip_right, "2/3", font_md(), COLOR_BLUE);

  (void)focus_reset_event_cb;
}

/****************************************************************************
 * Status 页
 ****************************************************************************/

static void create_status_page(lv_obj_t *page)
{
  lv_obj_t *metrics;
  lv_obj_t *panel;
  lv_obj_t *row;
  lv_obj_t *dummy;

  configure_page(page);

  create_page_heading(page, ICON_STATUS, COLOR_TEAL, "System Status",
                      "Live device metrics");

  /* 两张等分指标卡 */
  metrics = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(metrics, LV_PCT(100));
  lv_obj_set_height(metrics, g_compact ? 62 : 74);
  lv_obj_set_style_pad_column(metrics, g_compact ? 6 : 10, 0);

  create_metric_card(metrics, COLOR_TEAL, "CPU Load", "--",
                     &g_ui.status_cpu);
  create_metric_card(metrics, COLOR_BLUE, "Memory", "--",
                     &g_ui.status_memory);

  /* 使用率进度条 */
  panel = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 6, 0);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "CPU", font_sm(), COLOR_MUTED);
  create_label(row, "%", font_sm(), COLOR_TEAL);
  g_ui.status_cpu_bar = create_thin_bar(panel, COLOR_TEAL);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "Memory", font_sm(), COLOR_MUTED);
  create_label(row, "%", font_sm(), COLOR_BLUE);
  g_ui.status_memory_bar = create_thin_bar(panel, COLOR_BLUE);

  /* 设备信息 */
  panel = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 4, 0);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "Network", font_sm(), COLOR_MUTED);
  create_label(row, "Connected", font_sm(), COLOR_GREEN);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "Uptime", font_sm(), COLOR_MUTED);
  g_ui.status_uptime = create_label(row, "--:--", font_sm(), COLOR_TEXT);

  (void)dummy;
}

/****************************************************************************
 * Tasks 页
 ****************************************************************************/

static void create_task_row(lv_obj_t *parent, const char *title,
                            const char *meta, uint32_t accent, bool done)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *mark;
  lv_obj_t *copy;

  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_style_pad_top(row, 2, 0);
  lv_obj_set_style_pad_bottom(row, 2, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* 状态点: 完成填充, 未完成描边 */
  mark = lv_obj_create(row);
  object_reset(mark);
  lv_obj_set_size(mark, 12, 12);
  lv_obj_set_style_radius(mark, 6, 0);
  if (done)
    {
      lv_obj_set_style_bg_color(mark, color(accent), 0);
      lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    }
  else
    {
      lv_obj_set_style_bg_color(mark, color(COLOR_SURFACE), 0);
      lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(mark, color(accent), 0);
      lv_obj_set_style_border_width(mark, 2, 0);
    }

  copy = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(copy, 1);
  lv_obj_set_height(copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(copy, 1);
  lv_obj_set_style_pad_row(copy, 1, 0);

  {
    lv_obj_t *title_label = create_label(copy, title, font_sm(),
                                         done ? COLOR_MUTED : COLOR_TEXT);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title_label, LV_PCT(100));
  }

  create_label(row, meta, font_sm(), done ? COLOR_GREEN : COLOR_MUTED);
}

static void create_tasks_page(lv_obj_t *page)
{
  lv_obj_t *summary;
  lv_obj_t *panel;

  configure_page(page);

  create_page_heading(page, ICON_TASKS, COLOR_AMBER, "Study Tasks",
                      "Track today's plan");

  /* 统计卡 */
  summary = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(summary, LV_PCT(100));
  lv_obj_set_height(summary, g_compact ? 58 : 68);
  lv_obj_set_style_pad_column(summary, g_compact ? 6 : 10, 0);

  create_metric_card(summary, COLOR_AMBER, "Pending", "2",
                     &g_ui.task_pending);
  create_metric_card(summary, COLOR_GREEN, "Completed", "2",
                     &g_ui.task_done);

  /* 任务列表 */
  panel = create_panel(page, 1);
  lv_obj_set_flex_grow(panel, 1);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 2, 0);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_style_bg_color(panel, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);

  create_task_row(panel, "Read ai_agent design", "30m", COLOR_BLUE, false);
  create_task_row(panel, "Fix G2D IRQ order", "45m", COLOR_AMBER, false);
  create_task_row(panel, "Port UI to board", "done", COLOR_GREEN, true);
  create_task_row(panel, "Verify RGB565 colors", "done", COLOR_GREEN, true);

  g_ui.task_bar = create_thin_bar(page, COLOR_GREEN);
  lv_bar_set_value(g_ui.task_bar, 50, LV_ANIM_OFF);
}

/****************************************************************************
 * AI 页
 ****************************************************************************/

/* 聊天气泡。用户消息右对齐蓝底, 助手消息左对齐浅底 */
static void create_chat_bubble(lv_obj_t *parent, bool from_user,
                               const char *text)
{
  lv_obj_t *line = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *bubble;
  lv_obj_t *label;

  lv_obj_set_width(line, LV_PCT(100));
  lv_obj_set_height(line, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(line,
                        from_user ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  bubble = lv_obj_create(line);
  lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(bubble, LV_PCT(78));
  lv_obj_set_height(bubble, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(bubble,
                            color(from_user ? COLOR_BLUE
                                            : COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bubble, 0, 0);
  lv_obj_set_style_radius(bubble, 8, 0);
  lv_obj_set_style_pad_all(bubble, 8, 0);
  lv_obj_set_style_shadow_width(bubble, 0, 0);

  label = create_label(bubble, text, font_sm(),
                       from_user ? COLOR_WHITE : COLOR_TEXT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, LV_PCT(100));
}

static void create_ai_page(lv_obj_t *page)
{
  lv_obj_t *chat;
  lv_obj_t *composer;
  lv_obj_t *field;
  lv_obj_t *send;
  lv_obj_t *mic;

  configure_page(page);

  create_page_heading(page, ICON_AI, COLOR_PURPLE, "AI Assistant",
                      "Ask about your study plan");

  /* 对话区, 可滚动 */
  chat = create_panel(page, 1);
  lv_obj_set_flex_grow(chat, 1);
  lv_obj_set_layout(chat, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(chat, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(chat, 6, 0);
  lv_obj_add_flag(chat, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(chat, LV_DIR_VER);

  create_chat_bubble(chat, false, "Hi, I can help plan your study session.");
  create_chat_bubble(chat, true, "What should I focus on first?");
  create_chat_bubble(chat, false,
                     "Start with the ai_agent architecture, then review "
                     "the driver notes.");

  /* 状态行 */
  {
    lv_obj_t *state_row = create_group(page, LV_FLEX_FLOW_ROW);
    lv_obj_t *dot;

    lv_obj_set_width(state_row, LV_PCT(100));
    lv_obj_set_height(state_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(state_row, 5, 0);
    lv_obj_set_flex_align(state_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    dot = lv_obj_create(state_row);
    object_reset(dot);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_color(dot, color(COLOR_GREEN), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    g_ui.ai_state = create_label(state_row, "Idle", font_sm(), COLOR_MUTED);
  }

  /* 输入行 */
  composer = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(composer, LV_PCT(100));
  lv_obj_set_height(composer, g_compact ? 32 : 38);
  lv_obj_set_style_pad_column(composer, 6, 0);
  lv_obj_set_flex_align(composer, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  field = lv_obj_create(composer);
  lv_obj_remove_flag(field, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(field, 1);
  lv_obj_set_flex_grow(field, 1);
  lv_obj_set_height(field, LV_PCT(100));
  lv_obj_set_style_bg_color(field, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(field, color(COLOR_BORDER_SOFT), 0);
  lv_obj_set_style_border_width(field, 1, 0);
  lv_obj_set_style_radius(field, 8, 0);
  lv_obj_set_style_pad_left(field, 8, 0);
  lv_obj_set_style_shadow_width(field, 0, 0);
  {
    lv_obj_t *hint = create_label(field, "Type a message", font_sm(),
                                  COLOR_MUTED);
    lv_obj_align(hint, LV_ALIGN_LEFT_MID, 0, 0);
  }

  mic = lv_button_create(composer);
  lv_obj_set_size(mic, g_compact ? 32 : 38, LV_PCT(100));
  lv_obj_set_style_bg_color(mic, color(COLOR_PURPLE), 0);
  lv_obj_set_style_bg_opa(mic, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(mic, 8, 0);
  lv_obj_set_style_shadow_width(mic, 0, 0);
  {
    lv_obj_t *label = create_label(mic, "MIC", font_sm(), COLOR_WHITE);
    lv_obj_center(label);
  }

  send = lv_button_create(composer);
  lv_obj_set_size(send, g_compact ? 38 : 44, LV_PCT(100));
  lv_obj_set_style_bg_color(send, color(COLOR_BLUE), 0);
  lv_obj_set_style_bg_opa(send, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(send, 8, 0);
  lv_obj_set_style_shadow_width(send, 0, 0);
  {
    lv_obj_t *label = create_label(send, "Send", font_sm(), COLOR_WHITE);
    lv_obj_center(label);
  }
}

/****************************************************************************
 * Settings 页
 ****************************************************************************/

static lv_obj_t *create_setting_row(lv_obj_t *parent, const char *title,
                                    const char *detail, bool with_switch,
                                    bool on)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *copy;
  lv_obj_t *control = NULL;

  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(row, 3, 0);
  lv_obj_set_style_pad_bottom(row, 3, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  copy = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(copy, 1);
  lv_obj_set_height(copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(copy, 1);
  lv_obj_set_style_pad_row(copy, 1, 0);
  create_label(copy, title, font_sm(), COLOR_TEXT);
  if (!g_compact)
    {
      create_label(copy, detail, font_sm(), COLOR_MUTED);
    }

  if (with_switch)
    {
      control = lv_switch_create(row);
      lv_obj_set_size(control, 40, 20);
      lv_obj_set_style_bg_color(control, color(COLOR_ARC_TRACK),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(control, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_color(control, color(COLOR_BLUE),
                                LV_PART_INDICATOR | LV_STATE_CHECKED);
      lv_obj_set_style_bg_opa(control, LV_OPA_COVER,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
      lv_obj_set_style_shadow_width(control, 0, 0);
      if (on)
        {
          lv_obj_add_state(control, LV_STATE_CHECKED);
        }
    }
  else
    {
      create_label(row, detail, font_sm(), COLOR_BLUE);
    }

  return control;
}

static void create_settings_page(lv_obj_t *page)
{
  lv_obj_t *panel;

  configure_page(page);

  create_page_heading(page, ICON_SETTINGS, COLOR_CORAL, "Settings",
                      "Terminal preferences");

  /* 开关组 */
  panel = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 2, 0);

  create_setting_row(panel, "Focus reminders", "Alert when a session ends",
                     true, true);
  create_setting_row(panel, "Voice wake", "Listen for the wake word",
                     true, false);
  create_setting_row(panel, "Auto brightness", "Follow ambient light",
                     true, true);

  /* 设备信息 */
  panel = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 3, 0);

  create_setting_row(panel, "Device", "R528", false, false);
  create_setting_row(panel, "System", "OpenVela", false, false);
  create_setting_row(panel, "Build", "Contest 2026", false, false);
  create_setting_row(panel, "Team", "120", false, false);
}

/****************************************************************************
 * 顶部状态栏
 ****************************************************************************/

static void create_header(lv_obj_t *parent)
{
  lv_obj_t *header;
  lv_obj_t *left;
  lv_obj_t *dot;
  time_t now;
  struct tm *timeinfo;
  char time_str[32];

  header = lv_obj_create(parent);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, g_compact ? 28 : 34);
  lv_obj_set_style_bg_color(header, color(COLOR_DARK), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_shadow_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_pad_left(header, 10, 0);
  lv_obj_set_style_pad_right(header, 10, 0);
  lv_obj_set_layout(header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* 左侧: 绿点 + 应用名 */
  left = create_group(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(left, LV_SIZE_CONTENT);
  lv_obj_set_height(left, LV_PCT(100));
  lv_obj_set_style_pad_column(left, 6, 0);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  dot = lv_obj_create(left);
  object_reset(dot);
  lv_obj_set_size(dot, 6, 6);
  lv_obj_set_style_radius(dot, 3, 0);
  lv_obj_set_style_bg_color(dot, color(COLOR_GREEN), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

  create_label(left, "AI Study Terminal", font_sm(), COLOR_WHITE);

  /* 右侧: 时钟 */
  now = time(NULL);
  timeinfo = localtime(&now);
  if (timeinfo != NULL)
    {
      snprintf(time_str, sizeof(time_str), "%02d:%02d",
               timeinfo->tm_hour, timeinfo->tm_min);
    }
  else
    {
      snprintf(time_str, sizeof(time_str), "--:--");
    }

  g_ui.header_clock = create_label(header, time_str, font_sm(), COLOR_WHITE);
}

/****************************************************************************
 * 定时刷新
 ****************************************************************************/

static void update_timer_cb(lv_timer_t *timer)
{
  time_t now;
  struct tm *timeinfo;
  char buffer[32];
  uint32_t remaining;
  uint32_t percent;

  LV_UNUSED(timer);

  g_ui.tick++;

  /* 时钟 */
  if (g_ui.header_clock != NULL)
    {
      now = time(NULL);
      timeinfo = localtime(&now);
      if (timeinfo != NULL)
        {
          snprintf(buffer, sizeof(buffer), "%02d:%02d",
                   timeinfo->tm_hour, timeinfo->tm_min);
          lv_label_set_text(g_ui.header_clock, buffer);
        }
    }

  /* 运行时长 —— 用 tick 自己算, 不调 sysinfo()。
   * V1 就是在 update_system_widgets() 里调 sysinfo() -> mallinfo() ->
   * mm_foreach() 走堆时崩的, 这里不碰堆遍历。
   */
  snprintf(buffer, sizeof(buffer), "%02u:%02u",
           (unsigned)(g_ui.tick / 60), (unsigned)(g_ui.tick % 60));

  if (g_ui.home_uptime != NULL)
    {
      lv_label_set_text(g_ui.home_uptime, buffer);
    }

  if (g_ui.status_uptime != NULL)
    {
      lv_label_set_text(g_ui.status_uptime, buffer);
    }

  /* 专注计时 */
  if (g_ui.focus_running)
    {
      g_ui.focus_elapsed++;

      if (g_ui.focus_elapsed >= FOCUS_TOTAL_SEC)
        {
          g_ui.focus_running = false;
          g_ui.focus_elapsed = FOCUS_TOTAL_SEC;

          if (g_ui.focus_status != NULL)
            {
              lv_label_set_text(g_ui.focus_status, "Session done");
              lv_obj_set_style_text_color(g_ui.focus_status,
                                          color(COLOR_GREEN), 0);
            }

          if (g_ui.focus_button_label != NULL)
            {
              lv_label_set_text(g_ui.focus_button_label, ICON_PLAY);
            }
        }

      remaining = FOCUS_TOTAL_SEC - g_ui.focus_elapsed;

      if (g_ui.focus_time != NULL)
        {
          snprintf(buffer, sizeof(buffer), "%02u:%02u",
                   (unsigned)(remaining / 60), (unsigned)(remaining % 60));
          lv_label_set_text(g_ui.focus_time, buffer);
        }

      if (g_ui.focus_arc != NULL)
        {
          percent = (g_ui.focus_elapsed * 100u) / FOCUS_TOTAL_SEC;
          lv_arc_set_value(g_ui.focus_arc, (int32_t)percent);
        }
    }
}

/****************************************************************************
 * 主 UI
 ****************************************************************************/

static void create_ui(void)
{
  lv_obj_t *tabview;
  lv_obj_t *tabbar;
  lv_obj_t *home;
  lv_obj_t *status;
  lv_obj_t *tasks;
  lv_obj_t *ai;
  lv_obj_t *settings;
  uint32_t index;

  memset(&g_ui, 0, sizeof(g_ui));

  g_ui.screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_ui.screen, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_ui.screen, 0, 0);
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
  lv_tabview_set_tab_bar_size(tabview, g_compact ? 38 : 46);
  lv_obj_set_style_bg_color(tabview, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tabview, 0, 0);

  tabbar = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_width(tabbar, LV_PCT(100));
  lv_obj_set_style_bg_color(tabbar, color(COLOR_DARK), 0);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(tabbar, font_sm(), 0);
  lv_obj_set_style_text_color(tabbar, color(COLOR_TABBAR_TXT), 0);
  lv_obj_set_style_text_color(tabbar, color(COLOR_TABBAR_SEL),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabbar, color(COLOR_DARK_ALT),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER,
                          LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabbar, color(COLOR_TABBAR_SEL),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabbar, 2,
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tabbar, LV_BORDER_SIDE_TOP,
                               LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabbar, 0, 0);
  lv_obj_set_style_pad_all(tabbar, 0, 0);
  lv_obj_set_style_pad_column(tabbar, 0, 0);
  lv_obj_set_style_shadow_width(tabbar, 0, 0);

  home     = lv_tabview_add_tab(tabview, "Home");
  status   = lv_tabview_add_tab(tabview, "Stat");
  tasks    = lv_tabview_add_tab(tabview, "Task");
  ai       = lv_tabview_add_tab(tabview, "AI");
  settings = lv_tabview_add_tab(tabview, "Set");

  /* 五个标签等分宽度 */
  for (index = 0; index < lv_obj_get_child_count(tabbar); index++)
    {
      lv_obj_set_flex_grow(lv_obj_get_child(tabbar, index), 1);
    }

  create_home_page(home);
  create_status_page(status);
  create_tasks_page(tasks);
  create_ai_page(ai);
  create_settings_page(settings);

  lv_tabview_set_active(tabview, 0, LV_ANIM_OFF);
  lv_timer_create(update_timer_cb, 1000, NULL);

  g_ui.tabview = tabview;
}

/****************************************************************************
 * 显示方向
 ****************************************************************************/

static void configure_display_orientation(lv_display_t *display)
{
  int32_t horizontal = lv_display_get_horizontal_resolution(display);
  int32_t vertical = lv_display_get_vertical_resolution(display);

  /* 竖屏面板顺时针转 90 度, 得到 480x320 横屏 */
  if (horizontal < vertical)
    {
      lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
      horizontal = lv_display_get_horizontal_resolution(display);
      vertical = lv_display_get_vertical_resolution(display);
    }

  g_compact = (horizontal <= 600) && (vertical <= 400);

  printf("study_terminal_v2: display %ldx%ld landscape%s\n",
         (long)horizontal, (long)vertical, g_compact ? " compact" : "");
}

/****************************************************************************
 * 触摸镜像修正
 ****************************************************************************/

#ifdef CONFIG_FT5X06_SWAPXY
static void read_touch_aligned_landscape(lv_indev_t *indev,
                                         lv_indev_data_t *data)
{
  lv_display_t *display;
  int32_t horizontal;

  g_touch_read_cb(indev, data);
  display = lv_indev_get_display(indev);
  horizontal = lv_display_get_horizontal_resolution(display);

  if (horizontal > 0)
    {
      data->point.x = horizontal - 1 - data->point.x;
    }
}
#endif

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  (void)argc;
  (void)argv;

  printf("study_terminal_v2: starting full UI\n");

  if (lv_is_initialized())
    {
      fprintf(stderr, "study_terminal_v2: LVGL already initialized\n");
      return 1;
    }

#ifdef CONFIG_BOARDCTL
#ifndef CONFIG_NSH_ARCHINIT
  boardctl(BOARDIOC_INIT, 0);
#endif
#endif

  lv_init();
  printf("study_terminal_v2: lv_init() completed\n");

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      fprintf(stderr, "study_terminal_v2: display init failed\n");
      lv_deinit();
      return 1;
    }

  printf("study_terminal_v2: display up\n");

  configure_display_orientation(result.disp);

#ifdef CONFIG_FT5X06_SWAPXY
  if (result.indev != NULL)
    {
      g_touch_read_cb = lv_indev_get_read_cb(result.indev);
      if (g_touch_read_cb != NULL)
        {
          lv_indev_set_read_cb(result.indev, read_touch_aligned_landscape);
          printf("study_terminal_v2: touch remapping enabled\n");
        }
    }
#endif

  create_ui();
  printf("study_terminal_v2: UI created (5 pages)\n");

  lv_screen_load(g_ui.screen);
  printf("study_terminal_v2: screen loaded\n");

  printf("study_terminal_v2: entering main loop\n");
  while (true)
    {
      uint32_t idle = lv_timer_handler();
      usleep((idle == 0 ? 1 : idle) * 1000);
    }

  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
