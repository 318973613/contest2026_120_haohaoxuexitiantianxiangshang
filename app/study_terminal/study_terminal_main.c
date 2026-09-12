/****************************************************************************
 * Contest 2026 team 120 - AI study terminal LVGL demo
 ****************************************************************************/

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#else
#  include <nuttx/config.h>
#  include <arpa/inet.h>
#  include <errno.h>
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <sys/boardctl.h>
#  include <sys/stat.h>
#  include <sys/sysinfo.h>
#  include <sys/utsname.h>
#  include <unistd.h>

/* Board-only wiring. The Win32 preview has no ai_agent, no microphone and no
 * task file to own, so every bridge call below is compiled out there and the
 * page keeps the canned copy it was designed with.
 */

#  include "ai_chat_bridge.h"
#  include "task_presets.h"
#  include "voice_ui_bridge.h"
#  include "wifi_setup.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <lvgl/lvgl.h>
#if LV_USE_FREETYPE
#  include <lvgl/src/libs/freetype/lv_freetype.h>
#endif

/* LV_SYMBOL_* pulls glyphs from the FontAwesome range, and rendering those
 * on this panel faults inside the sunxi G2D path. Every icon below is an
 * ASCII stand-in drawn with the ordinary Montserrat ranges instead.
 */

#define UI_ICON_HOME      "H"
#define UI_ICON_BARS      "="
#define UI_ICON_LIST      "L"
#define UI_ICON_AUDIO     "MIC"
#define UI_ICON_SETTINGS  "S"
#define UI_ICON_REFRESH   "R"
#define UI_ICON_WIFI      "W"
#define UI_ICON_DRIVE     "M"
#define UI_ICON_DIRECTORY "D"
#define UI_ICON_PLAY      ">"
#define UI_ICON_PAUSE     "||"
#define UI_ICON_OK        "OK"
#define UI_ICON_TRASH     "X"

extern const lv_font_t lv_font_simsun_16_cjk;
#ifdef _WIN32
extern const lv_font_t lv_font_study_16;
#endif

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#  include <uv.h>
#endif

#define COLOR_BG          0xdceff3
#define COLOR_BG_BOTTOM   0xcfe8ee
#define COLOR_SURFACE     0xffffff
#define COLOR_SURFACE_ALT 0xe7f1f4
#define COLOR_BORDER      0xffffff
#define COLOR_TEXT        0x14212b
#define COLOR_MUTED       0x61727d
#define COLOR_TEAL        0x0f9f8f
#define COLOR_BLUE        0x246bfd
#define COLOR_PURPLE      0x7158d9
#define COLOR_AMBER       0xeaa126
#define COLOR_CORAL       0xdc5d67
#define COLOR_GREEN       0x209765
#define COLOR_WHITE       0xffffff
#define COLOR_OPENVELA    0x0074ff

#ifdef _WIN32
#  define TASK_FILE "runtime/ai_agent/STUDY_TASKS.md"
#  define UI_FONT_FILE "runtime/NotoSansSC-Regular.ttf"
#  define UI_WALLPAPER_FILE \
     "../assets/wallpapers/study-terminal-wallpaper-480x320.rgb565"
#  define UI_BOOT_LOGO_FILE "../assets/branding/openvela-64x64.rgb565"
#else
#  define TASK_FILE "/data/ai_agent/STUDY_TASKS.md"
#  define TASK_TEMP_FILE "/data/ai_agent/STUDY_TASKS.md.tmp"
#  define VOLUME_FILE "/data/ai_agent/STUDY_VOLUME.json"
#  define VOLUME_TEMP_FILE "/data/ai_agent/STUDY_VOLUME.json.tmp"
#  define AGENT_DATA_DIR "/data/ai_agent"
#  define UI_FONT_FILE "/data/NotoSansSC-Regular.ttf"
#  define UI_WALLPAPER_FILE "/data/study-terminal-wallpaper.rgb565"
#endif

#ifndef _WIN32
#  define MAX_TASKS 20
#  define MAX_TASK_TEXT 80
#  define VISIBLE_TASKS 4
#endif
#define UI_WALLPAPER_WIDTH 480
#define UI_WALLPAPER_HEIGHT 320
#define UI_WALLPAPER_BYTES (UI_WALLPAPER_WIDTH * UI_WALLPAPER_HEIGHT * 2)
#define UI_BOOT_LOGO_WIDTH  64
#define UI_BOOT_LOGO_HEIGHT 64
#define UI_BOOT_LOGO_BYTES  (UI_BOOT_LOGO_WIDTH * UI_BOOT_LOGO_HEIGHT * 2)
#define FOCUS_SECONDS (25 * 60)
#define SPLASH_PROGRESS_MS 1350
#define SPLASH_HOLD_MS     1650
#define SPLASH_FADE_MS     240
#define STANDBY_IDLE_SECONDS 45

struct study_ui_s
{
  lv_obj_t *screen;
  lv_obj_t *standby_screen;
  lv_obj_t *tabview;
  lv_obj_t *header_clock;
  lv_obj_t *home_task_count;
  lv_obj_t *home_network;
  lv_obj_t *home_cpu;
  lv_obj_t *home_memory;
  lv_obj_t *home_uptime;
  lv_obj_t *focus_status;
  lv_obj_t *focus_button;
  lv_obj_t *focus_button_label;
  lv_obj_t *focus_bar;
  lv_obj_t *focus_time;
  lv_obj_t *cpu_value;
  lv_obj_t *memory_value;
  lv_obj_t *network_value;
  lv_obj_t *uptime_value;
  lv_obj_t *task_pending_value;
  lv_obj_t *task_completed_value;
  lv_obj_t *task_file_status;
  lv_obj_t *task_sync_label;
  lv_obj_t *ai_connection;
  lv_obj_t *ai_button;
  lv_obj_t *ai_pulse;
  lv_obj_t *ai_check_label;
  lv_obj_t *voice_screen;
  lv_obj_t *voice_ring_outer;
  lv_obj_t *voice_ring_mid;
  lv_obj_t *voice_ring_inner;
  lv_obj_t *voice_avatar;
  lv_obj_t *voice_status;
  lv_obj_t *voice_heard;
  lv_obj_t *voice_reply;
  lv_obj_t *voice_close_label;
  lv_obj_t *standby_time;
  lv_obj_t *standby_date;
  lv_obj_t *standby_weather;
  lv_obj_t *standby_temp;
  lv_obj_t *standby_hint;
#ifndef _WIN32
  /* Live task list (replaces the three hardcoded demo rows). */

  lv_obj_t *task_list_container;

  /* Voice screen additions: the design had a scripted 4-step animation; on
   * the board the same widgets are driven by real PTT/ASR/LLM/TTS state.
   */

  lv_obj_t *voice_ptt_button;
  lv_obj_t *voice_ptt_label;
  lv_obj_t *interim_bubble;
  bool is_recording;

  /* Settings additions. */

  lv_obj_t *volume_slider;
  lv_obj_t *volume_label;
  lv_obj_t *wake_switch;
  lv_obj_t *wifi_button;
#endif
  bool focus_running;
  bool on_standby;
  bool voice_active;
  uint32_t focus_elapsed;
  uint32_t idle_seconds;
  uint32_t voice_phase;
};

#ifndef _WIN32
struct study_task_s
{
  bool completed;
  char text[MAX_TASK_TEXT + 1];
};

#endif

static struct study_ui_s g_ui;
static uint32_t g_initial_tab;
#ifndef _WIN32
static lv_font_t *g_dynamic_cjk_font;
#endif
static bool g_compact_layout;
static uint8_t *g_wallpaper_data;
static lv_image_dsc_t g_wallpaper;

/* Network/agent status is sampled every NET_POLL_SECONDS rather than every
 * second: both probes allocate file descriptors, and doing that once per
 * second races ai_agent's network watcher inside NuttX's fd allocator.
 */

#define NET_POLL_SECONDS 10

static uint8_t g_net_poll_tick;
static bool g_net_cached_online;
static char g_net_cached_text[64];
#ifndef _WIN32
static bool g_agent_cached_running;
#endif

#ifndef _WIN32
static time_t g_task_file_mtime;
static bool g_task_reload_pending;

#  ifdef CONFIG_FT5X06_SWAPXY
static lv_indev_read_cb_t g_touch_read_cb;
#  endif

static void update_task_list(void);
#endif

static void note_user_activity(void);
static void show_main_ui(void);
static void show_standby_ui(void);
static void show_voice_ui(void);
static void hide_voice_ui(void);
static void start_voice_wave(void);
static void stop_voice_wave(void);
static void update_standby_widgets(void);
static void update_voice_widgets(void);
static void create_voice_screen(void);
static void main_input_event_cb(lv_event_t *event);
static lv_obj_t *create_panel(lv_obj_t *parent, int32_t height);
static lv_obj_t *create_icon_badge(lv_obj_t *parent, const char *symbol,
                                   uint32_t accent, int32_t size);
static lv_obj_t *create_group(lv_obj_t *parent, lv_flex_flow_t flow);
static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t text_color);
static void object_reset(lv_obj_t *obj);
static void set_label_text(lv_obj_t *label, const char *text);
static void create_wallpaper(lv_obj_t *parent);
static void pulse_scale_cb(void *object, int32_t value);
static void pulse_opacity_cb(void *object, int32_t value);
#ifdef _WIN32
static uint8_t *g_boot_logo_data;
static lv_image_dsc_t g_boot_logo;
#endif

static lv_color_t color(uint32_t value)
{
  return lv_color_hex(value);
}

static const lv_font_t *ui_font(void)
{
#ifdef _WIN32
  return &lv_font_study_16;
#else
  if (g_dynamic_cjk_font != NULL)
    {
      return g_dynamic_cjk_font;
    }

  return &lv_font_simsun_16_cjk;
#endif
}

#if !defined(_WIN32) && defined(CONFIG_FT5X06_SWAPXY)
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

static void initialize_ui_font(void)
{
#if LV_USE_FREETYPE
  lv_freetype_init(128);
  g_dynamic_cjk_font = lv_freetype_font_create(
    UI_FONT_FILE, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 18,
    LV_FREETYPE_FONT_STYLE_NORMAL);
#endif
}

static void initialize_wallpaper(void)
{
  FILE *file;
  size_t bytes_read;

  if (!g_compact_layout)
    {
      return;
    }

  file = fopen(UI_WALLPAPER_FILE, "rb");
  if (file == NULL)
    {
      printf("study_terminal: wallpaper not found: %s\n",
             UI_WALLPAPER_FILE);
      return;
    }

  g_wallpaper_data = malloc(UI_WALLPAPER_BYTES);
  if (g_wallpaper_data == NULL)
    {
      fclose(file);
      printf("study_terminal: wallpaper allocation failed\n");
      return;
    }

  bytes_read = fread(g_wallpaper_data, 1, UI_WALLPAPER_BYTES, file);
  fclose(file);
  if (bytes_read != UI_WALLPAPER_BYTES)
    {
      printf("study_terminal: wallpaper size mismatch: %lu\n",
             (unsigned long)bytes_read);
      free(g_wallpaper_data);
      g_wallpaper_data = NULL;
      return;
    }

  memset(&g_wallpaper, 0, sizeof(g_wallpaper));
  g_wallpaper.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_wallpaper.header.cf = LV_COLOR_FORMAT_RGB565;
  g_wallpaper.header.w = UI_WALLPAPER_WIDTH;
  g_wallpaper.header.h = UI_WALLPAPER_HEIGHT;
  g_wallpaper.header.stride = UI_WALLPAPER_WIDTH * 2;
  g_wallpaper.data_size = UI_WALLPAPER_BYTES;
  g_wallpaper.data = g_wallpaper_data;
}

#ifdef _WIN32
static void initialize_boot_logo(void)
{
  FILE *file;
  size_t bytes_read;

  file = fopen(UI_BOOT_LOGO_FILE, "rb");
  if (file == NULL)
    {
      printf("study_terminal: boot logo not found: %s\n",
             UI_BOOT_LOGO_FILE);
      return;
    }

  g_boot_logo_data = malloc(UI_BOOT_LOGO_BYTES);
  if (g_boot_logo_data == NULL)
    {
      fclose(file);
      printf("study_terminal: boot logo allocation failed\n");
      return;
    }

  bytes_read = fread(g_boot_logo_data, 1, UI_BOOT_LOGO_BYTES, file);
  fclose(file);
  if (bytes_read != UI_BOOT_LOGO_BYTES)
    {
      printf("study_terminal: boot logo size mismatch: %lu\n",
             (unsigned long)bytes_read);
      free(g_boot_logo_data);
      g_boot_logo_data = NULL;
      return;
    }

  memset(&g_boot_logo, 0, sizeof(g_boot_logo));
  g_boot_logo.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_boot_logo.header.cf = LV_COLOR_FORMAT_RGB565;
  g_boot_logo.header.w = UI_BOOT_LOGO_WIDTH;
  g_boot_logo.header.h = UI_BOOT_LOGO_HEIGHT;
  g_boot_logo.header.stride = UI_BOOT_LOGO_WIDTH * 2;
  g_boot_logo.data_size = UI_BOOT_LOGO_BYTES;
  g_boot_logo.data = g_boot_logo_data;
}
#endif

static void create_wallpaper(lv_obj_t *parent)
{
  lv_obj_t *image;

  if (g_wallpaper_data == NULL || g_wallpaper.data == NULL)
    {
      return;
    }

  image = lv_image_create(parent);
  lv_image_set_src(image, &g_wallpaper);
  lv_obj_add_flag(image, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_pos(image, 0, 0);
  lv_obj_move_background(image);
}

static void configure_display_orientation(lv_display_t *display)
{
  int32_t horizontal = lv_display_get_horizontal_resolution(display);
  int32_t vertical = lv_display_get_vertical_resolution(display);

  if (horizontal < vertical)
    {
      lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
      horizontal = lv_display_get_horizontal_resolution(display);
      vertical = lv_display_get_vertical_resolution(display);
    }

  g_compact_layout = horizontal <= 600 && vertical <= 400;

  printf("study_terminal: display %ldx%ld landscape%s\n",
         (long)horizontal, (long)vertical,
          g_compact_layout ? " compact" : "");
}

static void splash_opacity_cb(void *object, int32_t value)
{
  lv_obj_set_style_opa(object, value, 0);
}

static void splash_scale_cb(void *object, int32_t value)
{
  lv_obj_set_style_transform_scale(object, value, 0);
}

static void splash_rotation_cb(void *object, int32_t value)
{
  lv_obj_set_style_transform_rotation(object, value, 0);
}

static void splash_progress_cb(void *object, int32_t value)
{
  lv_bar_set_value(object, value, LV_ANIM_OFF);
}

static void splash_scan_cb(void *object, int32_t value)
{
  lv_obj_set_x(object, value);
}

static lv_obj_t *create_splash_mark(lv_obj_t *screen)
{
  lv_obj_t *mark = lv_obj_create(screen);
  lv_obj_t *content;

  lv_obj_remove_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(mark, UI_BOOT_LOGO_WIDTH, UI_BOOT_LOGO_HEIGHT);
  lv_obj_align(mark, LV_ALIGN_CENTER, 0, -42);
  lv_obj_set_style_radius(mark, 16, 0);
  lv_obj_set_style_border_width(mark, 0, 0);
  lv_obj_set_style_pad_all(mark, 0, 0);

#ifdef _WIN32
  if (g_boot_logo_data != NULL)
    {
      lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
      content = lv_image_create(mark);
      lv_image_set_src(content, &g_boot_logo);
      lv_obj_center(content);
      return mark;
    }
#endif

  lv_obj_set_style_border_color(mark, color(0x75a0ff), 0);
  lv_obj_set_style_border_width(mark, 1, 0);
  lv_obj_set_style_bg_color(mark, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);

  content = lv_label_create(mark);
  lv_label_set_text(content, "AI");
  lv_obj_set_style_text_font(content, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(content, color(COLOR_WHITE), 0);
  lv_obj_center(content);
  return mark;
}

static lv_obj_t *create_splash_screen(void)
{
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_t *ring;
  lv_obj_t *dot;
  lv_obj_t *mark;
  lv_obj_t *title;
  lv_obj_t *subtitle;
  lv_obj_t *status;
  lv_obj_t *label;
  lv_obj_t *progress;
  lv_obj_t *scan;
  lv_anim_t animation;

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, color(0x071522), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  label = lv_label_create(screen);
  lv_label_set_text(label, "openvela");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_letter_space(label, 1, 0);
  lv_obj_set_style_text_color(label, color(COLOR_OPENVELA), 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 14);

  label = lv_label_create(screen);
  lv_label_set_text(label, "CONTEST 2026 / 120");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(label, color(0x6f8294), 0);
  lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -18, 14);

  ring = lv_obj_create(screen);
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(ring, 94, 94);
  lv_obj_align(ring, LV_ALIGN_CENTER, 0, -42);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ring, color(0x31506d), 0);
  lv_obj_set_style_border_opa(ring, LV_OPA_70, 0);
  lv_obj_set_style_border_width(ring, 1, 0);
  lv_obj_set_style_pad_all(ring, 0, 0);

  dot = lv_obj_create(ring);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, -4);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_bg_color(dot, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_shadow_color(dot, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_shadow_opa(dot, LV_OPA_70, 0);
  lv_obj_set_style_shadow_width(dot, 10, 0);

  mark = create_splash_mark(screen);
  lv_obj_set_style_transform_scale(mark, 208, 0);

  title = lv_label_create(screen);
  lv_label_set_text(title, "智能学习终端");
  lv_obj_set_style_text_font(title, ui_font(), 0);
  lv_obj_set_style_text_color(title, color(COLOR_WHITE), 0);
  lv_obj_set_style_text_letter_space(title, 2, 0);
  lv_obj_set_style_opa(title, LV_OPA_TRANSP, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 26);

  subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "NATIVE LVGL  +  AI_AGENT");
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_letter_space(subtitle, 1, 0);
  lv_obj_set_style_text_color(subtitle, color(0x8ca0b3), 0);
  lv_obj_set_style_opa(subtitle, LV_OPA_TRANSP, 0);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 54);

  status = lv_label_create(screen);
  lv_label_set_text(status, "基于 openvela  ·  AI 服务 Xiaomi MiMo");
  lv_obj_set_style_text_font(status, ui_font(), 0);
  lv_obj_set_style_text_color(status, color(0xa9bac8), 0);
  lv_obj_set_style_opa(status, LV_OPA_TRANSP, 0);
  lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -46);

  progress = lv_bar_create(screen);
  lv_obj_set_size(progress, 260, 5);
  lv_obj_align(progress, LV_ALIGN_BOTTOM_MID, 0, -25);
  lv_obj_set_style_radius(progress, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(progress, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(progress, color(0x21374a), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progress, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress, color(COLOR_OPENVELA),
                            LV_PART_INDICATOR);
  lv_bar_set_range(progress, 0, 100);
  lv_bar_set_value(progress, 0, LV_ANIM_OFF);

  scan = lv_obj_create(screen);
  lv_obj_remove_flag(scan, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(scan, 96, 1);
  lv_obj_set_pos(scan, -96, 43);
  lv_obj_set_style_radius(scan, 0, 0);
  lv_obj_set_style_border_width(scan, 0, 0);
  lv_obj_set_style_bg_color(scan, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_bg_opa(scan, LV_OPA_40, 0);

  lv_anim_init(&animation);
  lv_anim_set_var(&animation, ring);
  lv_anim_set_exec_cb(&animation, splash_rotation_cb);
  lv_anim_set_values(&animation, 0, 3600);
  lv_anim_set_duration(&animation, 1450);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
  lv_anim_start(&animation);

  lv_anim_init(&animation);
  lv_anim_set_var(&animation, mark);
  lv_anim_set_exec_cb(&animation, splash_scale_cb);
  lv_anim_set_values(&animation, 208, 256);
  lv_anim_set_duration(&animation, 420);
  lv_anim_set_path_cb(&animation, lv_anim_path_overshoot);
  lv_anim_start(&animation);

  lv_anim_init(&animation);
  lv_anim_set_var(&animation, title);
  lv_anim_set_exec_cb(&animation, splash_opacity_cb);
  lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_delay(&animation, 240);
  lv_anim_set_duration(&animation, 360);
  lv_anim_start(&animation);

  lv_anim_set_var(&animation, subtitle);
  lv_anim_set_delay(&animation, 430);
  lv_anim_set_duration(&animation, 320);
  lv_anim_start(&animation);

  lv_anim_set_var(&animation, status);
  lv_anim_set_delay(&animation, 720);
  lv_anim_set_duration(&animation, 360);
  lv_anim_start(&animation);

  lv_anim_init(&animation);
  lv_anim_set_var(&animation, progress);
  lv_anim_set_exec_cb(&animation, splash_progress_cb);
  lv_anim_set_values(&animation, 0, 100);
  lv_anim_set_delay(&animation, 120);
  lv_anim_set_duration(&animation, SPLASH_PROGRESS_MS);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
  lv_anim_start(&animation);

  lv_anim_init(&animation);
  lv_anim_set_var(&animation, scan);
  lv_anim_set_exec_cb(&animation, splash_scan_cb);
  lv_anim_set_values(&animation, -96, 480);
  lv_anim_set_delay(&animation, 150);
  lv_anim_set_duration(&animation, 1050);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
  lv_anim_start(&animation);

  return screen;
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
  lv_obj_set_style_bg_opa(panel,
                          g_compact_layout ? LV_OPA_80 : LV_OPA_90, 0);
  lv_obj_set_style_border_color(panel, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, g_compact_layout ? 6 : 18, 0);
  lv_obj_set_style_shadow_color(panel, color(0x9fb4de), 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_10, 0);
  lv_obj_set_style_shadow_width(panel, g_compact_layout ? 8 : 14, 0);
  lv_obj_set_style_shadow_ofs_y(panel, g_compact_layout ? 2 : 4, 0);
  return panel;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, uint32_t bg_color,
                                      const char *text,
                                      lv_event_cb_t callback)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label;

  lv_obj_set_height(button, g_compact_layout ? 28 : 46);
  lv_obj_set_width(button, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(button, color(bg_color), 0);
  lv_obj_set_style_bg_color(button, color(COLOR_SURFACE_ALT), LV_STATE_PRESSED);
  lv_obj_set_style_radius(button, 8, 0);
  lv_obj_set_style_pad_left(button, g_compact_layout ? 10 : 18, 0);
  lv_obj_set_style_pad_right(button, g_compact_layout ? 10 : 18, 0);
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
  lv_obj_set_style_bg_opa(card,
                          g_compact_layout ? LV_OPA_80 : LV_OPA_90, 0);
  lv_obj_set_style_border_color(card, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, g_compact_layout ? 4 : 16, 0);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card, g_compact_layout ? 2 : 8, 0);
  lv_obj_set_style_shadow_color(card, color(0xa4b8dd), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_width(card, g_compact_layout ? 7 : 12, 0);
  lv_obj_set_style_shadow_ofs_y(card, g_compact_layout ? 2 : 3, 0);

  marker = lv_obj_create(card);
  object_reset(marker);
  lv_obj_set_size(marker, g_compact_layout ? 18 : 34,
                  g_compact_layout ? 2 : 4);
  lv_obj_set_style_bg_color(marker, color(accent), 0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(marker, 2, 0);

  create_label(card, eyebrow, ui_font(), COLOR_MUTED);
  *value_label = create_label(card, value, ui_font(), COLOR_TEXT);
  lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(*value_label, LV_PCT(100));
  if (!g_compact_layout)
    {
      create_label(card, detail, ui_font(), COLOR_MUTED);
    }
  return card;
}

static bool read_network_address(char *buffer, size_t buffer_size)
{
#ifdef _WIN32
  struct addrinfo hints;
  struct addrinfo *addresses = NULL;
  struct addrinfo *item;
  char hostname[256];
  bool found = false;

  if (gethostname(hostname, sizeof(hostname)) != 0)
    {
      return false;
    }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(hostname, NULL, &hints, &addresses) != 0)
    {
      return false;
    }

  for (item = addresses; item != NULL; item = item->ai_next)
    {
      struct sockaddr_in *address = (struct sockaddr_in *)item->ai_addr;
      char ip[INET_ADDRSTRLEN];

      if (address == NULL ||
          address->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ||
          address->sin_addr.s_addr == htonl(INADDR_ANY))
        {
          continue;
        }

      if (InetNtopA(AF_INET, &address->sin_addr, ip, sizeof(ip)) == NULL)
        {
          continue;
        }

      snprintf(buffer, buffer_size, "%s", ip);
      found = true;
      break;
    }

  freeaddrinfo(addresses);
  return found;
#else
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
          (item->ifa_flags & IFF_LOOPBACK) != 0 ||
          (item->ifa_flags & IFF_UP) == 0)
        {
          continue;
        }

      address = (struct sockaddr_in *)item->ifa_addr;
      if (address->sin_addr.s_addr == htonl(INADDR_ANY))
        {
          continue;
        }

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
#endif
}

#ifndef _WIN32

/* Modification time of the task file, or 0 when it does not exist yet. */

static time_t task_file_mtime(void)
{
  struct stat info;

  if (stat(TASK_FILE, &info) != 0)
    {
      return 0;
    }

  return info.st_mtime;
}

static size_t load_tasks(struct study_task_s *tasks, size_t capacity)
{
  FILE *file;
  char line[256];
  size_t count = 0;

  file = fopen(TASK_FILE, "r");
  if (file == NULL)
    {
      return 0;
    }

  while (count < capacity && fgets(line, sizeof(line), file) != NULL)
    {
      const char *text;
      size_t text_length;

      if (strncmp(line, "- [ ] ", 6) == 0)
        {
          tasks[count].completed = false;
        }
      else if (strncmp(line, "- [x] ", 6) == 0 ||
               strncmp(line, "- [X] ", 6) == 0)
        {
          tasks[count].completed = true;
        }
      else
        {
          continue;
        }

      /* Keep the line verbatim (including any "[YYYY-MM-DD HH:MM]" stamp the
       * agent wrote) so a save round-trips without losing it. The stamp is
       * hidden at display time instead.
       */

      text = line + 6;
      text_length = strcspn(text, "\r\n");
      if (text_length > MAX_TASK_TEXT)
        {
          text_length = MAX_TASK_TEXT;

          /* Back off to a character boundary: truncating inside a UTF-8
           * sequence would render as garbage in the label.
           */

          while (text_length > 0 && (text[text_length] & 0xc0) == 0x80)
            {
              text_length--;
            }
        }

      memcpy(tasks[count].text, text, text_length);
      tasks[count].text[text_length] = '\0';
      if (tasks[count].text[0] != '\0')
        {
          count++;
        }
    }

  fclose(file);
  return count;
}

static bool save_tasks(const struct study_task_s *tasks, size_t count)
{
  FILE *file;
  size_t index;
  bool ok = true;

  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      return false;
    }

  file = fopen(TASK_TEMP_FILE, "w");
  if (file == NULL)
    {
      return false;
    }

  /* Keep the heading the study-assistant skill expects, so the agent and the
   * UI can both read and rewrite this file.
   */

  if (fputs("# Study Tasks\n\n", file) < 0)
    {
      ok = false;
    }

  for (index = 0; ok && index < count; index++)
    {
      if (fprintf(file, "- [%c] %s\n",
                  tasks[index].completed ? 'x' : ' ',
                  tasks[index].text) < 0)
        {
          ok = false;
          break;
        }
    }

  if (ok && fflush(file) != 0)
    {
      ok = false;
    }

  if (ok && fsync(fileno(file)) != 0)
    {
      ok = false;
    }

  if (fclose(file) != 0)
    {
      ok = false;
    }

  if (ok && rename(TASK_TEMP_FILE, TASK_FILE) == 0)
    {
      return true;
    }

  unlink(TASK_TEMP_FILE);
  return false;
}

/* Hide the leading "[YYYY-MM-DD HH:MM] " stamp the study-assistant skill
 * prepends. A compact row has no room for it, and the stored text keeps it.
 */

static const char *task_display_text(const char *text)
{
  const char *close;

  if (text == NULL || text[0] != '[')
    {
      return text;
    }

  close = strchr(text, ']');
  if (close != NULL && close[1] == ' ' && close[2] != '\0')
    {
      return close + 2;
    }

  return text;
}

#endif /* !_WIN32 */

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

  char buffer[40];

  read_task_counts(&pending, &completed);

  snprintf(buffer, sizeof(buffer), "%d", pending);
  set_label_text(g_ui.task_pending_value, buffer);
  snprintf(buffer, sizeof(buffer), "%d", completed);
  set_label_text(g_ui.task_completed_value, buffer);
  snprintf(buffer, sizeof(buffer), "%d 项待办", pending);
  set_label_text(g_ui.home_task_count, buffer);

  if (pending == 0 && completed == 0)
    {
      set_label_text(g_ui.task_file_status, "暂时没有已保存任务");
    }
  else
    {
      snprintf(buffer, sizeof(buffer), "共 %d 项任务", pending + completed);
      set_label_text(g_ui.task_file_status, buffer);
    }

#ifndef _WIN32
  update_task_list();
#endif
}

#ifndef _WIN32

/* Rebuild the visible task rows from the file. Each row gets a checkbox that
 * writes the toggle back and a delete button, so the list is editable on the
 * board rather than the three fixed demo rows the design mockup showed.
 */

static void task_toggle_event_cb(lv_event_t *event)
{
  lv_obj_t *checkbox = lv_event_get_target(event);
  size_t target = (size_t)(uintptr_t)lv_obj_get_user_data(checkbox);
  struct study_task_s tasks[MAX_TASKS];
  size_t count;

  note_user_activity();

  count = load_tasks(tasks, MAX_TASKS);
  if (target >= count)
    {
      return;
    }

  tasks[target].completed = !tasks[target].completed;

  if (save_tasks(tasks, count))
    {
      g_task_reload_pending = true;
    }
}

static void task_delete_event_cb(lv_event_t *event)
{
  lv_obj_t *button = lv_event_get_target(event);
  size_t target = (size_t)(uintptr_t)lv_obj_get_user_data(button);
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t index;
  size_t write_index = 0;

  note_user_activity();

  count = load_tasks(tasks, MAX_TASKS);
  if (target >= count)
    {
      return;
    }

  for (index = 0; index < count; index++)
    {
      if (index == target)
        {
          continue;
        }

      if (write_index != index)
        {
          tasks[write_index] = tasks[index];
        }

      write_index++;
    }

  if (save_tasks(tasks, write_index))
    {
      g_task_reload_pending = true;
    }
}

static void update_task_list(void)
{
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t index;

  if (g_ui.task_list_container == NULL)
    {
      return;
    }

  count = load_tasks(tasks, MAX_TASKS);

  lv_obj_clean(g_ui.task_list_container);

  if (count == 0)
    {
      lv_obj_t *empty = create_label(g_ui.task_list_container,
                                     "还没有任务，点下面的按钮添加",
                                     ui_font(), COLOR_MUTED);
      lv_obj_set_width(empty, LV_PCT(100));
      g_task_file_mtime = task_file_mtime();
      g_task_reload_pending = false;
      return;
    }

  for (index = 0; index < count && index < VISIBLE_TASKS; index++)
    {
      lv_obj_t *row;
      lv_obj_t *checkbox;
      lv_obj_t *delete_button;
      lv_obj_t *delete_label;

      row = lv_obj_create(g_ui.task_list_container);
      lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 26 : 44);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_set_style_radius(row, 0, 0);
      lv_obj_set_style_pad_all(row, 2, 0);

      checkbox = lv_checkbox_create(row);
      lv_checkbox_set_text(checkbox, task_display_text(tasks[index].text));
      lv_obj_set_style_text_font(checkbox, ui_font(), 0);
      lv_obj_set_style_text_color(checkbox,
                                  color(tasks[index].completed ?
                                        COLOR_MUTED : COLOR_TEXT), 0);
      lv_obj_set_width(checkbox, g_compact_layout ? 250 : 320);
      lv_obj_align(checkbox, LV_ALIGN_LEFT_MID, 0, 0);
      lv_obj_set_user_data(checkbox, (void *)(uintptr_t)index);
      lv_obj_add_event_cb(checkbox, task_toggle_event_cb,
                          LV_EVENT_VALUE_CHANGED, NULL);
      if (tasks[index].completed)
        {
          lv_obj_add_state(checkbox, LV_STATE_CHECKED);
        }

      delete_button = lv_button_create(row);
      lv_obj_set_size(delete_button, g_compact_layout ? 22 : 30,
                      g_compact_layout ? 22 : 30);
      lv_obj_align(delete_button, LV_ALIGN_RIGHT_MID, 0, 0);
      lv_obj_set_style_bg_color(delete_button, color(COLOR_CORAL), 0);
      lv_obj_set_style_radius(delete_button, 4, 0);
      lv_obj_set_style_shadow_width(delete_button, 0, 0);
      lv_obj_set_style_pad_all(delete_button, 0, 0);
      lv_obj_set_user_data(delete_button, (void *)(uintptr_t)index);
      lv_obj_add_event_cb(delete_button, task_delete_event_cb,
                          LV_EVENT_CLICKED, NULL);

      delete_label = lv_label_create(delete_button);
      lv_label_set_text(delete_label, UI_ICON_TRASH);
      lv_obj_set_style_text_color(delete_label, color(COLOR_WHITE), 0);
      lv_obj_center(delete_label);
    }

  g_task_file_mtime = task_file_mtime();
  g_task_reload_pending = false;
}

/* Append a preset task with the same "[YYYY-MM-DD HH:MM] desc" shape the
 * study-assistant skill writes, so UI-added and agent-added tasks are
 * indistinguishable on disk.
 */

static void task_preset_event_cb(lv_event_t *event)
{
  lv_obj_t *button = lv_event_get_target(event);
  size_t preset_index = (size_t)(uintptr_t)lv_obj_get_user_data(button);
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  time_t now;
  struct tm tm_now;
  char stamp[24] = "";

  note_user_activity();

  if (preset_index >= TASK_PRESETS_COUNT)
    {
      return;
    }

  count = load_tasks(tasks, MAX_TASKS);
  if (count >= MAX_TASKS)
    {
      lv_label_set_text(g_ui.task_file_status, "任务已满，请先删除一些");
      return;
    }

  now = time(NULL);
  if (localtime_r(&now, &tm_now) != NULL)
    {
      strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm_now);
    }

  tasks[count].completed = false;
  if (stamp[0] != '\0')
    {
      snprintf(tasks[count].text, sizeof(tasks[count].text), "[%s] %s",
               stamp, TASK_PRESETS[preset_index].full_text);
    }
  else
    {
      snprintf(tasks[count].text, sizeof(tasks[count].text), "%s",
               TASK_PRESETS[preset_index].full_text);
    }

  if (save_tasks(tasks, count + 1))
    {
      update_task_widgets();
    }
  else
    {
      lv_label_set_text(g_ui.task_file_status, "保存失败");
      g_task_reload_pending = true;
    }
}

#endif /* !_WIN32 */

static void format_uptime(char *buffer, size_t buffer_size, time_t seconds)
{
  long hours = (long)(seconds / 3600);
  long minutes = (long)((seconds % 3600) / 60);
  long secs = (long)(seconds % 60);

  snprintf(buffer, buffer_size, "%02ld:%02ld:%02ld", hours, minutes, secs);
}

static void format_memory(char *buffer, size_t buffer_size,
                          unsigned long long bytes)
{
  if (bytes >= 1024ULL * 1024ULL)
    {
      snprintf(buffer, buffer_size, "%llu MB",
               bytes / (1024ULL * 1024ULL));
    }
  else
    {
      snprintf(buffer, buffer_size, "%llu KB", bytes / 1024ULL);
    }
}

/* The 1 s update timer starts before every screen exists, and not every
 * screen owns every label (the standby screen has no metric cards, the
 * board build has no Win32-only widgets). Setting text on a NULL object
 * dereferences it inside LVGL, so route all timer-driven updates through
 * this guard instead of calling lv_label_set_text() directly.
 */

static void set_label_text(lv_obj_t *label, const char *text)
{
  if (label == NULL || text == NULL)
    {
      return;
    }

  lv_label_set_text(label, text);
}

static void update_system_widgets(void)
{
  static bool status_logged;
#ifdef _WIN32
  SYSTEM_INFO native_info;
  MEMORYSTATUSEX memory_status;
#else
  struct sysinfo system_info;
  struct utsname kernel_info;
  struct timespec monotonic;
#endif
  unsigned long long free_bytes = 0;
  unsigned int cpu_count = 1;
  bool network_online;
  char cpu_text[48];
  char uptime_text[32];
  char memory_text[32];
  char network_text[64];
  char clock_text[16];
  time_t wall_time;
  struct tm local_time;

#ifdef _WIN32
  GetNativeSystemInfo(&native_info);
  cpu_count = native_info.dwNumberOfProcessors > 0 ?
              native_info.dwNumberOfProcessors : 1;

  memset(&memory_status, 0, sizeof(memory_status));
  memory_status.dwLength = sizeof(memory_status);
  if (GlobalMemoryStatusEx(&memory_status))
    {
      free_bytes = memory_status.ullAvailPhys;
      format_memory(memory_text, sizeof(memory_text), free_bytes);
    }
  else
    {
      snprintf(memory_text, sizeof(memory_text), "不可用");
    }

  format_uptime(uptime_text, sizeof(uptime_text),
                (time_t)(GetTickCount64() / 1000ULL));
#  if defined(_M_X64) || defined(__x86_64__)
  snprintf(cpu_text, sizeof(cpu_text), "x64 / %u 核", cpu_count);
#  elif defined(_M_ARM64) || defined(__aarch64__)
  snprintf(cpu_text, sizeof(cpu_text), "ARM64 / %u 核", cpu_count);
#  else
  snprintf(cpu_text, sizeof(cpu_text), "Win / %u 核", cpu_count);
#  endif
#else
  bool system_info_valid;

  system_info_valid = sysinfo(&system_info) == 0;
  if (system_info_valid)
    {
      cpu_count = system_info.procs > 0 ? system_info.procs : 1;
      free_bytes = (unsigned long long)system_info.freeram *
                   system_info.mem_unit;
      format_uptime(uptime_text, sizeof(uptime_text), system_info.uptime);
      format_memory(memory_text, sizeof(memory_text), free_bytes);
    }
  else
    {
      clock_gettime(CLOCK_MONOTONIC, &monotonic);
      format_uptime(uptime_text, sizeof(uptime_text), monotonic.tv_sec);
      snprintf(memory_text, sizeof(memory_text), "不可用");
    }

  if (uname(&kernel_info) == 0)
    {
      snprintf(cpu_text, sizeof(cpu_text), "%s / %u 核",
               kernel_info.machine, cpu_count);
    }
  else
    {
      snprintf(cpu_text, sizeof(cpu_text), "CPU / %u 核", cpu_count);
    }
#endif

  set_label_text(g_ui.cpu_value, cpu_text);
  set_label_text(g_ui.memory_value, memory_text);
  set_label_text(g_ui.uptime_value, uptime_text);
  {
    char home_cpu[24];
    char home_memory[24];
    char home_uptime[16];

    snprintf(home_cpu, sizeof(home_cpu), "%u核", cpu_count);
    if (free_bytes >= 1024ULL * 1024ULL * 1024ULL)
      {
        snprintf(home_memory, sizeof(home_memory), "%lluG",
                 free_bytes / (1024ULL * 1024ULL * 1024ULL));
      }
    else if (free_bytes >= 1024ULL * 1024ULL)
      {
        snprintf(home_memory, sizeof(home_memory), "%lluM",
                 free_bytes / (1024ULL * 1024ULL));
      }
      else
      {
        snprintf(home_memory, sizeof(home_memory), "--");
      }

    /* The home card is narrow, so show "HH:MM" and drop the seconds. The
     * explicit precision also keeps this inside home_uptime[] no matter how
     * many hours format_uptime() produced.
     */

    snprintf(home_uptime, sizeof(home_uptime), "%.5s", uptime_text);

    set_label_text(g_ui.home_cpu, home_cpu);
    set_label_text(g_ui.home_memory, home_memory);
    set_label_text(g_ui.home_uptime, home_uptime);
  }

  /* read_network_address() opens a socket and ai_chat_bridge_agent_running()
   * walks all of /proc opening a file per entry. Doing that once per second
   * churns descriptors hard, and ai_agent's network watcher is already
   * spinning on getifaddrs() whenever WiFi is unconfigured. The two together
   * fault inside NuttX's fd allocation (fs_files.c fdlist_dupfile).
   *
   * COMPLETE DISABLE: To isolate whether UI fd operations are contributing to
   * the race, disable both calls entirely and use hardcoded offline state.
   * If this eliminates the crash, the problem is the combination of UI polling
   * + ai_agent's continuous network watcher. If it still crashes, the problem
   * is solely in ai_agent or elsewhere.
   */

#if 0
  if (g_net_poll_tick == 0)
    {
      g_net_cached_online = read_network_address(g_net_cached_text,
                                                 sizeof(g_net_cached_text));
#ifndef _WIN32
      g_agent_cached_running = ai_chat_bridge_agent_running();
#endif
    }

  g_net_poll_tick = (g_net_poll_tick + 1) % NET_POLL_SECONDS;

  network_online = g_net_cached_online;
  snprintf(network_text, sizeof(network_text), "%s", g_net_cached_text);
#else
  /* Hardcoded offline to eliminate all UI fd operations */
  network_online = false;
  snprintf(network_text, sizeof(network_text), "测试模式");
#ifndef _WIN32
  g_agent_cached_running = false;
#endif
#endif

  if (network_online)
    {
      set_label_text(g_ui.network_value, network_text);
      set_label_text(g_ui.home_network, "在线");
#ifdef _WIN32
      set_label_text(g_ui.ai_connection, "网络正常，MiMo 已就绪");
#else
      set_label_text(g_ui.ai_connection,
                     g_agent_cached_running ?
                     "网络正常，MiMo 已就绪" :
                     "网络正常，但 AI 服务未运行");
#endif
    }
  else
    {
      set_label_text(g_ui.network_value, "离线");
      set_label_text(g_ui.home_network, "离线");
      set_label_text(g_ui.ai_connection, "正在等待网络");
    }

  if (!status_logged)
    {
      printf("study_terminal: system cpu=%s free=%s network=%s uptime=%s\n",
             cpu_text, memory_text,
             network_online ? network_text : "offline", uptime_text);
      status_logged = true;
    }

  wall_time = time(NULL);
#ifdef _WIN32
  localtime_s(&local_time, &wall_time);
#else
  localtime_r(&wall_time, &local_time);
#endif
  strftime(clock_text, sizeof(clock_text), "%H:%M", &local_time);
  set_label_text(g_ui.header_clock, clock_text);
}

static void pulse_scale_cb(void *object, int32_t value)
{
  lv_obj_set_style_transform_scale(object, value, 0);
}

static void pulse_opacity_cb(void *object, int32_t value)
{
  lv_obj_set_style_opa(object, value, 0);
}

static void start_focus_pulse(void)
{
  lv_anim_t animation;

  lv_anim_delete(g_ui.focus_button, pulse_scale_cb);
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, g_ui.focus_button);
  lv_anim_set_exec_cb(&animation, pulse_scale_cb);
  lv_anim_set_values(&animation, 256, 270);
  lv_anim_set_duration(&animation, 850);
  lv_anim_set_playback_duration(&animation, 850);
  lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&animation);
}

static void stop_focus_pulse(void)
{
  lv_anim_delete(g_ui.focus_button, pulse_scale_cb);
  lv_obj_set_style_transform_scale(g_ui.focus_button, 256, 0);
}

static void start_ai_pulse(void)
{
  lv_anim_t animation;

  if (g_ui.ai_pulse == NULL)
    {
      return;
    }

  lv_anim_delete(g_ui.ai_pulse, pulse_opacity_cb);
  lv_obj_set_style_opa(g_ui.ai_pulse, LV_OPA_COVER, 0);
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, g_ui.ai_pulse);
  lv_anim_set_exec_cb(&animation, pulse_opacity_cb);
  lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_40);
  lv_anim_set_duration(&animation, 450);
  lv_anim_set_playback_duration(&animation, 450);
  lv_anim_set_repeat_count(&animation, 1);
  lv_anim_start(&animation);
}

static void focus_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  g_ui.focus_running = !g_ui.focus_running;

  if (g_ui.focus_running)
    {
      lv_label_set_text(g_ui.focus_button_label, UI_ICON_PAUSE);
      lv_label_set_text(g_ui.focus_status, "专注进行中");
      start_focus_pulse();
    }
  else
    {
      lv_label_set_text(g_ui.focus_button_label, UI_ICON_PLAY);
      lv_label_set_text(g_ui.focus_status, "已暂停");
      stop_focus_pulse();
    }
}

static void task_sync_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  update_task_widgets();
  lv_label_set_text(g_ui.task_sync_label, "已同步");
}

static void prepare_voice_ring(lv_obj_t *ring)
{
  int32_t width;
  int32_t height;

  if (ring == NULL)
    {
      return;
    }

  width = lv_obj_get_width(ring);
  height = lv_obj_get_height(ring);
  lv_obj_set_style_transform_pivot_x(ring, width / 2, 0);
  lv_obj_set_style_transform_pivot_y(ring, height / 2, 0);
  lv_obj_set_style_transform_scale(ring, 180, 0);
  lv_obj_set_style_opa(ring, LV_OPA_TRANSP, 0);
}

static void start_voice_ring_anim(lv_obj_t *ring, uint32_t delay_ms)
{
  lv_anim_t scale_anim;
  lv_anim_t fade_anim;

  if (ring == NULL)
    {
      return;
    }

  lv_anim_delete(ring, pulse_scale_cb);
  lv_anim_delete(ring, pulse_opacity_cb);
  prepare_voice_ring(ring);

  lv_anim_init(&scale_anim);
  lv_anim_set_var(&scale_anim, ring);
  lv_anim_set_exec_cb(&scale_anim, pulse_scale_cb);
  lv_anim_set_values(&scale_anim, 180, 310);
  lv_anim_set_delay(&scale_anim, delay_ms);
  lv_anim_set_duration(&scale_anim, 1400);
  lv_anim_set_repeat_count(&scale_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_repeat_delay(&scale_anim, 200);
  lv_anim_set_path_cb(&scale_anim, lv_anim_path_ease_out);
  lv_anim_start(&scale_anim);

  lv_anim_init(&fade_anim);
  lv_anim_set_var(&fade_anim, ring);
  lv_anim_set_exec_cb(&fade_anim, pulse_opacity_cb);
  lv_anim_set_values(&fade_anim, LV_OPA_70, LV_OPA_TRANSP);
  lv_anim_set_delay(&fade_anim, delay_ms);
  lv_anim_set_duration(&fade_anim, 1400);
  lv_anim_set_repeat_count(&fade_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_repeat_delay(&fade_anim, 200);
  lv_anim_set_path_cb(&fade_anim, lv_anim_path_ease_out);
  lv_anim_start(&fade_anim);
}

static void start_voice_wave(void)
{
  start_voice_ring_anim(g_ui.voice_ring_inner, 0);
  start_voice_ring_anim(g_ui.voice_ring_mid, 280);
  start_voice_ring_anim(g_ui.voice_ring_outer, 560);
}

static void stop_voice_wave(void)
{
  if (g_ui.voice_ring_outer != NULL)
    {
      lv_anim_delete(g_ui.voice_ring_outer, pulse_scale_cb);
      lv_anim_delete(g_ui.voice_ring_outer, pulse_opacity_cb);
      prepare_voice_ring(g_ui.voice_ring_outer);
    }
  if (g_ui.voice_ring_mid != NULL)
    {
      lv_anim_delete(g_ui.voice_ring_mid, pulse_scale_cb);
      lv_anim_delete(g_ui.voice_ring_mid, pulse_opacity_cb);
      prepare_voice_ring(g_ui.voice_ring_mid);
    }
  if (g_ui.voice_ring_inner != NULL)
    {
      lv_anim_delete(g_ui.voice_ring_inner, pulse_scale_cb);
      lv_anim_delete(g_ui.voice_ring_inner, pulse_opacity_cb);
      prepare_voice_ring(g_ui.voice_ring_inner);
    }
}

static void show_voice_ui(void)
{
  if (g_ui.voice_screen == NULL)
    {
      return;
    }

  note_user_activity();
  g_ui.voice_active = true;
  g_ui.voice_phase = 0;
  g_ui.on_standby = false;
  update_voice_widgets();

#ifdef _WIN32
  /* The preview animates continuously to demonstrate the rings. On the board
   * the rings only pulse while the microphone is actually open.
   */

  start_voice_wave();
#endif

  lv_screen_load_anim(g_ui.voice_screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0,
                      false);
}

static void hide_voice_ui(void)
{
  g_ui.voice_active = false;
  g_ui.voice_phase = 0;

#ifndef _WIN32
  /* Leaving while the microphone is open would strand the recorder, so close
   * it out and discard the transcript.
   */

  if (g_ui.is_recording)
    {
      char discard[8];

      voice_ui_bridge_stop_recording(discard, sizeof(discard));
      g_ui.is_recording = false;
    }
#endif

  stop_voice_wave();
  note_user_activity();
  show_main_ui();
  if (g_ui.tabview != NULL)
    {
      lv_tabview_set_active(g_ui.tabview, 3, LV_ANIM_OFF);
    }
}

#ifdef _WIN32

/* Preview build: there is no microphone or agent behind the window, so the
 * voice screen replays a fixed four-step script to show the visual states.
 */

static void update_voice_widgets(void)
{
  static const char *heard_steps[] = {
    "识别中...",
    "帮我总结今天的学习重点",
    "帮我总结今天的学习重点",
    "帮我总结今天的学习重点"
  };
  static const char *reply_steps[] = {
    "AI 正在聆听...",
    "AI 正在思考...",
    "先复习 openvela 启动流程。",
    "先复习 openvela 启动流程。"
  };
  static const char *status_steps[] = {
    "正在聆听",
    "识别完成",
    "生成回答",
    "回答完成"
  };
  uint32_t index = g_ui.voice_phase;

  if (g_ui.voice_status == NULL)
    {
      return;
    }

  if (index > 3)
    {
      index = 3;
    }

  lv_label_set_text(g_ui.voice_status, status_steps[index]);
  lv_label_set_text(g_ui.voice_heard, heard_steps[index]);
  lv_label_set_text(g_ui.voice_reply, reply_steps[index]);
}

#else

/* Board build: the same widgets are driven by real state. The script is gone
 * and the labels only change when recording, ASR, the agent or TTS actually
 * change something, so nothing on screen claims progress that did not happen.
 */

static void update_voice_widgets(void)
{
  if (g_ui.voice_status == NULL)
    {
      return;
    }

  if (g_ui.is_recording)
    {
      lv_label_set_text(g_ui.voice_status, "正在聆听");
    }
  else if (ai_chat_bridge_is_busy())
    {
      lv_label_set_text(g_ui.voice_status, "生成回答");
    }
  else if (voice_ui_bridge_is_speaking())
    {
      lv_label_set_text(g_ui.voice_status, "正在播报");
    }
  else if (!ai_chat_bridge_agent_running())
    {
      lv_label_set_text(g_ui.voice_status, "AI 服务未运行");
    }
  else
    {
      lv_label_set_text(g_ui.voice_status, "按住下方按钮说话");
    }
}

#endif /* _WIN32 */

static void voice_close_event_cb(lv_event_t *event)
{
  (void)event;
  hide_voice_ui();
}

#ifndef _WIN32

/* Push-to-talk. voice_ui_bridge owns the recorder and the ASR call; the
 * transcript then goes to ai_agent over the message bus and the reply comes
 * back through ai_chat_bridge's tap, drained by ai_reply_timer_cb().
 */

static void voice_ptt_pressed_cb(lv_event_t *event)
{
  (void)event;

  note_user_activity();

  if (g_ui.is_recording)
    {
      return;
    }

  /* One turn at a time: recording over an unanswered question would leave two
   * requests racing on the same channel.
   */

  if (ai_chat_bridge_is_busy())
    {
      lv_label_set_text(g_ui.voice_status, "正在等待上一条回复...");
      return;
    }

  if (voice_ui_bridge_start_recording() < 0)
    {
      lv_label_set_text(g_ui.voice_status, "麦克风启动失败");
      return;
    }

  g_ui.is_recording = true;
  lv_label_set_text(g_ui.voice_ptt_label, "松手结束");
  lv_obj_set_style_bg_color(g_ui.voice_ptt_button, color(COLOR_CORAL), 0);
  lv_label_set_text(g_ui.voice_status, "正在聆听");
  lv_label_set_text(g_ui.voice_heard, "识别中...");
  start_voice_wave();
}

static void voice_ptt_released_cb(lv_event_t *event)
{
  char text[256] = {0};
  int ret;

  (void)event;

  if (!g_ui.is_recording)
    {
      return;
    }

  lv_label_set_text(g_ui.voice_status, "识别中");
  ret = voice_ui_bridge_stop_recording(text, sizeof(text));
  g_ui.is_recording = false;
  stop_voice_wave();

  lv_label_set_text(g_ui.voice_ptt_label, "按住说话");
  lv_obj_set_style_bg_color(g_ui.voice_ptt_button, color(COLOR_OPENVELA), 0);

  if (ret < 0)
    {
      lv_label_set_text(g_ui.voice_status, "识别失败");
      lv_label_set_text(g_ui.voice_heard, "没听清，请再说一次");
      return;
    }

  if (text[0] == '\0')
    {
      lv_label_set_text(g_ui.voice_status, "未识别到内容");
      lv_label_set_text(g_ui.voice_heard, "没听到声音");
      return;
    }

  lv_label_set_text(g_ui.voice_heard, text);

  if (!ai_chat_bridge_agent_running())
    {
      lv_label_set_text(g_ui.voice_status, "AI 服务未运行");
      lv_label_set_text(g_ui.voice_reply,
                        "AI 服务未启动，请在终端执行 ai_agent &");
      return;
    }

  if (ai_chat_bridge_send(text) != 0)
    {
      lv_label_set_text(g_ui.voice_status, "发送失败");
      lv_label_set_text(g_ui.voice_reply, "发送失败，请重试");
      return;
    }

  lv_label_set_text(g_ui.voice_status, "生成回答");
  lv_label_set_text(g_ui.voice_reply, "AI 正在思考...");
}

#endif /* !_WIN32 */

static lv_obj_t *create_chat_bubble(lv_obj_t *parent, bool from_user,
                                    const char *name, const char *text,
                                    lv_obj_t **text_label)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *bubble = lv_obj_create(row);
  lv_obj_t *name_label;
  lv_obj_t *body;

  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(row,
                        from_user ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  object_reset(bubble);
  lv_obj_set_width(bubble, g_compact_layout ? 300 : 340);
  lv_obj_set_height(bubble, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bubble, 14, 0);
  lv_obj_set_style_pad_left(bubble, 10, 0);
  lv_obj_set_style_pad_right(bubble, 10, 0);
  lv_obj_set_style_pad_top(bubble, 6, 0);
  lv_obj_set_style_pad_bottom(bubble, 6, 0);
  lv_obj_set_style_pad_row(bubble, 3, 0);
  lv_obj_set_layout(bubble, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
  if (from_user)
    {
      lv_obj_set_style_bg_color(bubble, color(COLOR_OPENVELA), 0);
      lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(bubble, 0, 0);
    }
  else
    {
      lv_obj_set_style_bg_color(bubble, color(COLOR_SURFACE), 0);
      lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
      lv_obj_set_style_border_color(bubble, color(COLOR_BORDER), 0);
      lv_obj_set_style_border_width(bubble, 1, 0);
    }

  name_label = create_label(bubble, name, ui_font(),
                            from_user ? 0xd7e6ff : COLOR_MUTED);
  lv_obj_set_height(name_label, 16);
  body = create_label(bubble, text, ui_font(),
                      from_user ? COLOR_WHITE : COLOR_TEXT);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(body, LV_PCT(100));
  *text_label = body;
  return row;
}

static void create_voice_screen(void)
{
  lv_obj_t *center;
  lv_obj_t *stage;
  lv_obj_t *chat;
  lv_obj_t *close_button;
  lv_obj_t *title;

  g_ui.voice_screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.voice_screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_ui.voice_screen, color(0x071522), 0);
  lv_obj_set_style_bg_opa(g_ui.voice_screen, LV_OPA_COVER, 0);
  lv_obj_set_layout(g_ui.voice_screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_ui.voice_screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_left(g_ui.voice_screen, 10, 0);
  lv_obj_set_style_pad_right(g_ui.voice_screen, 10, 0);
  lv_obj_set_style_pad_top(g_ui.voice_screen, 8, 0);
  lv_obj_set_style_pad_bottom(g_ui.voice_screen, 8, 0);
  lv_obj_set_style_pad_row(g_ui.voice_screen, 6, 0);
  lv_obj_add_event_cb(g_ui.voice_screen, main_input_event_cb, LV_EVENT_PRESSED,
                      NULL);

  create_wallpaper(g_ui.voice_screen);

  title = create_label(g_ui.voice_screen, "AI 语音助手", ui_font(),
                       COLOR_WHITE);
  lv_obj_set_width(title, LV_PCT(100));
  lv_obj_set_height(title, 20);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  g_ui.voice_status = create_label(g_ui.voice_screen, "正在聆听", ui_font(),
                                   0x75a0ff);
  lv_obj_set_width(g_ui.voice_status, LV_PCT(100));
  lv_obj_set_height(g_ui.voice_status, 20);
  lv_obj_set_style_text_align(g_ui.voice_status, LV_TEXT_ALIGN_CENTER, 0);

  center = create_group(g_ui.voice_screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(center, LV_PCT(100));
  lv_obj_set_height(center, 1);
  lv_obj_set_flex_grow(center, 1);
  lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  stage = create_group(center, LV_FLEX_FLOW_COLUMN);
  object_reset(stage);
  lv_obj_set_size(stage, 168, 168);
  lv_obj_set_style_pad_all(stage, 0, 0);

  g_ui.voice_ring_outer = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_outer);
  lv_obj_add_flag(g_ui.voice_ring_outer, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_outer, 150, 150);
  lv_obj_center(g_ui.voice_ring_outer);
  lv_obj_set_style_radius(g_ui.voice_ring_outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_outer, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_outer, color(0x3d6fff), 0);
  prepare_voice_ring(g_ui.voice_ring_outer);

  g_ui.voice_ring_mid = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_mid);
  lv_obj_add_flag(g_ui.voice_ring_mid, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_mid, 150, 150);
  lv_obj_center(g_ui.voice_ring_mid);
  lv_obj_set_style_radius(g_ui.voice_ring_mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_mid, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_mid, color(0x75a0ff), 0);
  prepare_voice_ring(g_ui.voice_ring_mid);

  g_ui.voice_ring_inner = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_inner);
  lv_obj_add_flag(g_ui.voice_ring_inner, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_inner, 150, 150);
  lv_obj_center(g_ui.voice_ring_inner);
  lv_obj_set_style_radius(g_ui.voice_ring_inner, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_inner, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_inner, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_inner, color(0xa8c4ff), 0);
  prepare_voice_ring(g_ui.voice_ring_inner);

  g_ui.voice_avatar = lv_obj_create(stage);
  object_reset(g_ui.voice_avatar);
  lv_obj_add_flag(g_ui.voice_avatar, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_avatar, 72, 72);
  lv_obj_center(g_ui.voice_avatar);
  lv_obj_set_style_radius(g_ui.voice_avatar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_ui.voice_avatar, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_bg_opa(g_ui.voice_avatar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_ui.voice_avatar, 0, 0);
  lv_obj_set_style_shadow_color(g_ui.voice_avatar, color(COLOR_OPENVELA), 0);
  lv_obj_set_style_shadow_opa(g_ui.voice_avatar, LV_OPA_40, 0);
  lv_obj_set_style_shadow_width(g_ui.voice_avatar, 18, 0);
  {
    lv_obj_t *avatar_text = create_label(g_ui.voice_avatar, "AI",
                                         &lv_font_montserrat_20, COLOR_WHITE);
    lv_obj_center(avatar_text);
  }

  chat = create_group(g_ui.voice_screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(chat, LV_PCT(100));
  lv_obj_set_height(chat, g_compact_layout ? 128 : 150);
  lv_obj_set_style_pad_row(chat, 8, 0);
  create_chat_bubble(chat, true, "你", "识别中...", &g_ui.voice_heard);
  create_chat_bubble(chat, false, "AI", "AI 正在聆听...", &g_ui.voice_reply);

#ifndef _WIN32

  /* The board needs a way to actually talk, so the footer carries the
   * push-to-talk button and the close button side by side. The design mockup
   * only had "结束并返回" because the preview had nothing to record.
   */

  {
    lv_obj_t *footer = create_group(g_ui.voice_screen, LV_FLEX_FLOW_ROW);

    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_height(footer, g_compact_layout ? 34 : 42);
    lv_obj_set_style_pad_column(footer, 8, 0);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    g_ui.voice_ptt_button = lv_button_create(footer);
    lv_obj_set_width(g_ui.voice_ptt_button, 1);
    lv_obj_set_flex_grow(g_ui.voice_ptt_button, 2);
    lv_obj_set_height(g_ui.voice_ptt_button, LV_PCT(100));
    lv_obj_set_style_radius(g_ui.voice_ptt_button, 10, 0);
    lv_obj_set_style_bg_color(g_ui.voice_ptt_button, color(COLOR_OPENVELA), 0);
    lv_obj_set_style_shadow_width(g_ui.voice_ptt_button, 0, 0);
    lv_obj_add_event_cb(g_ui.voice_ptt_button, voice_ptt_pressed_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ui.voice_ptt_button, voice_ptt_released_cb,
                        LV_EVENT_RELEASED, NULL);

    /* PRESS_LOST fires when the finger slides off the button; without it the
     * recorder would stay running with no way to stop it.
     */

    lv_obj_add_event_cb(g_ui.voice_ptt_button, voice_ptt_released_cb,
                        LV_EVENT_PRESS_LOST, NULL);
    g_ui.voice_ptt_label = create_label(g_ui.voice_ptt_button, "按住说话",
                                       ui_font(), COLOR_WHITE);
    lv_obj_center(g_ui.voice_ptt_label);

    close_button = lv_button_create(footer);
    lv_obj_set_width(close_button, 1);
    lv_obj_set_flex_grow(close_button, 1);
    lv_obj_set_height(close_button, LV_PCT(100));
    lv_obj_set_style_radius(close_button, 10, 0);
    lv_obj_set_style_bg_color(close_button, color(0x17314f), 0);
    lv_obj_set_style_shadow_width(close_button, 0, 0);
    lv_obj_add_event_cb(close_button, voice_close_event_cb, LV_EVENT_CLICKED,
                        NULL);
    g_ui.voice_close_label = create_label(close_button, "返回", ui_font(),
                                          COLOR_WHITE);
    lv_obj_center(g_ui.voice_close_label);
  }

  g_ui.is_recording = false;

#else

  close_button = lv_button_create(g_ui.voice_screen);
  lv_obj_set_size(close_button, LV_PCT(100), g_compact_layout ? 32 : 40);
  lv_obj_set_style_radius(close_button, 10, 0);
  lv_obj_set_style_bg_color(close_button, color(0x17314f), 0);
  lv_obj_set_style_shadow_width(close_button, 0, 0);
  lv_obj_add_event_cb(close_button, voice_close_event_cb, LV_EVENT_CLICKED,
                      NULL);
  g_ui.voice_close_label = create_label(close_button, "结束并返回", ui_font(),
                                        COLOR_WHITE);
  lv_obj_center(g_ui.voice_close_label);

#endif
}

static void ai_check_event_cb(lv_event_t *event)
{
  char network_text[64];

  (void)event;
  note_user_activity();
  start_ai_pulse();
  lv_label_set_text(g_ui.ai_check_label, UI_ICON_AUDIO);
  lv_obj_center(g_ui.ai_check_label);
  if (read_network_address(network_text, sizeof(network_text)))
    {
#ifdef _WIN32
      lv_label_set_text(g_ui.ai_connection, "网络正常，MiMo 已就绪");
#else
      /* Only claim the backend is ready when the process that owns it is
       * actually running. Otherwise the status line would say "已就绪" while
       * every question silently goes nowhere.
       */

      lv_label_set_text(g_ui.ai_connection,
                        ai_chat_bridge_agent_running() ?
                        "网络正常，MiMo 已就绪" :
                        "网络正常，但 AI 服务未运行");
#endif
    }
  else
    {
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
    }
  show_voice_ui();
}

static void note_user_activity(void)
{
  g_ui.idle_seconds = 0;
}

static void show_main_ui(void)
{
  if (g_ui.screen == NULL)
    {
      return;
    }

  g_ui.on_standby = false;
  g_ui.idle_seconds = 0;
  lv_screen_load_anim(g_ui.screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, false);
}

static void show_standby_ui(void)
{
  if (g_ui.standby_screen == NULL)
    {
      return;
    }

  if (g_ui.voice_active)
    {
      g_ui.voice_active = false;
      stop_voice_wave();
    }

  g_ui.on_standby = true;
  g_ui.idle_seconds = 0;
  lv_screen_load_anim(g_ui.standby_screen, LV_SCR_LOAD_ANIM_FADE_IN, 220, 0,
                      false);
}

static void standby_touch_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  show_main_ui();
}

static void main_input_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
}

static void update_standby_widgets(void)
{
  time_t wall_time;
  struct tm local_time;
  char time_text[16];
  char date_text[48];
  const char *weather_text;
  const char *temp_text;

  if (g_ui.standby_time == NULL)
    {
      return;
    }

  wall_time = time(NULL);
#ifdef _WIN32
  localtime_s(&local_time, &wall_time);
#else
  localtime_r(&wall_time, &local_time);
#endif
  strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  {
    static const char *weekdays[] = {
      "周日", "周一", "周二", "周三", "周四", "周五", "周六"
    };
    snprintf(date_text, sizeof(date_text), "%02d-%02d  %s",
             local_time.tm_mon + 1, local_time.tm_mday,
             weekdays[local_time.tm_wday % 7]);
  }

  weather_text = "晴转多云";
  temp_text = "26C";
  if (local_time.tm_hour >= 18 || local_time.tm_hour < 6)
    {
      weather_text = "夜间晴朗";
      temp_text = "21C";
    }

  set_label_text(g_ui.standby_time, time_text);
  set_label_text(g_ui.standby_date, date_text);
  set_label_text(g_ui.standby_weather, weather_text);
  set_label_text(g_ui.standby_temp, temp_text);
}

static void create_standby_screen(void)
{
  lv_obj_t *panel;
  lv_obj_t *weather_row;
  lv_obj_t *badge;
  lv_obj_t *title;

  g_ui.standby_screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.standby_screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_ui.standby_screen, color(0x071522), 0);
  lv_obj_set_style_bg_opa(g_ui.standby_screen, LV_OPA_COVER, 0);
  lv_obj_add_flag(g_ui.standby_screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_ui.standby_screen, standby_touch_event_cb,
                      LV_EVENT_PRESSED, NULL);

  create_wallpaper(g_ui.standby_screen);

  panel = lv_obj_create(g_ui.standby_screen);
  object_reset(panel);
  lv_obj_set_size(panel, g_compact_layout ? 420 : 440,
                  g_compact_layout ? 250 : 270);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, color(0x0a1b2f), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
  lv_obj_set_style_radius(panel, 16, 0);
  lv_obj_set_style_border_color(panel, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_20, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_pad_left(panel, g_compact_layout ? 18 : 28, 0);
  lv_obj_set_style_pad_right(panel, g_compact_layout ? 18 : 28, 0);
  lv_obj_set_style_pad_top(panel, g_compact_layout ? 14 : 22, 0);
  lv_obj_set_style_pad_bottom(panel, g_compact_layout ? 14 : 22, 0);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, 0, 0);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);

  title = create_label(panel, "openvela 学习终端", ui_font(), 0x9eb0c2);
  lv_obj_set_height(title, 22);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  g_ui.standby_time = create_label(panel, "--:--", &lv_font_montserrat_32,
                                   COLOR_WHITE);
  lv_obj_set_width(g_ui.standby_time, LV_PCT(100));
  lv_obj_set_height(g_ui.standby_time, 48);
  lv_obj_set_style_text_align(g_ui.standby_time, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(g_ui.standby_time, 2, 0);
  lv_obj_set_style_pad_top(g_ui.standby_time, 4, 0);
  lv_obj_set_style_pad_bottom(g_ui.standby_time, 4, 0);

  g_ui.standby_date = create_label(panel, "--", ui_font(), 0xb7c7d8);
  lv_obj_set_width(g_ui.standby_date, LV_PCT(100));
  lv_obj_set_height(g_ui.standby_date, 24);
  lv_obj_set_style_text_align(g_ui.standby_date, LV_TEXT_ALIGN_CENTER, 0);

  weather_row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(weather_row, LV_PCT(100));
  lv_obj_set_height(weather_row, 36);
  lv_obj_set_style_pad_column(weather_row, 12, 0);
  lv_obj_set_flex_align(weather_row, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  badge = create_icon_badge(weather_row, UI_ICON_REFRESH, COLOR_BLUE, 26);
  (void)badge;
  g_ui.standby_weather = create_label(weather_row, "晴转多云", ui_font(),
                                      COLOR_WHITE);
  lv_obj_set_height(g_ui.standby_weather, 24);
  g_ui.standby_temp = create_label(weather_row, "26C",
                                   &lv_font_montserrat_20, COLOR_BLUE);
  lv_obj_set_height(g_ui.standby_temp, 28);

  g_ui.standby_hint = create_label(panel, "触摸屏幕进入主页", ui_font(),
                                   0x75a0ff);
  lv_obj_set_width(g_ui.standby_hint, LV_PCT(100));
  lv_obj_set_height(g_ui.standby_hint, 22);
  lv_obj_set_style_text_align(g_ui.standby_hint, LV_TEXT_ALIGN_CENTER, 0);
  update_standby_widgets();
}

static void update_timer_cb(lv_timer_t *timer)
{
  static uint8_t task_update_tick;
  uint32_t progress;
  uint32_t remaining;
  lv_indev_t *indev;

  (void)timer;
  update_system_widgets();
  update_standby_widgets();

  indev = lv_indev_get_next(NULL);
  while (indev != NULL)
    {
      if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER &&
          lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED)
        {
          note_user_activity();
          break;
        }
      indev = lv_indev_get_next(indev);
    }

  if (g_ui.voice_active)
    {
#ifdef _WIN32
      /* Preview: walk the script and return to the main screen when it ends. */

      g_ui.voice_phase++;
      update_voice_widgets();
      if (g_ui.voice_phase > 8)
        {
          hide_voice_ui();
        }
#else
      /* Board: the screen stays until the user leaves it, and the labels
       * follow real recorder/agent/TTS state. Auto-closing here would cut off
       * a reply that is still being spoken.
       */

      update_voice_widgets();
#endif
    }
  else if (!g_ui.on_standby)
    {
      g_ui.idle_seconds++;
      if (g_ui.idle_seconds >= STANDBY_IDLE_SECONDS)
        {
          show_standby_ui();
        }
    }

  if (g_ui.focus_running)
    {
      g_ui.focus_elapsed++;
      if (g_ui.focus_elapsed >= FOCUS_SECONDS)
        {
          g_ui.focus_elapsed = FOCUS_SECONDS;
          g_ui.focus_running = false;
          set_label_text(g_ui.focus_status, "本轮专注完成，请稍作休息");
          set_label_text(g_ui.focus_button_label, UI_ICON_PLAY);
          stop_focus_pulse();
        }

      progress = g_ui.focus_elapsed * 100 / FOCUS_SECONDS;
      if (g_ui.focus_bar != NULL)
        {
          lv_arc_set_value(g_ui.focus_bar, (int32_t)progress);
        }
    }

  remaining = FOCUS_SECONDS - g_ui.focus_elapsed;
  if (g_ui.focus_time != NULL)
    {
      char focus_text[16];

      snprintf(focus_text, sizeof(focus_text), "%02lu:%02lu",
               (unsigned long)(remaining / 60),
               (unsigned long)(remaining % 60));
      set_label_text(g_ui.focus_time, focus_text);
    }

  task_update_tick++;
  if (task_update_tick >= 5)
    {
      task_update_tick = 0;
      update_task_widgets();
    }
}

#ifndef _WIN32

/* Drain replies produced by ai_agent. The bridge's tap runs on ai_agent's
 * dispatch thread and only queues text, so every LVGL call stays here on the
 * UI task.
 */

static void ai_reply_timer_cb(lv_timer_t *timer)
{
  char text[AI_CHAT_TEXT_MAX];
  bool interim;
  bool got_final = false;

  (void)timer;

  while (ai_chat_bridge_poll(text, sizeof(text), &interim))
    {
      if (g_ui.voice_reply != NULL)
        {
          lv_label_set_text(g_ui.voice_reply, text);
        }

      if (interim)
        {
          /* A progress note ("稍等，处理中...") — keep waiting for the real
           * answer and do not speak it.
           */

          if (g_ui.voice_status != NULL)
            {
              lv_label_set_text(g_ui.voice_status, "生成回答");
            }

          continue;
        }

      got_final = true;

      /* Speak the answer. The bridge queues it on a worker thread, so this
       * returns immediately and the UI keeps redrawing.
       */

      voice_ui_bridge_speak(text);
    }

  if (got_final && g_ui.voice_status != NULL)
    {
      lv_label_set_text(g_ui.voice_status, "回答完成");
    }

  /* Surface a stalled request instead of leaving the label stuck on
   * "生成回答" forever. ai_chat_bridge clears busy after its timeout.
   */

  if (!got_final && !ai_chat_bridge_is_busy() && g_ui.voice_reply != NULL &&
      g_ui.voice_status != NULL &&
      strcmp(lv_label_get_text(g_ui.voice_status), "生成回答") == 0)
    {
      lv_label_set_text(g_ui.voice_status, "未收到回复");
      lv_label_set_text(g_ui.voice_reply, "没有收到回复，请重试");
    }
}

/* Rebuild the task rows only when the file actually changed, so a tap is not
 * swallowed by a rebuild mid-interaction.
 */

static void task_watch_timer_cb(lv_timer_t *timer)
{
  (void)timer;

  if (g_task_reload_pending || task_file_mtime() != g_task_file_mtime)
    {
      update_task_widgets();
    }
}

#endif /* !_WIN32 */

static void configure_page(lv_obj_t *page)
{
  lv_obj_set_style_bg_color(page, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_20, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, g_compact_layout ? 4 : 20, 0);
  lv_obj_set_style_pad_row(page, g_compact_layout ? 4 : 14, 0);
  lv_obj_set_layout(page, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
}

static lv_obj_t *create_icon_badge(lv_obj_t *parent, const char *symbol,
                                   uint32_t accent, int32_t size)
{
  lv_obj_t *badge = lv_obj_create(parent);
  lv_obj_t *icon;

  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(badge, size, size);
  lv_obj_set_style_bg_color(badge, color(accent), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_20, 0);
  lv_obj_set_style_border_width(badge, 0, 0);
  lv_obj_set_style_radius(badge, 6, 0);
  lv_obj_set_style_pad_all(badge, 0, 0);

  icon = create_label(badge, symbol, &lv_font_montserrat_16, accent);
  lv_obj_center(icon);
  return badge;
}

static void create_page_heading(lv_obj_t *page, const char *symbol,
                                uint32_t accent, const char *title,
                                const char *subtitle)
{
  lv_obj_t *heading = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_t *copy = create_group(heading, LV_FLEX_FLOW_COLUMN);

  lv_obj_set_size(heading, LV_PCT(100), g_compact_layout ? 30 : 52);
  lv_obj_set_style_pad_column(heading, 8, 0);
  lv_obj_set_flex_align(heading, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_badge(heading, symbol, accent, g_compact_layout ? 28 : 38);

  lv_obj_set_width(copy, 1);
  lv_obj_set_height(copy, LV_PCT(100));
  lv_obj_set_flex_grow(copy, 1);
  lv_obj_set_style_pad_row(copy, 0, 0);
  lv_obj_set_flex_align(copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(copy, title, ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(copy, subtitle, ui_font(), COLOR_MUTED);
    }
}

static void create_header(lv_obj_t *parent)
{
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_t *left;
  lv_obj_t *right;
  lv_obj_t *brand;

  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, g_compact_layout ? 42 : 70);
  lv_obj_set_style_bg_color(header, color(0x0a1b2f), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_60, 0);
  lv_obj_set_style_border_color(header, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(header, LV_OPA_10, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_left(header, g_compact_layout ? 8 : 24, 0);
  lv_obj_set_style_pad_right(header, g_compact_layout ? 8 : 24, 0);
  lv_obj_set_style_pad_top(header, g_compact_layout ? 5 : 12, 0);
  lv_obj_set_style_pad_bottom(header, g_compact_layout ? 5 : 12, 0);
  lv_obj_set_layout(header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  left = create_group(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(left, LV_PCT(68), LV_PCT(100));
  lv_obj_set_style_pad_column(left, g_compact_layout ? 6 : 8, 0);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  brand = create_icon_badge(left, "AI", COLOR_BLUE,
                            g_compact_layout ? 26 : 42);
  lv_obj_set_style_text_font(lv_obj_get_child(brand, 0),
                             &lv_font_montserrat_12, 0);
  create_label(left, g_compact_layout ? "学习终端" : "AI 智能学习终端",
               ui_font(), COLOR_WHITE);

  right = create_group(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_height(right, g_compact_layout ? 30 : 36);
  lv_obj_set_width(right, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(right, color(0x071a2e), 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_60, 0);
  lv_obj_set_style_border_width(right, 0, 0);
  lv_obj_set_style_radius(right, 15, 0);
  lv_obj_set_style_pad_left(right, g_compact_layout ? 9 : 12, 0);
  lv_obj_set_style_pad_right(right, g_compact_layout ? 9 : 12, 0);
  lv_obj_set_style_pad_column(right, g_compact_layout ? 6 : 9, 0);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  create_label(right, UI_ICON_WIFI, &lv_font_montserrat_14, COLOR_GREEN);
  create_label(right, "在线", ui_font(), COLOR_GREEN);
  g_ui.header_clock = create_label(right, "--:--", &lv_font_montserrat_12,
                                   COLOR_WHITE);
}

static void create_dark_status_row(lv_obj_t *parent, const char *symbol,
                                   const char *title, const char *value,
                                   lv_obj_t **value_label)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *title_label;

  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 26 : 28);
  lv_obj_set_style_border_color(row, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(row, LV_OPA_10, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_column(row, g_compact_layout ? 6 : 8, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (!g_compact_layout)
    {
      create_label(row, symbol, &lv_font_montserrat_12, 0xb7c7d8);
    }

  title_label = create_label(row, title, ui_font(), 0xb7c7d8);
  lv_obj_set_flex_grow(title_label, 1);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title_label, 1);

  *value_label = create_label(row, value, ui_font(), COLOR_WHITE);
  lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(*value_label, g_compact_layout ? 72 : 96);
  lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_RIGHT, 0);
}

static void create_home_page(lv_obj_t *page)
{
  static int32_t columns[] = {
    LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST
  };
  static int32_t rows[] = {LV_GRID_FR(1), 48, LV_GRID_TEMPLATE_LAST};
  lv_obj_t *hero;
  lv_obj_t *hero_copy;
  lv_obj_t *timer_box;
  lv_obj_t *button;
  lv_obj_t *rail;
  lv_obj_t *rail_header;
  lv_obj_t *task_strip;
  lv_obj_t *task_copy;
  lv_obj_t *task_icon;
  lv_obj_t *task_progress;

  configure_page(page);
  lv_obj_set_layout(page, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(page, columns, rows);
  lv_obj_set_style_pad_all(page, 8, 0);
  lv_obj_set_style_pad_row(page, 8, 0);
  lv_obj_set_style_pad_column(page, 8, 0);

  hero = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_grid_cell(hero, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_layout(hero, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(hero, 12, 0);
  lv_obj_set_style_pad_column(hero, 8, 0);

  hero_copy = create_group(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(hero_copy, 1);
  lv_obj_set_height(hero_copy, LV_PCT(100));
  lv_obj_set_flex_grow(hero_copy, 1);
  lv_obj_set_style_pad_row(hero_copy, g_compact_layout ? 3 : 5, 0);
  lv_obj_set_flex_align(hero_copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(hero_copy, "今日专注", ui_font(), COLOR_BLUE);
  create_label(hero_copy, "阅读 ai_agent 架构", ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(hero_copy, "一次只完成一件事", ui_font(), COLOR_MUTED);
    }
  g_ui.focus_status = create_label(hero_copy, "准备就绪", ui_font(),
                                   COLOR_MUTED);
  lv_label_set_long_mode(g_ui.focus_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.focus_status, LV_PCT(100));

  timer_box = create_group(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(timer_box, 104, LV_PCT(100));
  lv_obj_set_flex_align(timer_box, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  g_ui.focus_bar = lv_arc_create(timer_box);
  lv_obj_set_size(g_ui.focus_bar, 88, 88);
  lv_obj_remove_flag(g_ui.focus_bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_style(g_ui.focus_bar, NULL, LV_PART_KNOB);
  lv_arc_set_rotation(g_ui.focus_bar, 270);
  lv_arc_set_bg_angles(g_ui.focus_bar, 0, 360);
  lv_arc_set_range(g_ui.focus_bar, 0, 100);
  lv_arc_set_value(g_ui.focus_bar, 18);
  lv_obj_set_style_arc_width(g_ui.focus_bar, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_ui.focus_bar, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_ui.focus_bar, color(0xd3deea), LV_PART_MAIN);
  lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE),
                             LV_PART_INDICATOR);

  g_ui.focus_time = create_label(g_ui.focus_bar, "20:30",
                                 &lv_font_montserrat_20, COLOR_TEXT);
  lv_obj_center(g_ui.focus_time);

  button = lv_button_create(timer_box);
  g_ui.focus_button = button;
  lv_obj_set_size(button, 38, 38);
  lv_obj_set_style_radius(button, 19, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
  lv_obj_set_style_shadow_width(button, 12, 0);
  lv_obj_add_flag(button, LV_OBJ_FLAG_FLOATING);
  lv_obj_add_event_cb(button, focus_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align_to(button, g_ui.focus_bar, LV_ALIGN_BOTTOM_RIGHT, 4, 3);
  g_ui.focus_button_label = create_label(button, UI_ICON_PLAY,
                                         &lv_font_montserrat_14,
                                         COLOR_WHITE);
  lv_obj_center(g_ui.focus_button_label);

  rail = lv_obj_create(page);
  lv_obj_remove_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(rail, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_bg_color(rail, color(0x0a1b2f), 0);
  lv_obj_set_style_bg_opa(rail, LV_OPA_80, 0);
  lv_obj_set_style_border_color(rail, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(rail, LV_OPA_10, 0);
  lv_obj_set_style_border_width(rail, 1, 0);
  lv_obj_set_style_radius(rail, 8, 0);
  lv_obj_set_style_pad_left(rail, g_compact_layout ? 8 : 9, 0);
  lv_obj_set_style_pad_right(rail, g_compact_layout ? 8 : 9, 0);
  lv_obj_set_style_pad_top(rail, g_compact_layout ? 6 : 9, 0);
  lv_obj_set_style_pad_bottom(rail, g_compact_layout ? 6 : 9, 0);
  lv_obj_set_style_pad_row(rail, g_compact_layout ? 2 : 0, 0);
  lv_obj_set_layout(rail, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);

  rail_header = create_group(rail, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(rail_header, LV_PCT(100), g_compact_layout ? 22 : 25);
  lv_obj_set_style_pad_column(rail_header, 4, 0);
  lv_obj_set_flex_align(rail_header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  {
    lv_obj_t *rail_title = create_label(rail_header,
                                        g_compact_layout ? "状态" : "系统状态",
                                        ui_font(), COLOR_WHITE);
    lv_label_set_long_mode(rail_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(rail_title, g_compact_layout ? 56 : 100);
  }
  create_label(rail_header, "正常", ui_font(), COLOR_GREEN);
  create_dark_status_row(rail, UI_ICON_SETTINGS, "CPU", "--",
                         &g_ui.home_cpu);
  create_dark_status_row(rail, UI_ICON_DRIVE, "内存", "--",
                         &g_ui.home_memory);
  create_dark_status_row(rail, UI_ICON_WIFI, "网络", "--",
                         &g_ui.home_network);
  create_dark_status_row(rail, UI_ICON_REFRESH, "运行", "--:--",
                         &g_ui.home_uptime);

  task_strip = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_grid_cell(task_strip, LV_GRID_ALIGN_STRETCH, 0, 2,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_layout(task_strip, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(task_strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_left(task_strip, 8, 0);
  lv_obj_set_style_pad_right(task_strip, 8, 0);
  lv_obj_set_style_pad_top(task_strip, 6, 0);
  lv_obj_set_style_pad_bottom(task_strip, 6, 0);
  lv_obj_set_style_pad_column(task_strip, 8, 0);
  lv_obj_set_flex_align(task_strip, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  task_icon = create_icon_badge(task_strip, UI_ICON_LIST, COLOR_BLUE, 26);
  task_copy = create_group(task_strip, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(task_copy, 1);
  lv_obj_set_height(task_copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(task_copy, 1);
  lv_obj_set_style_pad_row(task_copy, 2, 0);
  lv_obj_set_flex_align(task_copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(task_copy, "学习任务", ui_font(), COLOR_MUTED);
  g_ui.home_task_count = create_label(task_copy, "0 项待办", ui_font(),
                                      COLOR_TEXT);
  lv_label_set_long_mode(g_ui.home_task_count, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.home_task_count, LV_PCT(100));
  task_progress = create_group(task_strip, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(task_progress, 52, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(task_progress, 2, 0);
  lv_obj_set_flex_align(task_progress, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  create_label(task_progress, "进度", ui_font(), COLOR_MUTED);
  create_label(task_progress, "2/3", &lv_font_montserrat_12, COLOR_BLUE);
  (void)task_icon;
}

static void create_status_page(lv_obj_t *page)
{
  lv_obj_t *metrics;
  lv_obj_t *panel;
  lv_obj_t *row;
  lv_obj_t *value;

  configure_page(page);
  create_page_heading(page, UI_ICON_BARS, COLOR_TEAL, "系统状态",
                      "实时设备运行信息");

  if (g_compact_layout)
    {
      static int32_t metric_cols[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
      };
      static int32_t metric_rows[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
      };
      lv_obj_t *cpu_card;
      lv_obj_t *memory_card;
      lv_obj_t *network_card;
      lv_obj_t *uptime_card;

      metrics = create_group(page, LV_FLEX_FLOW_ROW);
      lv_obj_set_size(metrics, LV_PCT(100), 132);
      lv_obj_set_layout(metrics, LV_LAYOUT_GRID);
      lv_obj_set_grid_dsc_array(metrics, metric_cols, metric_rows);
      lv_obj_set_style_pad_row(metrics, 6, 0);
      lv_obj_set_style_pad_column(metrics, 6, 0);
      cpu_card = create_metric_card(metrics, COLOR_CORAL, "CPU", "检测中",
                                    "运行架构与核心数", &g_ui.cpu_value);
      memory_card = create_metric_card(metrics, COLOR_BLUE, "可用内存",
                                       "-- MB", "系统当前可用内存",
                                       &g_ui.memory_value);
      network_card = create_metric_card(metrics, COLOR_GREEN, "网络",
                                        "检测中", "活动 IPv4 接口",
                                        &g_ui.network_value);
      uptime_card = create_metric_card(metrics, COLOR_AMBER, "运行时间",
                                       "00:00:00", "系统持续运行",
                                       &g_ui.uptime_value);
      lv_obj_set_grid_cell(cpu_card, LV_GRID_ALIGN_STRETCH, 0, 1,
                           LV_GRID_ALIGN_STRETCH, 0, 1);
      lv_obj_set_grid_cell(memory_card, LV_GRID_ALIGN_STRETCH, 1, 1,
                           LV_GRID_ALIGN_STRETCH, 0, 1);
      lv_obj_set_grid_cell(network_card, LV_GRID_ALIGN_STRETCH, 0, 1,
                           LV_GRID_ALIGN_STRETCH, 1, 1);
      lv_obj_set_grid_cell(uptime_card, LV_GRID_ALIGN_STRETCH, 1, 1,
                           LV_GRID_ALIGN_STRETCH, 1, 1);
    }
  else
    {
      metrics = create_group(page, LV_FLEX_FLOW_ROW);
      lv_obj_set_size(metrics, LV_PCT(100), 150);
      lv_obj_set_style_pad_column(metrics, 14, 0);
      create_metric_card(metrics, COLOR_CORAL, "CPU", "检测中",
                         "运行架构与核心数", &g_ui.cpu_value);
      create_metric_card(metrics, COLOR_BLUE, "可用内存", "-- MB",
                         "系统当前可用内存", &g_ui.memory_value);
      create_metric_card(metrics, COLOR_GREEN, "网络", "检测中",
                         "活动 IPv4 接口", &g_ui.network_value);
      create_metric_card(metrics, COLOR_AMBER, "运行时间", "00:00:00",
                         "系统持续运行", &g_ui.uptime_value);
    }

  panel = create_panel(page, g_compact_layout ? 88 : 190);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, g_compact_layout ? 2 : 0, 0);
  create_label(panel, "核心服务", ui_font(), COLOR_TEXT);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 20 : 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "openvela 内核", ui_font(), COLOR_MUTED);
  value = create_label(row, "运行中", ui_font(), COLOR_GREEN);
  lv_obj_set_width(value, g_compact_layout ? 72 : 90);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 20 : 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "ai_agent", &lv_font_montserrat_14, COLOR_MUTED);
  value = create_label(row, "已就绪", ui_font(), COLOR_TEAL);
  lv_obj_set_width(value, g_compact_layout ? 72 : 90);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 20 : 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "显示后端", ui_font(), COLOR_MUTED);
  value = create_label(row, "LVGL 9.1", &lv_font_montserrat_12, COLOR_BLUE);
  lv_obj_set_width(value, g_compact_layout ? 72 : 90);
}

#ifdef _WIN32
/* Static demo row. The board builds its rows in update_task_list() instead. */

static void create_task_row(lv_obj_t *parent, bool completed,
                            const char *title, const char *detail)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *marker;
  lv_obj_t *text;

  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 30 : 64);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_left(row, 2, 0);
  lv_obj_set_style_pad_right(row, 2, 0);
  lv_obj_set_style_pad_column(row, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  marker = lv_obj_create(row);
  object_reset(marker);
  lv_obj_set_size(marker, g_compact_layout ? 14 : 18,
                  g_compact_layout ? 14 : 18);
  lv_obj_set_style_bg_color(marker,
                            color(completed ? COLOR_GREEN : COLOR_SURFACE_ALT),
                            0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(marker,
                                color(completed ? COLOR_GREEN : COLOR_MUTED), 0);
  lv_obj_set_style_border_width(marker, 2, 0);
  lv_obj_set_style_radius(marker, 4, 0);

  text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_width(text, 1);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  {
    lv_obj_t *title_label = create_label(text, title, ui_font(),
                                         completed ? COLOR_MUTED : COLOR_TEXT);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title_label, LV_PCT(100));
  }
  if (!g_compact_layout)
    {
      create_label(text, detail, ui_font(), COLOR_MUTED);
    }
}
#endif /* _WIN32 */

static void create_tasks_page(lv_obj_t *page)
{
  lv_obj_t *summary;
  lv_obj_t *list;
  lv_obj_t *button_row;
  lv_obj_t *unused;

  configure_page(page);
  create_page_heading(page, UI_ICON_LIST, COLOR_BLUE, "学习任务",
                      "由 Study Assistant Skill 保存");

  summary = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(summary, LV_PCT(100), g_compact_layout ? 56 : 128);
  lv_obj_set_style_pad_column(summary, g_compact_layout ? 6 : 14, 0);
  create_metric_card(summary, COLOR_AMBER, "待完成", "0",
                     "尚未勾选", &g_ui.task_pending_value);
  create_metric_card(summary, COLOR_GREEN, "已完成", "0",
                     "已经勾选", &g_ui.task_completed_value);
  create_metric_card(summary, COLOR_BLUE, "提醒", "可用",
                     "主动提醒已验证", &unused);

  list = create_panel(page, g_compact_layout ? 108 : 260);
  lv_obj_set_layout(list, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 0, 0);

#ifdef _WIN32
  /* Preview: static rows so the layout can be judged without a task file. */

  create_task_row(list, false, "完成传感器测试",
                  "保存在 STUDY_TASKS.md");
  create_task_row(list, true, "验证 QEMU 与 MiMo",
                  "TLS 和模型回复测试通过");
  create_task_row(list, true, "测试主动休息提醒",
                  "30 秒单次提醒测试通过");
#else
  /* Board: rows come from STUDY_TASKS.md and are editable. update_task_list()
   * fills this in and rebuilds it whenever the file changes — including when
   * the agent writes it through the task-manager skill.
   */

  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  g_ui.task_list_container = list;

  /* Preset buttons: the handoff settled on "预设短语 + 语音" with no on-screen
   * keyboard, so adding a task is a tap rather than typing.
   */

  {
    lv_obj_t *presets = create_group(page, LV_FLEX_FLOW_ROW_WRAP);
    size_t i;

    lv_obj_set_width(presets, LV_PCT(100));
    lv_obj_set_height(presets, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(presets, 4, 0);
    lv_obj_set_style_pad_row(presets, 4, 0);
    lv_obj_set_flex_align(presets, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    for (i = 0; i < TASK_PRESETS_COUNT; i++)
      {
        lv_obj_t *preset = lv_button_create(presets);
        lv_obj_t *preset_label;

        lv_obj_set_height(preset, g_compact_layout ? 24 : 32);
        lv_obj_set_style_radius(preset, 6, 0);
        lv_obj_set_style_bg_color(preset, color(COLOR_BLUE), 0);
        lv_obj_set_style_shadow_width(preset, 0, 0);
        lv_obj_set_style_pad_left(preset, 8, 0);
        lv_obj_set_style_pad_right(preset, 8, 0);
        lv_obj_set_style_pad_top(preset, 0, 0);
        lv_obj_set_style_pad_bottom(preset, 0, 0);
        lv_obj_set_user_data(preset, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(preset, task_preset_event_cb, LV_EVENT_CLICKED,
                            NULL);

        preset_label = create_label(preset, TASK_PRESETS[i].display_text,
                                    ui_font(), COLOR_WHITE);
        lv_obj_center(preset_label);
      }
  }
#endif

  button_row = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_height(button_row, g_compact_layout ? 28 : 48);
  lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  g_ui.task_file_status = create_label(button_row, "正在读取任务文件",
                                       ui_font(), COLOR_MUTED);
  lv_label_set_long_mode(g_ui.task_file_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.task_file_status, g_compact_layout ? 280 : 360);
  g_ui.task_sync_label = create_action_button(button_row, COLOR_BLUE,
                                              "同步",
                                              task_sync_event_cb);
  if (g_compact_layout)
    {
      lv_obj_set_height(lv_obj_get_parent(g_ui.task_sync_label), 28);
    }
}

static void create_ai_page(lv_obj_t *page)
{
  static int32_t columns[] = {
    LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST
  };
  static int32_t rows[] = {LV_GRID_FR(1), 48, LV_GRID_TEMPLATE_LAST};
  lv_obj_t *voice;
  lv_obj_t *voice_copy;
  lv_obj_t *voice_control;
  lv_obj_t *pulse;
  lv_obj_t *button;
  lv_obj_t *rail;
  lv_obj_t *rail_header;
  lv_obj_t *recent;
  lv_obj_t *recent_copy;
  lv_obj_t *unused;

  configure_page(page);
  lv_obj_set_layout(page, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(page, columns, rows);
  lv_obj_set_style_pad_all(page, 8, 0);
  lv_obj_set_style_pad_row(page, 8, 0);
  lv_obj_set_style_pad_column(page, 8, 0);

  voice = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_grid_cell(voice, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_layout(voice, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(voice, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(voice, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_style_pad_column(voice, 8, 0);

  voice_copy = create_group(voice, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(voice_copy, 1);
  lv_obj_set_height(voice_copy, LV_PCT(100));
  lv_obj_set_flex_grow(voice_copy, 1);
  lv_obj_set_style_pad_row(voice_copy, g_compact_layout ? 3 : 5, 0);
  lv_obj_set_flex_align(voice_copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(voice_copy, "AI 助手", ui_font(), COLOR_BLUE);
  create_label(voice_copy, "正在聆听", ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(voice_copy, "今天复习 ai_agent 架构", ui_font(),
                   COLOR_MUTED);
    }
  g_ui.ai_connection = create_label(voice_copy, "正在检查连接",
                                    ui_font(), COLOR_GREEN);
  lv_label_set_long_mode(g_ui.ai_connection, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.ai_connection, LV_PCT(100));

  voice_control = create_group(voice, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(voice_control, 104, LV_PCT(100));
  lv_obj_set_flex_align(voice_control, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  pulse = lv_arc_create(voice_control);
  g_ui.ai_pulse = pulse;
  lv_obj_set_size(pulse, 76, 76);
  lv_obj_remove_flag(pulse, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_style(pulse, NULL, LV_PART_KNOB);
  lv_arc_set_bg_angles(pulse, 0, 360);
  lv_arc_set_range(pulse, 0, 100);
  lv_arc_set_value(pulse, 72);
  lv_obj_set_style_arc_width(pulse, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_width(pulse, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(pulse, color(0xb9cffd), LV_PART_MAIN);
  lv_obj_set_style_arc_color(pulse, color(COLOR_BLUE), LV_PART_INDICATOR);

  button = lv_button_create(voice_control);
  g_ui.ai_button = button;
  lv_obj_set_size(button, g_compact_layout ? 44 : 52,
                  g_compact_layout ? 44 : 52);
  lv_obj_add_flag(button, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
  lv_obj_set_style_shadow_width(button, 16, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_add_event_cb(button, ai_check_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_align_to(button, pulse, LV_ALIGN_CENTER, 0, 0);
  g_ui.ai_check_label = create_label(button, UI_ICON_AUDIO,
                                     &lv_font_montserrat_20, COLOR_WHITE);
  lv_obj_center(g_ui.ai_check_label);

  rail = lv_obj_create(page);
  lv_obj_remove_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(rail, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_bg_color(rail, color(0x0a1b2f), 0);
  lv_obj_set_style_bg_opa(rail, LV_OPA_80, 0);
  lv_obj_set_style_border_color(rail, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(rail, LV_OPA_10, 0);
  lv_obj_set_style_border_width(rail, 1, 0);
  lv_obj_set_style_radius(rail, 8, 0);
  lv_obj_set_style_pad_all(rail, g_compact_layout ? 7 : 9, 0);
  lv_obj_set_style_pad_row(rail, 0, 0);
  lv_obj_set_layout(rail, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);

  rail_header = create_group(rail, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(rail_header, LV_PCT(100), 24);
  lv_obj_set_style_pad_column(rail_header, 4, 0);
  lv_obj_set_flex_align(rail_header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  {
    lv_obj_t *rail_title = create_label(rail_header,
                                        g_compact_layout ? "MiMo" : "小米 MiMo",
                                        ui_font(), COLOR_WHITE);
    lv_label_set_long_mode(rail_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(rail_title, g_compact_layout ? 70 : 120);
  }
  create_label(rail_header, "在线", ui_font(), COLOR_GREEN);
  create_dark_status_row(rail, UI_ICON_DIRECTORY, "模型", "v2.5",
                         &unused);
  create_dark_status_row(rail, UI_ICON_SETTINGS, "工具", "36", &unused);
  create_dark_status_row(rail, UI_ICON_REFRESH, "延迟", "3.3", &unused);
  create_dark_status_row(rail, UI_ICON_AUDIO,
                         g_compact_layout ? "麦克" : "麦克风", "开",
                         &unused);

  recent = create_panel(page, LV_SIZE_CONTENT);
  lv_obj_set_grid_cell(recent, LV_GRID_ALIGN_STRETCH, 0, 2,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_layout(recent, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(recent, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_left(recent, 8, 0);
  lv_obj_set_style_pad_right(recent, 8, 0);
  lv_obj_set_style_pad_top(recent, 6, 0);
  lv_obj_set_style_pad_bottom(recent, 6, 0);
  lv_obj_set_style_pad_column(recent, 8, 0);
  lv_obj_set_flex_align(recent, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_badge(recent, UI_ICON_OK, COLOR_BLUE,
                    g_compact_layout ? 24 : 26);
  recent_copy = create_group(recent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(recent_copy, 1);
  lv_obj_set_height(recent_copy, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(recent_copy, 1);
  lv_obj_set_style_pad_row(recent_copy, 2, 0);
  lv_obj_set_flex_align(recent_copy, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(recent_copy, "最近回复", ui_font(), COLOR_MUTED);
  {
    lv_obj_t *recent_text = create_label(recent_copy,
                                         g_compact_layout ? "已生成 4 个重点" :
                                         "已生成 4 个学习重点",
                                         ui_font(), COLOR_TEXT);
    lv_label_set_long_mode(recent_text, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(recent_text, LV_PCT(100));
  }
}

#ifdef _WIN32
/* Decorative row. The board uses create_toggle_row()/create_action_row(),
 * which attach real callbacks.
 */

static void create_setting_row(lv_obj_t *parent, const char *title,
                               const char *detail, bool enabled)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *toggle;

  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 34 : 70);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_column(row, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_width(text, 1);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(text, g_compact_layout ? 0 : 2, 0);
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  create_label(text, title, ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(text, detail, ui_font(), COLOR_MUTED);
    }

  toggle = lv_switch_create(row);
  lv_obj_set_size(toggle, g_compact_layout ? 40 : 52,
                  g_compact_layout ? 22 : 28);
  lv_obj_set_style_bg_color(toggle, color(COLOR_SURFACE_ALT), LV_PART_MAIN);
  lv_obj_set_style_bg_color(toggle, color(COLOR_BLUE),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (enabled)
    {
      lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }
}
#endif /* _WIN32 */

#ifndef _WIN32

/* Persisted playback gain. audio_playback.c in ai_agent reads this file. */

static int load_volume(void)
{
  FILE *file;
  char buffer[128];
  size_t nread;
  const char *found;
  int value = 70;

  file = fopen(VOLUME_FILE, "r");
  if (file == NULL)
    {
      return value;
    }

  nread = fread(buffer, 1, sizeof(buffer) - 1, file);
  fclose(file);

  if (nread == 0)
    {
      return value;
    }

  buffer[nread] = '\0';

  found = strstr(buffer, "\"volume\"");
  if (found != NULL)
    {
      found = strchr(found, ':');
      if (found != NULL)
        {
          int parsed = atoi(found + 1);

          if (parsed >= 0 && parsed <= 100)
            {
              value = parsed;
            }
        }
    }

  return value;
}

static void volume_slider_event_cb(lv_event_t *event)
{
  lv_obj_t *slider = lv_event_get_target(event);
  int32_t value = lv_slider_get_value(slider);
  FILE *file;
  char buffer[64];

  note_user_activity();
  lv_label_set_text_fmt(g_ui.volume_label, "音量  %d%%", (int)value);

  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      return;
    }

  snprintf(buffer, sizeof(buffer), "{\"version\":1,\"volume\":%d}\n",
           (int)value);

  file = fopen(VOLUME_TEMP_FILE, "w");
  if (file == NULL)
    {
      return;
    }

  if (fputs(buffer, file) < 0 || fflush(file) != 0)
    {
      fclose(file);
      unlink(VOLUME_TEMP_FILE);
      return;
    }

  fclose(file);

  if (rename(VOLUME_TEMP_FILE, VOLUME_FILE) != 0)
    {
      unlink(VOLUME_TEMP_FILE);
    }
}

static void wake_switch_event_cb(lv_event_t *event)
{
  lv_obj_t *sw = lv_event_get_target(event);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

  note_user_activity();

  if (!enabled)
    {
      voice_ui_bridge_wake_stop();
      return;
    }

  if (voice_ui_bridge_wake_start() < 0)
    {
      lv_obj_t *msgbox;

      /* Do not leave the switch showing "on" when the listener never
       * started. voice_wake_start() returns -ENODEV until the MiMo ASR
       * backend is registered, which is the usual cause here.
       */

      lv_obj_clear_state(sw, LV_STATE_CHECKED);

      msgbox = lv_msgbox_create(lv_screen_active());
      lv_msgbox_add_title(msgbox, "唤醒词");
      lv_msgbox_add_text(msgbox, "语音识别未就绪，请先验证 ASR");
      lv_msgbox_add_close_button(msgbox);
    }
}

static void wifi_button_event_cb(lv_event_t *event)
{
  (void)event;

  note_user_activity();
  wifi_setup_init(lv_screen_active(), ui_font(), g_compact_layout);
}

/* A settings row carrying a real switch, styled like create_setting_row() but
 * with a callback attached and the object returned so state can be reset.
 */

static lv_obj_t *create_toggle_row(lv_obj_t *parent, const char *title,
                                   const char *detail, bool enabled,
                                   lv_event_cb_t callback)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *toggle;

  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 34 : 70);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_column(row, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_width(text, 1);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(text, g_compact_layout ? 0 : 2, 0);
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  create_label(text, title, ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(text, detail, ui_font(), COLOR_MUTED);
    }

  toggle = lv_switch_create(row);
  lv_obj_set_size(toggle, g_compact_layout ? 40 : 52,
                  g_compact_layout ? 22 : 28);
  lv_obj_set_style_bg_color(toggle, color(COLOR_SURFACE_ALT), LV_PART_MAIN);
  lv_obj_set_style_bg_color(toggle, color(COLOR_BLUE),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  if (enabled)
    {
      lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }

  if (callback != NULL)
    {
      lv_obj_add_event_cb(toggle, callback, LV_EVENT_VALUE_CHANGED, NULL);
    }

  return toggle;
}

/* Same row shape, but the control is a button that opens another page. */

static lv_obj_t *create_action_row(lv_obj_t *parent, const char *title,
                                   const char *detail,
                                   lv_event_cb_t callback)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *text = create_group(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *button;
  lv_obj_t *label;

  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 34 : 70);
  lv_obj_set_style_border_color(row, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_column(row, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_width(text, 1);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(text, g_compact_layout ? 0 : 2, 0);
  lv_obj_set_flex_align(text, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  create_label(text, title, ui_font(), COLOR_TEXT);
  if (!g_compact_layout)
    {
      create_label(text, detail, ui_font(), COLOR_MUTED);
    }

  button = lv_button_create(row);
  lv_obj_set_size(button, g_compact_layout ? 60 : 84,
                  g_compact_layout ? 24 : 32);
  lv_obj_set_style_radius(button, 6, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_TEAL), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);

  label = create_label(button, "打开", ui_font(), COLOR_WHITE);
  lv_obj_center(label);

  return button;
}

#endif /* !_WIN32 */

static void create_settings_page(lv_obj_t *page)
{
  lv_obj_t *panel;
  lv_obj_t *info;

  configure_page(page);
  create_page_heading(page, UI_ICON_SETTINGS, COLOR_AMBER, "设置",
                      "调整终端的常用行为");

  panel = create_panel(page, g_compact_layout ? 156 : 310);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 0, 0);

#ifdef _WIN32
  /* Preview: decorative rows, nothing behind them to toggle. */

  create_setting_row(panel, "主动提醒", "允许学习与休息提醒", true);
  create_setting_row(panel, "状态自动刷新", "每秒更新设备信息", true);
  create_setting_row(panel, "页面动画", "使用简短切换动画", false);
  create_setting_row(panel, "调试信息", "显示开发阶段诊断内容", false);
#else
  /* Board: only controls that actually do something. A switch that toggles
   * nothing is worse than no switch, so the four mockup rows are replaced by
   * volume, the wake word, and WiFi provisioning.
   */

  lv_obj_set_scroll_dir(panel, LV_DIR_VER);

  {
    lv_obj_t *volume_row;
    int initial_volume = load_volume();

    volume_row = create_group(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(volume_row, LV_PCT(100));
    lv_obj_set_height(volume_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(volume_row, 2, 0);
    lv_obj_set_style_pad_top(volume_row, 4, 0);
    lv_obj_set_style_pad_bottom(volume_row, 6, 0);

    g_ui.volume_label = create_label(volume_row, "", ui_font(), COLOR_TEXT);
    lv_label_set_text_fmt(g_ui.volume_label, "音量  %d%%", initial_volume);

    g_ui.volume_slider = lv_slider_create(volume_row);
    lv_obj_set_width(g_ui.volume_slider, LV_PCT(96));
    lv_obj_set_height(g_ui.volume_slider, g_compact_layout ? 8 : 12);
    lv_slider_set_range(g_ui.volume_slider, 0, 100);
    lv_slider_set_value(g_ui.volume_slider, initial_volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_ui.volume_slider, color(COLOR_SURFACE_ALT),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.volume_slider, color(COLOR_BLUE),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_ui.volume_slider, color(COLOR_BLUE),
                              LV_PART_KNOB);
    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
  }

  /* Wake word stays OFF by default until ASR is verified on hardware. */

  g_ui.wake_switch = create_toggle_row(panel, "唤醒词 小维同学",
                                       "验证 ASR 后再开启", false,
                                       wake_switch_event_cb);

  g_ui.wifi_button = create_action_row(panel, "WiFi 配置",
                                       "扫描并连接无线网络",
                                       wifi_button_event_cb);
#endif

  if (!g_compact_layout)
    {
      info = create_panel(page, 104);
      lv_obj_set_layout(info, LV_LAYOUT_FLEX);
      lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_row(info, 8, 0);
      create_label(info, "设备信息", ui_font(), COLOR_TEXT);
      create_label(info, "openvela AI 学习终端 · QEMU 演示版", ui_font(),
                   COLOR_MUTED);
    }
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
  uint32_t tab_index;

  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.focus_elapsed = FOCUS_SECONDS * 18 / 100;
  g_ui.on_standby = true;
  g_ui.idle_seconds = 0;

  g_ui.screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_ui.screen, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(g_ui.screen, &lv_font_montserrat_16, 0);
  lv_obj_set_layout(g_ui.screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_ui.screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(g_ui.screen, 0, 0);
  lv_obj_set_style_pad_row(g_ui.screen, 0, 0);
  lv_obj_add_event_cb(g_ui.screen, main_input_event_cb, LV_EVENT_PRESSED,
                      NULL);
  lv_obj_add_event_cb(g_ui.screen, main_input_event_cb, LV_EVENT_CLICKED,
                      NULL);

  create_wallpaper(g_ui.screen);
  create_header(g_ui.screen);

  tabview = lv_tabview_create(g_ui.screen);
  lv_obj_set_width(tabview, LV_PCT(100));
  lv_obj_set_height(tabview, 1);
  lv_obj_set_flex_grow(tabview, 1);
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tabview, g_compact_layout ? 44 : 62);
  lv_obj_set_style_bg_color(tabview, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(tabview, LV_OPA_20, 0);
  lv_obj_set_style_border_width(tabview, 0, 0);

  tabbar = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_width(tabbar, LV_PCT(100));
  lv_obj_set_style_bg_color(tabbar, color(0x0a1b2f), 0);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_80, 0);
  lv_obj_set_style_text_color(tabbar, color(0x9eb0c2), 0);
  lv_obj_set_style_text_font(tabbar, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(tabbar, color(0x75a0ff),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabbar, color(0x17314f),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER,
                          LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabbar, color(0x75a0ff),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabbar, 2,
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tabbar, LV_BORDER_SIDE_TOP,
                               LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabbar, 0, 0);
  lv_obj_set_style_pad_left(tabbar, 0, 0);
  lv_obj_set_style_pad_right(tabbar, 0, 0);
  lv_obj_set_style_pad_column(tabbar, 0, 0);

  home = lv_tabview_add_tab(tabview, "Home");
  status = lv_tabview_add_tab(tabview, "Stat");
  tasks = lv_tabview_add_tab(tabview, "Task");
  ai = lv_tabview_add_tab(tabview, "AI");
  settings = lv_tabview_add_tab(tabview, "Set");

  for (tab_index = 0; tab_index < lv_obj_get_child_count(tabbar); tab_index++)
    {
      lv_obj_set_flex_grow(lv_obj_get_child(tabbar, tab_index), 1);
    }

  create_home_page(home);
  create_status_page(status);
  create_tasks_page(tasks);
  create_ai_page(ai);
  create_settings_page(settings);
  create_standby_screen();
  create_voice_screen();

  lv_tabview_set_active(tabview, g_initial_tab, LV_ANIM_OFF);

  update_task_widgets();
  update_system_widgets();
  update_standby_widgets();
  lv_timer_create(update_timer_cb, 1000, NULL);

#ifndef _WIN32
  /* Replies arrive asynchronously on ai_agent's dispatch thread, so poll the
   * bridge often enough that a streamed answer does not feel stalled.
   */

  lv_timer_create(ai_reply_timer_cb, 500, NULL);

  /* The agent can rewrite STUDY_TASKS.md at any time via the task-manager
   * skill; watch the mtime so those edits show up without a restart.
   */

  lv_timer_create(task_watch_timer_cb, 3000, NULL);
#endif
}

#ifndef _WIN32

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
  lv_obj_t *splash;

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

  configure_display_orientation(result.disp);

#ifdef CONFIG_FT5X06_SWAPXY
  /* The panel reports touch in the pre-rotation frame, so mirror X to match
   * the landscape display or every tap lands on the wrong widget.
   */

  g_touch_read_cb = lv_indev_get_read_cb(result.indev);
  if (g_touch_read_cb != NULL)
    {
      lv_indev_set_read_cb(result.indev, read_touch_aligned_landscape);
      printf("study_terminal: touch aligned to landscape display\n");
    }
#endif

  /* Bring up the bridges before the UI so the first frame can already report
   * real state and the reply tap is registered before any question is sent.
   */

  voice_ui_bridge_init();
  ai_chat_bridge_init();

  initialize_ui_font();
  initialize_wallpaper();
  splash = create_splash_screen();
  lv_screen_load(splash);
  create_ui();
  if (g_ui.standby_screen != NULL)
    {
      lv_screen_load_anim(g_ui.standby_screen, LV_SCR_LOAD_ANIM_FADE_IN,
                          SPLASH_FADE_MS, SPLASH_HOLD_MS, true);
      g_ui.on_standby = true;
    }
  else
    {
      lv_screen_load_anim(g_ui.screen, LV_SCR_LOAD_ANIM_FADE_IN,
                          SPLASH_FADE_MS, SPLASH_HOLD_MS, true);
    }

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
  free(g_wallpaper_data);
  g_wallpaper_data = NULL;
  return 0;
}

#else

void study_terminal_windows_start(lv_display_t *display, int argc, char *argv[])
{
  lv_obj_t *splash;

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

  configure_display_orientation(display);
  initialize_ui_font();
  initialize_boot_logo();
  initialize_wallpaper();
  splash = create_splash_screen();
  lv_screen_load(splash);
  create_ui();
  if (g_ui.standby_screen != NULL)
    {
      lv_screen_load_anim(g_ui.standby_screen, LV_SCR_LOAD_ANIM_FADE_IN,
                          SPLASH_FADE_MS, SPLASH_HOLD_MS, true);
      g_ui.on_standby = true;
    }
  else
    {
      lv_screen_load_anim(g_ui.screen, LV_SCR_LOAD_ANIM_FADE_IN,
                          SPLASH_FADE_MS, SPLASH_HOLD_MS, true);
    }
}

void study_terminal_windows_stop(void)
{
  free(g_boot_logo_data);
  g_boot_logo_data = NULL;
  free(g_wallpaper_data);
  g_wallpaper_data = NULL;
}

#endif
