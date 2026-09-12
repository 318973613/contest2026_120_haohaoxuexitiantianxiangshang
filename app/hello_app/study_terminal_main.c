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
#  include "study_focus_stats.h"
#  include <netutils/cJSON.h>
#  include <infra/cron_snapshot.h>
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

extern const lv_font_t lv_font_simsun_16_cjk;

/* Baked at build time from NotoSansSC (generated/lv_font_study_16.c).  The
 * Win32 preview draws with it, so reusing it here is what makes the board's
 * UI text pixel-identical rather than merely similar.
 */

extern const lv_font_t lv_font_study_16;

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
#  define WEATHER_FILE "/data/ai_agent/WEATHER.json"

/* The agent's own config store.  Writing weather.city here is what makes the
 * on-screen picker reach weather_service.c -- it re-reads the key every cycle,
 * so no restart is needed.
 */

#  define AGENT_CONFIG_DIR "/data/ai_agent/config"
#  define AGENT_CONFIG_PATH AGENT_CONFIG_DIR "/config.json"
#  define VOLUME_TEMP_FILE "/data/ai_agent/STUDY_VOLUME.json.tmp"
#  define FOCUS_FILE "/data/ai_agent/STUDY_FOCUS.json"
#  define AGENT_DATA_DIR "/data/ai_agent"
#  define UI_FONT_FILE "/data/NotoSansSC-Regular.ttf"
#  define UI_WALLPAPER_FILE "/data/study-terminal-wallpaper.rgb565"
#  define UI_BOOT_LOGO_FILE "/data/openvela-64x64.rgb565"
#endif

#ifndef _WIN32
#  define MAX_TASKS 20
#  define MAX_TASK_TEXT 80

/* VISIBLE_TASKS used to cap the rendered rows at 4.  The container scrolls, so
 * the cap only served to strand tasks 5..MAX_TASKS where they could be counted
 * but never ticked off or deleted.  Rows are bounded by MAX_TASKS now.
 */
#endif
#define UI_WALLPAPER_WIDTH 480
#define UI_WALLPAPER_HEIGHT 320
#define UI_WALLPAPER_BYTES (UI_WALLPAPER_WIDTH * UI_WALLPAPER_HEIGHT * 2)
#define UI_BOOT_LOGO_WIDTH  64
#define UI_BOOT_LOGO_HEIGHT 64
#define UI_BOOT_LOGO_BYTES  (UI_BOOT_LOGO_WIDTH * UI_BOOT_LOGO_HEIGHT * 2)
/* Selectable session lengths, in g_focus_presets.  25 minutes stays the
 * default because it is what the previous fixed FOCUS_SECONDS used and what
 * most users expect; 15 suits a single exercise set and 45 a longer reading
 * block.  focus_round_seconds() replaced every use of the old constant.
 */

#define FOCUS_PRESET_COUNT 3

/* Break length.  Fixed at 5 minutes rather than scaled to the round: the
 * point of the break is to be short enough that the user comes back, and a
 * 45-minute round does not warrant a 9-minute one.
 */

#define BREAK_SECONDS (5 * 60)
#define FOCUS_DEFAULT_INDEX 1
#define SPLASH_PROGRESS_MS 1350
#define SPLASH_HOLD_MS     1650
#define SPLASH_FADE_MS     240
#define STANDBY_IDLE_SECONDS 45

#ifndef _WIN32
#define UI_REMINDER_LIMIT 16
struct reminder_row_s
{
  lv_obj_t *row;
  lv_obj_t *message;
  lv_obj_t *remaining;
  lv_obj_t *cancel;
  char id[9];
  char pressed_id[9];
};
#endif

struct study_ui_s
{
  lv_obj_t *screen;
  lv_obj_t *standby_screen;
  lv_obj_t *tabview;
  lv_obj_t *header_clock;
  lv_obj_t *header_net_icon;
  lv_obj_t *header_net_text;
  lv_obj_t *home_task_count;
  lv_obj_t *home_task_ratio;
  lv_obj_t *home_focus_subject;
  lv_obj_t *home_focus_title;
  lv_obj_t *home_health;
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
  lv_obj_t *task_rate_value;
  lv_obj_t *task_plan_label;
  lv_obj_t *status_agent_value;
  lv_obj_t *status_readiness_value;
  lv_obj_t *ai_connection;
  lv_obj_t *ai_rail_state;
  lv_obj_t *ai_service_value;
  lv_obj_t *ai_voice_value;
  lv_obj_t *ai_request_value;
  lv_obj_t *ai_subtitle;
  lv_obj_t *ai_recent_text;
  lv_obj_t *ai_button;
  lv_obj_t *ai_pulse;
  lv_obj_t *ai_check_label;
  lv_obj_t *ai_coach_label;
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
  bool ptt_held;
  uint32_t ptt_request_id;
  enum voice_ui_ptt_state_e ptt_state;
  const char *voice_notice;
  voice_dialogue_status_t dialogue;
  uint32_t dialogue_seen;
  bool dialogue_animated;
  bool dialogue_mode;
  cron_reminder_snapshot_t reminder;
  bool reminder_cancelling;
  study_focus_stats_t focus_stats;
  int focus_save_error;
  int focus_display_day;
  bool tools_active;
  lv_obj_t *tools_screen;
  lv_obj_t *tools_tabs;
  lv_obj_t *reminder_badge;
  lv_obj_t *reminder_status;
  lv_obj_t *reminder_empty;
  lv_obj_t *reminder_duration;
  lv_obj_t *reminder_kind;
  lv_obj_t *reminder_add;
  lv_obj_t *reminder_clear;
  lv_obj_t *reminder_quick[3];
  uint32_t reminder_request;
  int reminder_count;
  char reminder_cancel_id[9];
  cron_reminder_snapshot_t reminder_items[UI_REMINDER_LIMIT];
  struct reminder_row_s reminder_rows[UI_REMINDER_LIMIT];
  lv_obj_t *goal_control;
  lv_obj_t *goal_progress;
  lv_obj_t *goal_summary;
  lv_obj_t *focus_history_note;
  lv_obj_t *history_bars[STUDY_FOCUS_DAYS];
  lv_obj_t *history_values[STUDY_FOCUS_DAYS];
  lv_obj_t *history_dates[STUDY_FOCUS_DAYS];

  /* Study report tab.  Every figure is derived from focus_stats and the task
   * file, so the tab adds no storage of its own and cannot disagree with the
   * home page.
   */

  lv_obj_t *report_today;
  lv_obj_t *report_rounds;
  lv_obj_t *report_tasks;
  lv_obj_t *report_streak;
  lv_obj_t *report_week_label;
  lv_obj_t *report_week_bar;
  lv_obj_t *report_note;
  lv_obj_t *notice_box;
  lv_obj_t *notice_snooze_button;
  lv_obj_t *notice_ack_button;
  bool notice_snooze_pending;
  char notice_message[256];
  char pending_focus_title[32];
  char pending_focus_message[128];

  /* Settings additions. */

  lv_obj_t *volume_slider;
  lv_obj_t *volume_label;
  lv_obj_t *wake_switch;
  lv_obj_t *wifi_button;
  lv_obj_t *city_label;
#endif
  bool focus_running;
  uint8_t focus_preset;

  /* True while the arc is counting a break instead of a focus round. */

  bool on_break;
  uint32_t focus_rounds;
  uint32_t focus_minutes;
  lv_obj_t *focus_length_label;
  lv_obj_t *focus_stat_value;
  bool on_standby;
  bool voice_active;
  uint32_t focus_elapsed;
  uint32_t focus_tick_ms;
  uint32_t focus_fraction_ms;
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

/* lv_font_study_16 is const, so its fallback chain cannot be retargeted in
 * place.  This copy carries the same glyph data and callbacks but lets us
 * point `fallback` at the full TTF face for characters the baked font lacks.
 */

static lv_font_t g_ui_font;

/* Writable copy of the baked font's montserrat fallback.  Needed for the same
 * reason as g_ui_font: the built-in fonts are const, so the only way to hang
 * the TTF behind montserrat is to chain through a copy.
 */

static lv_font_t g_montserrat_link;
static bool g_ui_font_ready;
#endif
static bool g_compact_layout;
static uint8_t *g_wallpaper_data;
static lv_image_dsc_t g_wallpaper;

#ifndef _WIN32
static time_t g_task_file_mtime;
static bool g_task_reload_pending;

#  ifdef CONFIG_FT5X06_SWAPXY
static lv_indev_read_cb_t g_touch_read_cb;
#  endif

static void update_task_list(void);
#endif

static bool clock_is_synced(time_t wall_time);

/* Minutes per selectable round.  Index into this with g_ui.focus_preset. */

static const uint8_t g_focus_presets[FOCUS_PRESET_COUNT] =
{
  15, 25, 45
};

static uint32_t focus_round_minutes(void)
{
  uint8_t index = g_ui.focus_preset;

  if (index >= FOCUS_PRESET_COUNT)
    {
      index = FOCUS_DEFAULT_INDEX;
    }

  return g_focus_presets[index];
}

static uint32_t focus_round_seconds(void)
{
  return focus_round_minutes() * 60;
}

static void refresh_study_report(void);
static void refresh_focus_labels(void);

#ifndef _WIN32
static void save_focus_stats(void);
static int focus_day_stamp(void);
static void refresh_focus_history(void);
static void create_tools_screen(void);
static void show_tools_event_cb(lv_event_t *event);
static void refresh_local_tools(void);
static void show_local_notice(const char *title, const char *message,
                              bool can_snooze);
#else

/* The simulator has no /data partition to persist into, and the tally is a
 * device feature.  Stubbed so the call sites need no guards.
 */

#  define save_focus_stats() ((void)0)
#endif
static void note_user_activity(void);
static void show_main_ui(void);
static void show_standby_ui(void);
static void show_voice_ui(void);
static void hide_voice_ui(void);
static void start_voice_wave(void);
static void stop_voice_wave(void);
static void update_standby_widgets(void);
static void update_ai_rail(bool network_online);
static void advance_focus_timer(void);

#ifndef _WIN32

/* Last final answer, kept so the AI page can restore the line after a
 * rebuild.  Only the head of a long reply fits the one-line label, and that
 * is all this needs to hold.
 */

static char g_last_reply[96];

static void remember_recent_reply(const char *text)
{
  size_t len = strlen(text);

  if (len >= sizeof(g_last_reply))
    {
      len = sizeof(g_last_reply) - 1;
      while (len > 0 && ((unsigned char)text[len] & 0xc0) == 0x80)
        {
          len--;
        }
    }

  memcpy(g_last_reply, text, len);
  g_last_reply[len] = '\0';
}
#endif
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
static void create_wallpaper(lv_obj_t *parent);
static void pulse_scale_cb(void *object, int32_t value);
static void pulse_opacity_cb(void *object, int32_t value);
static uint8_t *g_boot_logo_data;
static lv_image_dsc_t g_boot_logo;

static lv_color_t color(uint32_t value)
{
  return lv_color_hex(value);
}

static const lv_font_t *ui_font(void)
{
#ifdef _WIN32
  return &lv_font_study_16;
#else
  /* The chained copy when it is built, otherwise the baked font as-is: either
   * way UI text renders with the preview's glyphs.
   */

  if (g_ui_font_ready)
    {
      return &g_ui_font;
    }

  return &lv_font_study_16;
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
  FILE *probe;
  lv_font_t *font;

  /* Start from the baked font so UI strings always match the preview, then
   * try to extend it with the full TTF for everything else (AI replies, task
   * text, SSIDs -- arbitrary characters the baked 247 cannot cover).
   */

  g_ui_font = lv_font_study_16;
  g_ui_font_ready = true;

  /* Only hand the path to FreeType once we know the file is really there and
   * large enough to be a TTF.  A truncated or missing font makes
   * lv_freetype_font_create() return a half-built lv_font_t whose glyph
   * callbacks are garbage, and the first CJK label to draw then jumps through
   * a wild pointer inside the display refresh (prefetch abort at an unmapped
   * PC).  The built-in CJK face below covers every glyph this UI uses, so a
   * bad TTF costs us the nicer typeface instead of the whole app.
   */

  probe = fopen(UI_FONT_FILE, "rb");
  if (probe == NULL)
    {
      printf("study_terminal: CJK font not found: %s, using built-in\n",
             UI_FONT_FILE);
      return;
    }

  if (fseek(probe, 0, SEEK_END) != 0 || ftell(probe) < 4096)
    {
      printf("study_terminal: CJK font truncated: %s, using built-in\n",
             UI_FONT_FILE);
      fclose(probe);
      return;
    }

  fclose(probe);

  lv_freetype_init(128);
  font = lv_freetype_font_create(
    UI_FONT_FILE, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 18,
    LV_FREETYPE_FONT_STYLE_NORMAL);

  if (font == NULL)
    {
      printf("study_terminal: CJK font create failed, using built-in\n");
      return;
    }

  /* A usable face must be able to hand back glyph data.  If FreeType left
   * either callback unset the font is unusable and drawing through it would
   * fault, so drop it rather than publish it.
   */

  if (font->get_glyph_dsc == NULL || font->get_glyph_bitmap == NULL)
    {
      printf("study_terminal: CJK font incomplete, using built-in\n");
      lv_freetype_font_delete(font);
      return;
    }

  /* Chain: baked CJK -> montserrat_16 -> full TTF.
   *
   * Order matters a great deal here.  lv_font_study_16 holds 258 codepoints
   * and every one of them is CJK, so ASCII, digits and LV_SYMBOL_* icons all
   * miss it.  Putting the TTF second would therefore route the entire UI --
   * including the first frame drawn at boot -- through FreeType, which is
   * both slow and, before the cache NULL guard in lv_freetype_image.c, fatal.
   *
   * With montserrat second the board resolves exactly what the Windows
   * preview resolves (the preview's chain ends there and has no FreeType at
   * all), so the visible pixels match.  The TTF sits behind montserrat and is
   * consulted only for a CJK glyph the baked font lacks, i.e. arbitrary AI
   * reply text -- coverage is unchanged, only the lookup order is.
   *
   * lv_font.c walks `fallback` recursively (`while(f) { ... f = f->fallback; }`),
   * so a three-level chain resolves in one lookup.
   */

  if (lv_font_study_16.fallback != NULL)
    {
      g_montserrat_link = *lv_font_study_16.fallback;
      g_montserrat_link.fallback = font;
      g_ui_font.fallback = &g_montserrat_link;
    }
  else
    {
      /* No montserrat behind the baked font (should not happen: the generated
       * file sets it at compile time).  Fall straight back to the TTF so
       * ASCII still renders, just via FreeType.
       */

      g_ui_font.fallback = font;
    }

  g_dynamic_cjk_font = font;
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

  if (g_boot_logo_data != NULL)
    {
      lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
      content = lv_image_create(mark);
      lv_image_set_src(content, &g_boot_logo);
      lv_obj_center(content);
      return mark;
    }

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

#ifndef _WIN32

/* Copy the first unfinished task into `buffer`.  Returns false when every task
 * is done (or there are none), which the caller shows as a distinct state
 * rather than an empty line.
 */

static bool first_pending_task(char *buffer, size_t buffer_size)
{
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t index;

  count = load_tasks(tasks, MAX_TASKS);

  for (index = 0; index < count; index++)
    {
      if (!tasks[index].completed)
        {
          snprintf(buffer, buffer_size, "%s", tasks[index].text);
          return true;
        }
    }

  return false;
}

#endif

static void update_task_widgets(void)
{
  int pending;
  int completed;
  int total;

  read_task_counts(&pending, &completed);
  total = pending + completed;
  lv_label_set_text_fmt(g_ui.task_pending_value, "%d", pending);
  lv_label_set_text_fmt(g_ui.task_completed_value, "%d", completed);
  lv_label_set_text_fmt(g_ui.home_task_count, "%d 项待办", pending);

  /* Home rail ratio and the tasks-page completion card: both derived from the
   * counts above so they can never drift from the list.
   */

  if (g_ui.home_task_ratio != NULL)
    {
      lv_label_set_text_fmt(g_ui.home_task_ratio, "%d/%d", completed, total);
    }

  if (g_ui.task_rate_value != NULL)
    {
      lv_label_set_text_fmt(g_ui.task_rate_value, "%d%%",
                            total > 0 ? completed * 100 / total : 0);
    }

#ifndef _WIN32
  if (g_ui.home_focus_subject != NULL)
    {
      char subject[MAX_TASK_TEXT + 1];

      if (first_pending_task(subject, sizeof(subject)))
        {
          lv_label_set_text(g_ui.home_focus_subject,
                            task_display_text(subject));
        }
      else if (total > 0)
        {
          lv_label_set_text(g_ui.home_focus_subject, "今天的任务都完成了");
        }
      else
        {
          lv_label_set_text(g_ui.home_focus_subject, "暂无待办");
        }
    }
#endif

  if (pending == 0 && completed == 0)
    {
      lv_label_set_text(g_ui.task_file_status, "暂时没有已保存任务");
    }
  else
    {
      lv_label_set_text_fmt(g_ui.task_file_status, "共 %d 项任务",
                            pending + completed);
    }

#ifndef _WIN32
  /* Rebuild the rows only when the task file really changed.  This function
   * sits on the UI tick, and a rebuild means lv_obj_clean() on the container:
   * the scroll position snaps back to the top and the row under the user's
   * finger is deleted mid-gesture, so the tap never lands.  update_task_list()
   * is what clears these two flags, so a call coming from task_watch_timer_cb
   * still passes.
   */

  if (g_task_reload_pending || task_file_mtime() != g_task_file_mtime)
    {
      update_task_list();
    }
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

  for (index = 0; index < count; index++)
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
      lv_label_set_text(delete_label, LV_SYMBOL_TRASH);
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

/* Drop every completed task in one pass.  Follows the same load, compact,
 * save shape as task_delete_event_cb so both take the same path through
 * save_tasks() and the same mtime-gated rebuild.
 */

static void task_clear_done_event_cb(lv_event_t *event)
{
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t index;
  size_t write_index = 0;

  (void)event;
  note_user_activity();

  count = load_tasks(tasks, MAX_TASKS);

  for (index = 0; index < count; index++)
    {
      if (tasks[index].completed)
        {
          continue;
        }

      if (write_index != index)
        {
          tasks[write_index] = tasks[index];
        }

      write_index++;
    }

  if (write_index == count)
    {
      /* Say so instead of appearing to fail. */

      if (g_ui.task_file_status != NULL)
        {
          lv_label_set_text(g_ui.task_file_status, "没有已完成的任务");
        }

      return;
    }

  if (!save_tasks(tasks, write_index))
    {
      if (g_ui.task_file_status != NULL)
        {
          lv_label_set_text(g_ui.task_file_status, "清理失败");
        }

      return;
    }

  /* Rebuild first: update_task_widgets() rewrites this same label with the
   * task count, so setting the message afterwards is what makes it visible.
   */

  g_task_reload_pending = true;
  update_task_widgets();

  if (g_ui.task_file_status != NULL)
    {
      lv_label_set_text_fmt(g_ui.task_file_status, "已清理 %d 项",
                            (int)(count - write_index));
    }
}

/* Build the same timestamped shape used by study-assistant.  An unsynced
 * board clock must not stamp a new task as 1970, so the prefix is omitted
 * until ntpc has supplied a real wall clock.
 */

static void task_from_preset(struct study_task_s *task, size_t preset_index)
{
  time_t now = time(NULL);
  struct tm tm_now;
  char stamp[24] = "";

  task->completed = false;

  if (clock_is_synced(now) && localtime_r(&now, &tm_now) != NULL)
    {
      strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm_now);
    }

  if (stamp[0] != '\0')
    {
      snprintf(task->text, sizeof(task->text), "[%s] %s", stamp,
               TASK_PRESETS[preset_index].full_text);
    }
  else
    {
      snprintf(task->text, sizeof(task->text), "%s",
               TASK_PRESETS[preset_index].full_text);
    }
}

static bool task_list_has_text(const struct study_task_s *tasks,
                               size_t count, const char *text)
{
  size_t index;

  for (index = 0; index < count; index++)
    {
      if (strcmp(task_display_text(tasks[index].text), text) == 0)
        {
          return true;
        }
    }

  return false;
}

static void task_preset_event_cb(lv_event_t *event)
{
  lv_obj_t *button = lv_event_get_target(event);
  size_t preset_index = (size_t)(uintptr_t)lv_obj_get_user_data(button);
  struct study_task_s tasks[MAX_TASKS];
  size_t count;

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

  task_from_preset(&tasks[count], preset_index);

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

/* A contest demo should reach a meaningful task list in one tap.  The plan is
 * deliberately local and deterministic: it works without a cloud account,
 * writes real task data, and never pretends that MiMo generated it.
 */

static void task_daily_plan_event_cb(lv_event_t *event)
{
  static const size_t plan_presets[] = {1, 3, 2};
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t plan_index;
  size_t added = 0;

  (void)event;
  note_user_activity();
  count = load_tasks(tasks, MAX_TASKS);

  for (plan_index = 0;
       plan_index < sizeof(plan_presets) / sizeof(plan_presets[0]);
       plan_index++)
    {
      size_t preset_index = plan_presets[plan_index];

      if (preset_index >= TASK_PRESETS_COUNT || count >= MAX_TASKS)
        {
          continue;
        }

      if (task_list_has_text(tasks, count,
                             TASK_PRESETS[preset_index].full_text))
        {
          continue;
        }

      task_from_preset(&tasks[count], preset_index);
      count++;
      added++;
    }

  if (added == 0)
    {
      lv_label_set_text(g_ui.task_file_status,
                        count >= MAX_TASKS ? "任务已满，请先清理" :
                                             "今日计划已经准备好");
      lv_label_set_text(g_ui.task_plan_label, "已存在");
      return;
    }

  if (!save_tasks(tasks, count))
    {
      lv_label_set_text(g_ui.task_file_status, "今日计划保存失败");
      lv_label_set_text(g_ui.task_plan_label, "重试");
      return;
    }

  g_task_reload_pending = true;
  update_task_widgets();
  lv_label_set_text_fmt(g_ui.task_file_status, "今日计划已添加 %d 项",
                        (int)added);
  lv_label_set_text(g_ui.task_plan_label, "已生成");
}

#else

/* Keep the Windows visual preview interactive without inventing persistence.
 */

static void task_clear_done_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  lv_label_set_text(g_ui.task_file_status, "预览模式不会修改任务文件");
}

static void task_daily_plan_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  lv_label_set_text(g_ui.task_file_status, "真机将写入 3 项今日计划");
  lv_label_set_text(g_ui.task_plan_label, "预览");
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

/* Fill in the AI page's status rail.
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
          subtitle = "说\"你好，openvela\"唤醒";
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

  lv_label_set_text(g_ui.cpu_value, cpu_text);
  lv_label_set_text(g_ui.memory_value, memory_text);
  lv_label_set_text(g_ui.uptime_value, uptime_text);
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

    lv_label_set_text(g_ui.home_cpu, home_cpu);
    lv_label_set_text(g_ui.home_memory, home_memory);
    lv_label_set_text(g_ui.home_uptime, home_uptime);
  }

  network_online = read_network_address(network_text, sizeof(network_text));
  if (network_online)
    {
      lv_label_set_text(g_ui.network_value, network_text);
      lv_label_set_text(g_ui.home_network, "在线");
#ifdef _WIN32
      lv_label_set_text(g_ui.ai_connection, "网络正常，MiMo 已就绪");
#else
      lv_label_set_text(g_ui.ai_connection,
                        ai_chat_bridge_agent_running() ?
                        "网络正常，MiMo 已就绪" :
                        "网络正常，但 AI 服务未运行");
#endif
    }
  else
    {
      lv_label_set_text(g_ui.network_value, "离线");
      lv_label_set_text(g_ui.home_network, "离线");
      lv_label_set_text(g_ui.ai_connection, "正在等待网络");
    }

  if (g_ui.header_net_icon != NULL)
    {
      uint32_t net_color = network_online ? COLOR_GREEN : COLOR_CORAL;

      lv_obj_set_style_text_color(g_ui.header_net_icon, color(net_color), 0);
      lv_obj_set_style_text_color(g_ui.header_net_text, color(net_color), 0);
      lv_label_set_text(g_ui.header_net_text,
                        network_online ? "在线" : "离线");
    }

  update_ai_rail(network_online);

  /* Health lamp: worst-of the three things that stop this device working.
   * Offline is the loudest because nothing AI-facing functions without it;
   * a missing agent still leaves tasks and the timer usable, so it is a
   * warning rather than a fault.  The memory floor is set below where LVGL
   * starts failing allocations for a full-screen redraw.
   */

  if (g_ui.home_health != NULL)
    {
      const char *health_text;
      uint32_t health_color;
      bool low_memory = free_bytes > 0 &&
                        free_bytes < 2ULL * 1024ULL * 1024ULL;

      if (!network_online)
        {
          health_text = "离线";
          health_color = COLOR_CORAL;
        }
      else if (low_memory)
        {
          health_text = "内存紧张";
          health_color = COLOR_AMBER;
        }
#ifndef _WIN32
      else if (!ai_chat_bridge_agent_running())
        {
          health_text = "AI 未运行";
          health_color = COLOR_AMBER;
        }
#endif
      else
        {
          health_text = "正常";
          health_color = COLOR_GREEN;
        }

      lv_label_set_text(g_ui.home_health, health_text);
      lv_obj_set_style_text_color(g_ui.home_health, color(health_color), 0);
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
  if (clock_is_synced(wall_time))
    {
      strftime(clock_text, sizeof(clock_text), "%H:%M", &local_time);
    }
  else
    {
      snprintf(clock_text, sizeof(clock_text), "--:--");
    }

  lv_label_set_text(g_ui.header_clock, clock_text);
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

/* Keep the length and tally labels in step with the state.  Called after a
 * round completes, after the length changes, and once at startup.
 */

static void refresh_focus_labels(void)
{
#ifndef _WIN32
  const study_focus_day_t *latest = &g_ui.focus_stats.days[STUDY_FOCUS_DAYS - 1];
  g_ui.focus_display_day = focus_day_stamp();
  bool dated = g_ui.focus_display_day > 0 && g_ui.focus_display_day == latest->day;
  g_ui.focus_rounds = dated || latest->day == 0 ? latest->rounds : 0;
  g_ui.focus_minutes = dated || latest->day == 0 ? latest->minutes : 0;
  if (!dated && latest->day != 0)
    {
      g_ui.focus_rounds = g_ui.focus_stats.pending_rounds;
      g_ui.focus_minutes = g_ui.focus_stats.pending_minutes;
    }
#endif
  if (g_ui.focus_length_label != NULL)
    {
      lv_label_set_text_fmt(g_ui.focus_length_label, "%lu 分钟",
                            (unsigned long)focus_round_minutes());
    }

  if (g_ui.focus_stat_value != NULL)
    {
#ifndef _WIN32
      lv_label_set_text_fmt(g_ui.focus_stat_value, "%lu/%u 分钟",
                            (unsigned long)g_ui.focus_minutes,
                            (unsigned)g_ui.focus_stats.goal_minutes);
#else
      if (g_ui.focus_rounds == 0)
        {
          lv_label_set_text(g_ui.focus_stat_value, "今天还没开始");
        }
      else
        {
          lv_label_set_text_fmt(g_ui.focus_stat_value, "%lu 轮 / %lu 分钟",
                                (unsigned long)g_ui.focus_rounds,
                                (unsigned long)g_ui.focus_minutes);
        }
#endif
    }
#ifndef _WIN32
  refresh_focus_history();
  refresh_study_report();
#endif
}

/* Cycle the round length.  Refused mid-session: shrinking the round under a
 * running timer makes the arc jump backwards, and the minutes credited at the
 * end would no longer match what the user actually sat through.
 */

static void focus_length_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();

#ifndef _WIN32
  if (g_ui.reminder.id[0])
    {
      return;
    }
#endif

  if (g_ui.focus_running)
    {
      lv_label_set_text(g_ui.focus_status, "专注中不能改时长");
      return;
    }

  g_ui.focus_preset = (uint8_t)((g_ui.focus_preset + 1) % FOCUS_PRESET_COUNT);
  g_ui.focus_elapsed = 0;
  g_ui.focus_fraction_ms = 0;
  lv_arc_set_value(g_ui.focus_bar, 0);
  refresh_focus_labels();
  save_focus_stats();

  lv_label_set_text_fmt(g_ui.focus_status, "已设为 %lu 分钟",
                        (unsigned long)focus_round_minutes());
}

static void focus_event_cb(lv_event_t *event)
{
  (void)event;
  note_user_activity();
#ifndef _WIN32
  if (g_ui.reminder.id[0])
    {
      if (cron_cancel_reminder(g_ui.reminder.id) == 0)
        {
          g_ui.reminder_cancelling = true;
          lv_label_set_text(g_ui.focus_status, "正在取消提醒");
          lv_obj_add_state(g_ui.focus_button, LV_STATE_DISABLED);
        }
      else
        {
          lv_label_set_text(g_ui.focus_status, "暂时无法取消，请重试");
        }
      return;
    }
#endif
  bool want_running = !g_ui.focus_running;
  advance_focus_timer();
  g_ui.focus_running = want_running;
  g_ui.focus_tick_ms = lv_tick_get();

  if (g_ui.focus_running)
    {
      /* Starting from a finished round begins a new one.  Without this the
       * elapsed count is still at the full length and the next tick would
       * immediately "complete" the round again, crediting the tally for a
       * second of work.
       */

      if (g_ui.focus_elapsed >= focus_round_seconds())
        {
          g_ui.focus_elapsed = 0;
          lv_arc_set_value(g_ui.focus_bar, 0);
        }

      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PAUSE);
      lv_label_set_text(g_ui.focus_status, "专注进行中");
      start_focus_pulse();
    }
  else
    {
      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
      stop_focus_pulse();

      if (g_ui.on_break)
        {
          /* End the break instead of pausing it.  A rest you are forced to sit
           * through with no way out is not a rest.
           */

          g_ui.on_break = false;
          g_ui.focus_elapsed = 0;
          lv_arc_set_value(g_ui.focus_bar, 0);
          lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE),
                                     LV_PART_INDICATOR);
          lv_label_set_text(g_ui.focus_status, "已跳过休息");
        }
      else
        {
          lv_label_set_text(g_ui.focus_status, "已暂停");
        }
    }
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
#ifndef _WIN32
  g_ui.dialogue_mode = false;
  g_ui.tools_active = false;
  if (g_ui.notice_box) lv_obj_add_flag(g_ui.notice_box, LV_OBJ_FLAG_HIDDEN);
#endif
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

#ifndef _WIN32

static void reset_ptt_button(void)
{
  if (g_ui.voice_ptt_label != NULL)
    {
      lv_label_set_text(g_ui.voice_ptt_label, "按住说话");
      lv_obj_set_style_bg_color(g_ui.voice_ptt_button,
                                color(COLOR_OPENVELA), 0);
    }
}

static void cancel_voice_request(void)
{
  /* Cancel only our request. The worker safely finishes any pending start
   * or ASR, and the cleared ID prevents its result from reaching the agent.
   */

  voice_ui_bridge_ptt_cancel(g_ui.ptt_request_id);
  g_ui.ptt_request_id = 0;
  g_ui.ptt_held = false;
  g_ui.ptt_state = VOICE_UI_PTT_IDLE;
  g_ui.voice_notice = NULL;
  reset_ptt_button();
}

#endif

static void hide_voice_ui(void)
{
  g_ui.voice_active = false;
  g_ui.voice_phase = 0;

#ifndef _WIN32
  if (g_ui.notice_box) lv_obj_remove_flag(g_ui.notice_box, LV_OBJ_FLAG_HIDDEN);
  voice_ui_bridge_cancel_dialogue();
  g_ui.dialogue_animated = false;
  g_ui.dialogue_mode = false;
  cancel_voice_request();
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
  struct voice_ui_ptt_status_s status;

  if (g_ui.voice_status == NULL)
    {
      return;
    }

  voice_ui_bridge_ptt_get_status(&status);
  if (status.state == VOICE_UI_PTT_STARTING)
    {
      lv_label_set_text(g_ui.voice_status, "正在启动麦克风");
    }
  else if (status.state == VOICE_UI_PTT_RECORDING)
    {
      lv_label_set_text(g_ui.voice_status, "正在聆听");
    }
  else if (status.state == VOICE_UI_PTT_RECOGNIZING)
    {
      lv_label_set_text(g_ui.voice_status, "正在识别");
    }
  else if (status.state == VOICE_UI_PTT_CANCELLING)
    {
      lv_label_set_text(g_ui.voice_status, "正在结束上一段，请稍候");
    }
  else if (g_ui.dialogue.active || g_ui.dialogue.response_pending)
    {
      const char *label = "请继续说";
      switch (g_ui.dialogue.phase)
        {
          case VOICE_DIALOGUE_LISTENING: label = "正在聆听"; break;
          case VOICE_DIALOGUE_RECOGNIZING: label = "正在识别"; break;
          case VOICE_DIALOGUE_WAITING: label = "正在等待回答"; break;
          case VOICE_DIALOGUE_SLOW: label = "回复较慢，仍在等待"; break;
          case VOICE_DIALOGUE_SPEAKING: label = "正在播报"; break;
          default: break;
        }
      lv_label_set_text(g_ui.voice_status, label);
    }
  else if (g_ui.voice_notice != NULL)
    {
      lv_label_set_text(g_ui.voice_status, g_ui.voice_notice);
    }
  else if (ai_chat_bridge_is_busy())
    {
      lv_label_set_text(g_ui.voice_status,
                        ai_chat_bridge_pending_sec() >= 15 ?
                        "回复较慢，仍在等待" : "生成回答");
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
  uint32_t request_id;
  int ret;

  (void)event;

  note_user_activity();

  if (g_ui.ptt_held)
    {
      return;
    }

  /* One turn at a time: recording over an unanswered question would leave two
   * requests racing on the same channel.
   */

  if (g_ui.ptt_request_id != 0)
    {
      lv_label_set_text(g_ui.voice_status, "上一段语音处理中，请稍候");
      return;
    }

  if (ai_chat_bridge_is_busy())
    {
      lv_label_set_text(g_ui.voice_status, "正在等待上一条回复...");
      return;
    }

  voice_ui_bridge_get_dialogue(&g_ui.dialogue);
  if (g_ui.dialogue.response_pending)
    {
      lv_label_set_text(g_ui.voice_status, "正在等待上一条回复...");
      return;
    }
  voice_ui_bridge_cancel_dialogue();
  voice_ui_bridge_set_frontend_busy(true);

  ret = voice_ui_bridge_ptt_start(&request_id);
  if (ret < 0)
    {
      voice_ui_bridge_set_frontend_busy(false);
      g_ui.voice_notice = ret == -EBUSY ? NULL : "语音启动失败，请重试";
      lv_label_set_text(g_ui.voice_status, ret == -EBUSY ?
                        "上一段语音处理中，请稍候" : g_ui.voice_notice);
      return;
    }

  g_ui.ptt_request_id = request_id;
  g_ui.ptt_held = true;
  g_ui.ptt_state = VOICE_UI_PTT_STARTING;
  g_ui.voice_notice = NULL;
  lv_label_set_text(g_ui.voice_ptt_label, "松手结束");
  lv_obj_set_style_bg_color(g_ui.voice_ptt_button, color(COLOR_CORAL), 0);
  lv_label_set_text(g_ui.voice_status, "正在启动麦克风");
  lv_label_set_text(g_ui.voice_heard, "麦克风就绪后开始说话");
}

static void voice_ptt_released_cb(lv_event_t *event)
{
  int ret;

  (void)event;

  if (!g_ui.ptt_held)
    {
      return;
    }

  g_ui.ptt_held = false;
  ret = voice_ui_bridge_ptt_stop(g_ui.ptt_request_id);
  stop_voice_wave();
  reset_ptt_button();

  if (ret < 0)
    {
      cancel_voice_request();
      g_ui.voice_notice = "语音已中断，请重试";
      update_voice_widgets();
      return;
    }

  lv_label_set_text(g_ui.voice_status, "正在识别");
}

/* All recorder results are consumed on the LVGL thread. A hidden page or an
 * old request ID can never submit a transcript, even if ASR finishes late.
 */

static void poll_wake_dialogue(void)
{
  uint32_t revision = g_ui.dialogue.revision;
  voice_ui_bridge_get_dialogue(&g_ui.dialogue);
  if (g_ui.dialogue.active && g_ui.dialogue.session != g_ui.dialogue_seen)
    {
      g_ui.dialogue_seen = g_ui.dialogue.session;
      g_ui.voice_notice = NULL;
      show_voice_ui();
      g_ui.dialogue_mode = true;
    }
  if (g_ui.dialogue_mode && revision != g_ui.dialogue.revision &&
      g_ui.dialogue.session != 0)
    {
      if (g_ui.dialogue.heard[0])
        {
          lv_label_set_text(g_ui.voice_heard, g_ui.dialogue.heard);
        }
      if (g_ui.dialogue.reply[0])
        {
          lv_label_set_text(g_ui.voice_reply, g_ui.dialogue.reply);
        }
      if (g_ui.dialogue.phase == VOICE_DIALOGUE_SPEAKING)
        {
          remember_recent_reply(g_ui.dialogue.reply);
          if (g_ui.ai_recent_text != NULL)
            {
              lv_label_set_text(g_ui.ai_recent_text, g_last_reply);
            }
        }
      update_voice_widgets();
    }
  bool animated = g_ui.voice_active && g_ui.dialogue.active &&
                  g_ui.dialogue.phase == VOICE_DIALOGUE_LISTENING;
  if (animated != g_ui.dialogue_animated)
    {
      g_ui.dialogue_animated = animated;
      if (animated) start_voice_wave();
      else stop_voice_wave();
    }
  if (g_ui.dialogue.active || g_ui.dialogue.response_pending)
    {
      lv_obj_add_state(g_ui.voice_ptt_button, LV_STATE_DISABLED);
      lv_label_set_text(g_ui.voice_ptt_label, "自动对话");
    }
  else if (lv_obj_has_state(g_ui.voice_ptt_button, LV_STATE_DISABLED))
    {
      lv_obj_remove_state(g_ui.voice_ptt_button, LV_STATE_DISABLED);
      reset_ptt_button();
    }
}

static void voice_ptt_timer_cb(lv_timer_t *timer)
{
  struct voice_ui_ptt_status_s status;

  (void)timer;

  voice_ui_bridge_set_frontend_busy(g_ui.ptt_request_id != 0 ||
                                    ai_chat_bridge_is_busy() ||
                                    voice_ui_bridge_is_speaking());
  poll_wake_dialogue();

  if (!g_ui.voice_active || g_ui.ptt_request_id == 0)
    {
      return;
    }

  voice_ui_bridge_ptt_get_status(&status);
  if (status.request_id != g_ui.ptt_request_id)
    {
      cancel_voice_request();
      g_ui.voice_notice = "语音已中断，请重试";
      update_voice_widgets();
      return;
    }

  if (status.state == g_ui.ptt_state)
    {
      return;
    }

  g_ui.ptt_state = status.state;
  if (status.state == VOICE_UI_PTT_RECORDING)
    {
      lv_label_set_text(g_ui.voice_heard, "正在聆听，松手结束");
      start_voice_wave();
      update_voice_widgets();
      return;
    }

  if (status.state == VOICE_UI_PTT_STARTING ||
      status.state == VOICE_UI_PTT_RECOGNIZING ||
      status.state == VOICE_UI_PTT_CANCELLING)
    {
      stop_voice_wave();
      update_voice_widgets();
      return;
    }

  g_ui.ptt_request_id = 0;
  g_ui.ptt_held = false;
  reset_ptt_button();
  stop_voice_wave();

  if (status.state == VOICE_UI_PTT_START_FAILED ||
      status.state == VOICE_UI_PTT_ASR_FAILED)
    {
      g_ui.voice_notice = status.state == VOICE_UI_PTT_START_FAILED ?
                          "麦克风启动失败，请重试" : "识别失败，请重试";
      lv_label_set_text(g_ui.voice_heard, "没听清，请再说一次");
      update_voice_widgets();
      return;
    }

  if (status.state != VOICE_UI_PTT_DONE)
    {
      update_voice_widgets();
      return;
    }

  if (status.text[0] == '\0')
    {
      g_ui.voice_notice = "未识别到内容，请重试";
      lv_label_set_text(g_ui.voice_heard, "没听到声音");
      update_voice_widgets();
      return;
    }

  lv_label_set_text(g_ui.voice_heard, status.text);

  if (!ai_chat_bridge_agent_running())
    {
      g_ui.voice_notice = "AI 服务未运行";
      lv_label_set_text(g_ui.voice_reply,
                        "AI 服务未启动，请在终端执行 ai_agent &");
      update_voice_widgets();
      return;
    }

  if (ai_chat_bridge_send(status.text) != 0)
    {
      g_ui.voice_notice = "发送失败，请重试";
      lv_label_set_text(g_ui.voice_reply, "发送失败，请重试");
      update_voice_widgets();
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
  lv_obj_set_size(stage, g_compact_layout ? 72 : 168,
                        g_compact_layout ? 72 : 168);
  lv_obj_set_style_pad_all(stage, 0, 0);

  g_ui.voice_ring_outer = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_outer);
  lv_obj_add_flag(g_ui.voice_ring_outer, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_outer, g_compact_layout ? 56 : 150,
                                        g_compact_layout ? 56 : 150);
  lv_obj_center(g_ui.voice_ring_outer);
  lv_obj_set_style_radius(g_ui.voice_ring_outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_outer, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_outer, color(0x3d6fff), 0);
  prepare_voice_ring(g_ui.voice_ring_outer);

  g_ui.voice_ring_mid = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_mid);
  lv_obj_add_flag(g_ui.voice_ring_mid, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_mid, g_compact_layout ? 56 : 150,
                                      g_compact_layout ? 56 : 150);
  lv_obj_center(g_ui.voice_ring_mid);
  lv_obj_set_style_radius(g_ui.voice_ring_mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_mid, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_mid, color(0x75a0ff), 0);
  prepare_voice_ring(g_ui.voice_ring_mid);

  g_ui.voice_ring_inner = lv_obj_create(stage);
  object_reset(g_ui.voice_ring_inner);
  lv_obj_add_flag(g_ui.voice_ring_inner, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_ring_inner, g_compact_layout ? 56 : 150,
                                        g_compact_layout ? 56 : 150);
  lv_obj_center(g_ui.voice_ring_inner);
  lv_obj_set_style_radius(g_ui.voice_ring_inner, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(g_ui.voice_ring_inner, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.voice_ring_inner, 2, 0);
  lv_obj_set_style_border_color(g_ui.voice_ring_inner, color(0xa8c4ff), 0);
  prepare_voice_ring(g_ui.voice_ring_inner);

  g_ui.voice_avatar = lv_obj_create(stage);
  object_reset(g_ui.voice_avatar);
  lv_obj_add_flag(g_ui.voice_avatar, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(g_ui.voice_avatar, g_compact_layout ? 40 : 72,
                                    g_compact_layout ? 40 : 72);
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
  lv_obj_add_flag(chat, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(chat, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(chat, LV_SCROLLBAR_MODE_AUTO);
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

  g_ui.ptt_held = false;

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

static void show_local_study_suggestion(int pending, int completed,
                                        const char *subject)
{
  char suggestion[192];
  const char *display_subject = subject;

#ifndef _WIN32
  display_subject = task_display_text(subject);
#endif

  if (pending > 0 && subject != NULL && subject[0] != '\0')
    {
      snprintf(suggestion, sizeof(suggestion),
               "本地建议：先完成“%s”，专注 %lu 分钟后做一次复盘。",
               display_subject,
               (unsigned long)focus_round_minutes());
    }
  else if (completed > 0)
    {
      snprintf(suggestion, sizeof(suggestion),
               "本地建议：今天的任务已经完成，可以复盘收获或预习下一节。");
    }
  else
    {
      snprintf(suggestion, sizeof(suggestion),
               "本地建议：先到任务页生成今日计划，再开启一轮专注。");
    }

#ifndef _WIN32
  remember_recent_reply(suggestion);
#endif

  if (g_ui.ai_recent_text != NULL)
    {
      lv_label_set_text(g_ui.ai_recent_text, suggestion);
    }

  if (g_ui.voice_reply != NULL && g_ui.voice_active)
    {
      lv_label_set_text(g_ui.voice_reply, suggestion);
    }

  if (g_ui.ai_connection != NULL)
    {
      lv_label_set_text(g_ui.ai_connection, "云端不可用，已给出本地建议");
    }

  if (g_ui.ai_coach_label != NULL)
    {
      lv_label_set_text(g_ui.ai_coach_label, "再给建议");
    }
}

/* Send a compact snapshot of the real study state to MiMo.  This gives the
 * contest demo a reliable text entry point when microphone/ASR is unavailable.
 * If cloud prerequisites are missing, the same button returns a plainly
 * labelled local suggestion instead of failing silently or faking an AI reply.
 */

static void ai_coach_event_cb(lv_event_t *event)
{
  int pending;
  int completed;
  char subject[96] = "";

  (void)event;
  note_user_activity();
  start_ai_pulse();
  read_task_counts(&pending, &completed);

#ifndef _WIN32
  {
    char pending_task[MAX_TASK_TEXT + 1];
    char network_text[64];
    char prompt[384];

    if (first_pending_task(pending_task, sizeof(pending_task)))
      {
        snprintf(subject, sizeof(subject), "%s", pending_task);
      }

    if (ai_chat_bridge_is_busy())
      {
        lv_label_set_text(g_ui.ai_connection, "已有请求正在处理中");
        lv_label_set_text(g_ui.ai_coach_label, "处理中");
        return;
      }

    if (read_network_address(network_text, sizeof(network_text)) &&
        ai_chat_bridge_agent_running())
      {
        snprintf(prompt, sizeof(prompt),
                 "你是学习终端教练。当前待办%d项，已完成%d项，今日专注%lu分钟，"
                 "下一项是%s。请用不超过80个汉字给出具体下一步建议；不要虚构"
                 "设备状态，不要输出Markdown标题。",
                 pending, completed, (unsigned long)g_ui.focus_minutes,
                 subject[0] != '\0' ? task_display_text(subject) :
                                        "暂无待办");

        if (ai_chat_bridge_send(prompt) == 0)
          {
            lv_label_set_text(g_ui.ai_connection, "已发送真实学习状态");
            lv_label_set_text(g_ui.ai_coach_label, "生成中");
            if (g_ui.ai_recent_text != NULL)
              {
                lv_label_set_text(g_ui.ai_recent_text,
                                  "正在生成个性化学习建议...");
              }
            return;
          }
      }
  }
#else
  snprintf(subject, sizeof(subject), "当前优先任务");
#endif

  show_local_study_suggestion(pending, completed, subject);
}

static void ai_check_event_cb(lv_event_t *event)
{
  char network_text[64];

  (void)event;
  note_user_activity();
  start_ai_pulse();
  lv_label_set_text(g_ui.ai_check_label, LV_SYMBOL_AUDIO);
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
#ifndef _WIN32
      cancel_voice_request();
#endif
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

/* A wall clock this far past the epoch has been set by something real (ntpc,
 * or a warm reboot that kept RAM).  Anything below it means the clock has not
 * been synced yet: this board has no battery-backed RTC, so a cold boot starts
 * at 0 and every displayed timestamp would be fiction.
 *
 * 2020-01-01 UTC.  Comfortably after any plausible build date, comfortably
 * before any real "now".
 */

#define CLOCK_SYNCED_EPOCH 1577836800

#ifndef _WIN32

/* Render times in China Standard Time.
 *
 * ntpc gives us correct UTC, but libc needs to be told the zone or it renders
 * UTC and the clock reads 8 hours slow.  The POSIX offset form is used because
 * "Asia/Shanghai" would need a tzdata file under CONFIG_LIBC_TZDIR
 * (/etc/zoneinfo), and none ship in this image.
 *
 * The sign is inverted by that syntax: -8 means 8 hours east of UTC.  No DST
 * rule follows because China dropped DST in 1991.
 */

static void apply_local_timezone(void)
{
  setenv("TZ", "CST-8", 1);
  tzset();
}
#endif

#ifndef _WIN32

/* Cities offered by the on-screen picker.  A short list of the obvious ones
 * beats a keyboard: AMap takes a plain name, and anyone needing somewhere else
 * can still set weather.city from the console.
 */

static const char * const g_city_choices[] =
{
  "深圳", "广州", "北京", "上海", "杭州", "成都", "武汉", "西安"
};

#define CITY_CHOICE_COUNT \
  (sizeof(g_city_choices) / sizeof(g_city_choices[0]))

/* Read one key out of the agent's config store.  Returns false when the file
 * or the key is missing, which is the normal state before first setup.
 */

static bool agent_config_read(const char *key, char *out, size_t out_size)
{
  FILE *file;
  long size;
  char *text;
  cJSON *root;
  cJSON *item;
  bool ok = false;

  file = fopen(AGENT_CONFIG_PATH, "r");
  if (file == NULL)
    {
      return false;
    }

  if (fseek(file, 0, SEEK_END) != 0)
    {
      fclose(file);
      return false;
    }

  size = ftell(file);

  /* 64 KiB ceiling: this file is a handful of short strings, and a bogus size
   * should not turn into a large allocation on a 128 MiB device.
   */

  if (size <= 0 || size > 65536 || fseek(file, 0, SEEK_SET) != 0)
    {
      fclose(file);
      return false;
    }

  text = malloc((size_t)size + 1);
  if (text == NULL)
    {
      fclose(file);
      return false;
    }

  size = (long)fread(text, 1, (size_t)size, file);
  fclose(file);
  text[size > 0 ? size : 0] = '\0';

  root = cJSON_Parse(text);
  free(text);

  if (root == NULL)
    {
      return false;
    }

  item = cJSON_GetObjectItem(root, key);
  if (item != NULL && cJSON_IsString(item) && item->valuestring[0] != '\0')
    {
      snprintf(out, out_size, "%s", item->valuestring);
      ok = true;
    }

  cJSON_Delete(root);
  return ok;
}

/* Write one key, preserving everything else.  The file also holds the LLM
 * endpoint, WiFi credentials and API keys, so this reads, edits and writes
 * back rather than emitting a fresh object -- the latter would erase the
 * device's configuration to save one string.
 */

static bool agent_config_write(const char *key, const char *value)
{
  FILE *file;
  long size;
  char *text = NULL;
  cJSON *root = NULL;
  char *rendered;
  char temp_path[96];
  bool ok = false;

  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      return false;
    }

  if (mkdir(AGENT_CONFIG_DIR, 0777) != 0 && errno != EEXIST)
    {
      return false;
    }

  file = fopen(AGENT_CONFIG_PATH, "r");
  if (file != NULL)
    {
      if (fseek(file, 0, SEEK_END) == 0)
        {
          size = ftell(file);
          if (size > 0 && size <= 65536 && fseek(file, 0, SEEK_SET) == 0)
            {
              text = malloc((size_t)size + 1);
              if (text != NULL)
                {
                  size = (long)fread(text, 1, (size_t)size, file);
                  text[size > 0 ? size : 0] = '\0';
                  root = cJSON_Parse(text);
                  free(text);
                }
            }
        }

      fclose(file);
    }

  if (root == NULL)
    {
      /* No file yet, or unparseable.  Starting fresh is correct for the first
       * case; for the second the alternative is refusing to save at all.
       */

      root = cJSON_CreateObject();
      if (root == NULL)
        {
          return false;
        }
    }

  cJSON_DeleteItemFromObject(root, key);
  if (cJSON_AddStringToObject(root, key, value) == NULL)
    {
      cJSON_Delete(root);
      return false;
    }

  rendered = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  if (rendered == NULL)
    {
      return false;
    }

  snprintf(temp_path, sizeof(temp_path), "%s.tmp", AGENT_CONFIG_PATH);

  file = fopen(temp_path, "w");
  if (file != NULL)
    {
      if (fputs(rendered, file) >= 0 && fflush(file) == 0)
        {
          ok = true;
        }

      fclose(file);

      /* tmp + rename so the agent never reads a half-written config. */

      if (ok && rename(temp_path, AGENT_CONFIG_PATH) != 0)
        {
          unlink(temp_path);
          ok = false;
        }
      else if (!ok)
        {
          unlink(temp_path);
        }
    }

  free(rendered);
  return ok;
}
#endif

static bool clock_is_synced(time_t wall_time)
{
  return wall_time >= (time_t)CLOCK_SYNCED_EPOCH;
}

#ifndef _WIN32

/* Pull one string field out of the weather file ai_agent publishes.
 *
 * The file is small and flat, so a bounded scan for "\"key\"" followed by the
 * next quoted token is enough -- linking a JSON parser into the UI for four
 * fields is not worth the footprint.  Returns false on any malformed input,
 * which the caller renders as "天气未配置" rather than a partial reading.
 */

static bool weather_field(const char *key, char *out, size_t out_size)
{
  FILE *file;
  char buffer[512];
  size_t got;
  char pattern[32];
  char *cursor;
  char *end;

  file = fopen(WEATHER_FILE, "r");
  if (file == NULL)
    {
      return false;
    }

  got = fread(buffer, 1, sizeof(buffer) - 1, file);
  fclose(file);
  buffer[got] = '\0';

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  cursor = strstr(buffer, pattern);
  if (cursor == NULL)
    {
      return false;
    }

  cursor = strchr(cursor + strlen(pattern), ':');
  if (cursor == NULL)
    {
      return false;
    }

  cursor = strchr(cursor, '"');
  if (cursor == NULL)
    {
      return false;
    }

  cursor++;
  end = strchr(cursor, '"');
  if (end == NULL || (size_t)(end - cursor) >= out_size)
    {
      return false;
    }

  memcpy(out, cursor, (size_t)(end - cursor));
  out[end - cursor] = '\0';
  return out[0] != '\0';
}

#endif

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

  /* Weather comes from the file ai_agent publishes.  When the key is not
   * configured or the fetch has not landed yet, say so -- the previous code
   * invented a forecast from the hour of the day, which reads as a real
   * reading and is not one.
   */

  weather_text = "天气未配置";
  temp_text = "--";

#ifndef _WIN32
  {
    static char weather_buf[32];
    static char temp_buf[16];

    if (weather_field("text", weather_buf, sizeof(weather_buf)))
      {
        weather_text = weather_buf;
      }

    if (weather_field("temp", temp_buf, sizeof(temp_buf)))
      {
        /* The provider hands back a bare number; the degree marker belongs to
         * presentation, so it is added here rather than stored in the file.
         */

        size_t used = strlen(temp_buf);

        if (used > 0 && used + 1 < sizeof(temp_buf) &&
            temp_buf[used - 1] != 'C')
          {
            temp_buf[used] = 'C';
            temp_buf[used + 1] = '\0';
          }

        temp_text = temp_buf;
      }
  }
#endif

  if (!clock_is_synced(wall_time))
    {
      /* No RTC on this board, so a cold boot starts at the epoch.  Showing
       * "00:07  01-01 周四" invites the user to think the device is broken;
       * naming the real state does not.
       */

      lv_label_set_text(g_ui.standby_time, "--:--");
      lv_label_set_text(g_ui.standby_date, "正在同步时间");
    }
  else
    {
      lv_label_set_text(g_ui.standby_time, time_text);
      lv_label_set_text(g_ui.standby_date, date_text);
    }

  lv_label_set_text(g_ui.standby_weather, weather_text);
  lv_label_set_text(g_ui.standby_temp, temp_text);
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
  badge = create_icon_badge(weather_row, LV_SYMBOL_REFRESH, COLOR_BLUE, 26);
  (void)badge;
  g_ui.standby_weather = create_label(weather_row, "天气未配置", ui_font(),
                                      COLOR_WHITE);
  lv_obj_set_height(g_ui.standby_weather, 24);
  g_ui.standby_temp = create_label(weather_row, "--",
                                   &lv_font_montserrat_20, COLOR_BLUE);
  lv_obj_set_height(g_ui.standby_temp, 28);

  g_ui.standby_hint = create_label(panel, "触摸屏幕进入主页", ui_font(),
                                   0x75a0ff);
  lv_obj_set_width(g_ui.standby_hint, LV_PCT(100));
  lv_obj_set_height(g_ui.standby_hint, 22);
  lv_obj_set_style_text_align(g_ui.standby_hint, LV_TEXT_ALIGN_CENTER, 0);
  update_standby_widgets();
}

#ifndef _WIN32
static int64_t reminder_remaining(const cron_reminder_snapshot_t *reminder)
{
  int64_t remaining;
  if (reminder->awaiting_clock) return -1;
  if (reminder->deadline_mono_ms)
    {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      int64_t mono = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
      remaining = reminder->deadline_mono_ms - mono;
      return remaining > 0 ? (remaining + 999) / 1000 : 0;
    }
  remaining = reminder->deadline - (int64_t)time(NULL);
  return remaining > 0 ? remaining : 0;
}

static void refresh_reminder_ring(void)
{
  cron_reminder_snapshot_t next;
  bool had_reminder = g_ui.reminder.id[0] != '\0';
  time_t now = time(NULL);

  /* The scheduler owns the deadline. Never start a second countdown here. */
  if (cron_get_next_reminder(&next) == 0)
    {
      if (strcmp(next.id, g_ui.reminder.id) != 0 ||
          next.deadline != g_ui.reminder.deadline)
        {
          g_ui.reminder_cancelling = false;
          if (next.id[0])
            {
              stop_focus_pulse();
            }
        }
      if (next.id[0] && (next.started_at <= 0 ||
                        next.started_at >= next.deadline))
        {
          next.started_at = strcmp(next.id, g_ui.reminder.id) == 0 ?
                            g_ui.reminder.started_at : (int64_t)now;
          if (next.started_at >= next.deadline)
            {
              next.started_at = next.deadline - 1;
            }
        }
      g_ui.reminder = next;
      g_ui.reminder_cancelling = next.cancel_result == -EINPROGRESS;
    }

  if (!g_ui.reminder.id[0])
    {
      if (had_reminder)
        {
          uint32_t total = g_ui.on_break ? BREAK_SECONDS : focus_round_seconds();
          lv_label_set_text(g_ui.home_focus_title, "今日专注");
          update_task_widgets();
          refresh_focus_labels();
          lv_obj_remove_state(g_ui.focus_button, LV_STATE_DISABLED);
          lv_obj_remove_state(lv_obj_get_parent(g_ui.focus_length_label),
                              LV_STATE_DISABLED);
          lv_obj_set_style_bg_color(g_ui.focus_button, color(COLOR_BLUE), 0);
          lv_obj_set_style_arc_color(g_ui.focus_bar,
                                     color(g_ui.on_break ? COLOR_TEAL : COLOR_BLUE),
                                     LV_PART_INDICATOR);
          lv_arc_set_value(g_ui.focus_bar, g_ui.focus_elapsed * 100 / total);
          lv_label_set_text(g_ui.focus_button_label,
                            g_ui.focus_running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
          lv_label_set_text(g_ui.focus_status,
                            g_ui.focus_running ? "专注进行中" : "准备就绪");
          if (g_ui.focus_running) start_focus_pulse();
        }
      return;
    }

  int64_t total = g_ui.reminder.duration_s ? g_ui.reminder.duration_s :
                  g_ui.reminder.deadline - g_ui.reminder.started_at;
  int64_t remaining = reminder_remaining(&g_ui.reminder);
  if (remaining < 0) remaining = 0;
  int64_t elapsed = total - remaining;
  if (elapsed < 0) elapsed = 0;
  if (total <= 0) total = 1;
  if (elapsed > total) elapsed = total;
  lv_arc_set_value(g_ui.focus_bar, (int32_t)(elapsed * 100 / total));
  lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_AMBER), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(g_ui.focus_button, color(COLOR_CORAL), 0);
  lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_CLOSE);
  lv_obj_add_state(lv_obj_get_parent(g_ui.focus_length_label), LV_STATE_DISABLED);
  if (!g_ui.reminder_cancelling)
    {
      lv_obj_remove_state(g_ui.focus_button, LV_STATE_DISABLED);
    }
  lv_label_set_text(g_ui.home_focus_title, "提醒倒计时");
  lv_label_set_text(g_ui.home_focus_subject, g_ui.reminder.message);
  lv_label_set_text(g_ui.focus_length_label, "提醒");
  lv_label_set_text(g_ui.focus_status, g_ui.reminder_cancelling ? "正在取消提醒" :
                    (g_ui.reminder.cancel_result < 0 ? "取消失败，请重试" :
                     (g_ui.reminder.awaiting_clock ? "等待校时" :
                     (remaining == 0 ? "提醒已到时" : "倒计时中"))));
  if (g_ui.reminder.awaiting_clock)
    {
      lv_arc_set_value(g_ui.focus_bar, 0);
      lv_label_set_text(g_ui.focus_time, "--:--");
    }
  else if (remaining >= 3600)
    {
      lv_label_set_text_fmt(g_ui.focus_time, "%luh%02lu",
                            (unsigned long)(remaining / 3600),
                            (unsigned long)(remaining / 60 % 60));
    }
  else
    {
      lv_label_set_text_fmt(g_ui.focus_time, "%02lu:%02lu",
                            (unsigned long)(remaining / 60),
                            (unsigned long)(remaining % 60));
    }
}

static lv_obj_t *tools_button(lv_obj_t *parent, const char *text, int32_t width,
                             lv_event_cb_t callback, void *data)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_size(button, width, 40);
  lv_obj_set_style_radius(button, 6, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_t *label = create_label(button, text, ui_font(), COLOR_WHITE);
  lv_obj_center(label);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, data);
  return button;
}

static void tools_close_event_cb(lv_event_t *event)
{
  (void)event;
  g_ui.tools_active = false;
  note_user_activity();
  show_main_ui();
}

static void show_tools_event_cb(lv_event_t *event)
{
  if (!g_ui.tools_screen) return;
  note_user_activity();
  g_ui.tools_active = true;
  g_ui.on_standby = false;
  lv_tabview_set_active(g_ui.tools_tabs,
                       event ? (uint32_t)(uintptr_t)lv_event_get_user_data(event) : 0,
                       LV_ANIM_OFF);
  refresh_local_tools();
  refresh_focus_labels();
  lv_screen_load(g_ui.tools_screen);
}

static int queue_local_reminder(uint32_t seconds, const char *message)
{
  note_user_activity();
  if (g_ui.reminder_request) return -EBUSY;
  int result = cron_create_reminder(seconds, message, &g_ui.reminder_request);
  lv_label_set_text(g_ui.reminder_status, result == 0 ? "正在添加提醒" :
                    result == -ENODEV ? "提醒服务未启动" : "服务忙，请重试");
  if (result == 0)
    {
      lv_obj_add_state(g_ui.reminder_add, LV_STATE_DISABLED);
      for (unsigned i = 0; i < 3; i++)
        lv_obj_add_state(g_ui.reminder_quick[i], LV_STATE_DISABLED);
    }
  return result;
}

static void reminder_add_event_cb(lv_event_t *event)
{
  static const uint32_t durations[] = {10, 30, 60, 180, 300, 600, 1200, 1800, 3600};
  static const char *messages[] = {"该休息一下了", "喝杯水，放松一下",
                                  "看看远处，让眼睛休息", "开始你的学习任务吧"};
  (void)event;
  unsigned duration = lv_dropdown_get_selected(g_ui.reminder_duration);
  unsigned kind = lv_dropdown_get_selected(g_ui.reminder_kind);
  if (duration < sizeof(durations) / sizeof(durations[0]) && kind < 4)
    queue_local_reminder(durations[duration], messages[kind]);
}

static void reminder_quick_event_cb(lv_event_t *event)
{
  unsigned preset = (unsigned)(uintptr_t)lv_event_get_user_data(event);
  static const uint32_t seconds[] = {300, 1200, 3600};
  static const char *messages[] = {"喝杯水，放松一下", "看看远处，让眼睛休息",
                                  "起来活动一下吧"};
  if (preset < 3) queue_local_reminder(seconds[preset], messages[preset]);
}

static void reminder_cancel_event_cb(lv_event_t *event)
{
  struct reminder_row_s *row = lv_event_get_user_data(event);
  if (lv_event_get_code(event) == LV_EVENT_PRESSED)
    {
      memcpy(row->pressed_id, row->id, sizeof(row->id));
      return;
    }
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  const char *id = row->pressed_id[0] ? row->pressed_id : row->id;
  note_user_activity();
  int result = cron_cancel_reminder(id);
  if (result == 0)
    {
      memcpy(g_ui.reminder_cancel_id, id, sizeof(g_ui.reminder_cancel_id));
      lv_obj_add_state(row->cancel, LV_STATE_DISABLED);
    }
  row->pressed_id[0] = '\0';
  lv_label_set_text(g_ui.reminder_status, result == 0 ? "正在取消提醒" : "取消失败，请重试");
}

static void close_local_notice(lv_event_t *event)
{
  (void)event;
  note_user_activity();
  if (g_ui.notice_box)
    {
      lv_obj_delete(g_ui.notice_box);
      g_ui.notice_box = NULL;
      g_ui.notice_snooze_button = NULL;
      g_ui.notice_ack_button = NULL;
      g_ui.notice_snooze_pending = false;
    }
}

static void snooze_notice_event_cb(lv_event_t *event)
{
  if (queue_local_reminder(300, g_ui.notice_message) == 0)
    {
      g_ui.notice_snooze_pending = true;
      lv_obj_add_state(g_ui.notice_snooze_button, LV_STATE_DISABLED);
      lv_obj_add_state(g_ui.notice_ack_button, LV_STATE_DISABLED);
      lv_label_set_text(lv_obj_get_child(g_ui.notice_snooze_button, 0), "正在添加");
    }
  else if (event)
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(event), 0), "添加失败，重试");
}

static void show_local_notice(const char *title, const char *message, bool can_snooze)
{
  /* Leave foreground capture and the current chat request alone. Cron events
   * stay queued until the voice screen closes. The focus status still updates. */
  if (g_ui.voice_active || g_ui.ptt_request_id || g_ui.notice_box)
    {
      if (!can_snooze)
        {
          snprintf(g_ui.pending_focus_title, sizeof(g_ui.pending_focus_title), "%s", title);
          snprintf(g_ui.pending_focus_message, sizeof(g_ui.pending_focus_message), "%s", message);
        }
      return;
    }
  if (!can_snooze) g_ui.pending_focus_title[0] = '\0';
  if (g_ui.on_standby) show_main_ui();
  note_user_activity();
  snprintf(g_ui.notice_message, sizeof(g_ui.notice_message), "%s", message);
  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  object_reset(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, color(COLOR_TEXT), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_40, 0);
  g_ui.notice_box = overlay;
  lv_obj_t *panel = create_group(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(panel, g_compact_layout ? LV_PCT(90) : 480, 188);
  lv_obj_set_style_bg_color(panel, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, 12, 0);
  lv_obj_set_style_pad_row(panel, 8, 0);
  lv_obj_center(panel);
  create_label(panel, title, ui_font(), COLOR_TEAL);
  lv_obj_t *body = create_group(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(body, LV_PCT(100), 1);
  lv_obj_set_flex_grow(body, 1);
  lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  lv_obj_t *text = create_label(body, message, ui_font(), COLOR_TEXT);
  lv_obj_set_width(text, LV_PCT(100));
  lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
  lv_obj_t *actions = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(actions, LV_PCT(100), 40);
  lv_obj_set_style_pad_column(actions, 8, 0);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (can_snooze)
    g_ui.notice_snooze_button = tools_button(actions, "5 分钟后再提醒", 150,
                                            snooze_notice_event_cb, NULL);
  g_ui.notice_ack_button = tools_button(actions, "知道了", 84, close_local_notice, NULL);
  if (g_ui.volume_slider && lv_slider_get_value(g_ui.volume_slider) > 0)
    voice_ui_bridge_chime();
}

static void goal_event_cb(lv_event_t *event)
{
  (void)event;
  static const uint16_t goals[] = {30, 60, 90};
  unsigned selected = lv_buttonmatrix_get_selected_button(g_ui.goal_control);
  if (selected < 3)
    {
      note_user_activity();
      g_ui.focus_stats.goal_minutes = goals[selected];
      save_focus_stats();
      refresh_focus_labels();
    }
}

static void refresh_focus_history(void)
{
  if (!g_ui.goal_summary) return;
  uint32_t goal = g_ui.focus_stats.goal_minutes;
  int day = focus_day_stamp();
  bool synced = day > 0 && day == g_ui.focus_stats.days[STUDY_FOCUS_DAYS - 1].day;
  lv_label_set_text_fmt(g_ui.goal_summary, "%s %lu / %lu 分钟",
                        synced ? "今日专注" : "待校时",
                        (unsigned long)g_ui.focus_minutes, (unsigned long)goal);
  lv_bar_set_value(g_ui.goal_progress, g_ui.focus_minutes >= goal ? 100 :
                   (int32_t)(g_ui.focus_minutes * 100 / goal), LV_ANIM_OFF);
  unsigned selected = goal == 30 ? 0 : goal == 90 ? 2 : 1;
  lv_buttonmatrix_set_button_ctrl(g_ui.goal_control, selected, LV_BUTTONMATRIX_CTRL_CHECKED);
  uint32_t largest = goal;
  uint64_t week = 0;
  for (unsigned i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      uint32_t minutes = g_ui.focus_stats.days[i].minutes;
      if (minutes > largest) largest = minutes;
      week += minutes;
    }
  for (unsigned i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      const study_focus_day_t *entry = &g_ui.focus_stats.days[i];
      lv_label_set_text_fmt(g_ui.history_values[i], "%lu", (unsigned long)entry->minutes);
      lv_bar_set_value(g_ui.history_bars[i],
                       (int32_t)((uint64_t)entry->minutes * 100 / largest), LV_ANIM_OFF);
      if (entry->day)
        lv_label_set_text_fmt(g_ui.history_dates[i], "%02d/%02d", entry->day / 100 % 100, entry->day % 100);
      else lv_label_set_text(g_ui.history_dates[i], "--");
    }
  if (g_ui.focus_save_error)
    lv_label_set_text(g_ui.focus_history_note, "记录未保存，下次完成后重试");
  else if (!synced)
    lv_label_set_text(g_ui.focus_history_note, "本次记录待校时，历史记录保留");
  else
    lv_label_set_text_fmt(g_ui.focus_history_note, "近 7 天 %llu 分钟  /  连学 %u 天",
                          (unsigned long long)week, study_focus_stats_streak(&g_ui.focus_stats));
}

/* Roll the same focus_stats the other two tabs read into one summary.  Nothing
 * is cached: recomputing on every refresh is what stops the report, the
 * seven-day chart and the home page tally from disagreeing after a midnight
 * rollover or a failed save.
 */

static void refresh_study_report(void)
{
  uint32_t goal;
  uint64_t total;
  uint32_t best;
  unsigned active;
  uint32_t week_goal;
  int day;
  bool synced;

  if (g_ui.report_today == NULL)
    {
      return;
    }

  goal = g_ui.focus_stats.goal_minutes;
  day = focus_day_stamp();
  synced = day > 0 && day == g_ui.focus_stats.days[STUDY_FOCUS_DAYS - 1].day;

  lv_label_set_text_fmt(g_ui.report_today, "%lu 分钟",
                        (unsigned long)g_ui.focus_minutes);
  lv_label_set_text_fmt(g_ui.report_rounds, "%lu 轮",
                        (unsigned long)g_ui.focus_rounds);
  lv_label_set_text_fmt(g_ui.report_streak, "%u 天",
                        study_focus_stats_streak(&g_ui.focus_stats));

  total = 0;
  best = 0;
  active = 0;
  for (unsigned i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      uint32_t minutes = g_ui.focus_stats.days[i].minutes;

      total += minutes;
      if (minutes > best)
        {
          best = minutes;
        }

      if (minutes > 0)
        {
          active++;
        }
    }

  week_goal = goal * 5;
  lv_label_set_text_fmt(g_ui.report_week_label, "近 7 天累计 %llu / %lu 分钟",
                        (unsigned long long)total, (unsigned long)week_goal);
  lv_bar_set_value(g_ui.report_week_bar,
                   week_goal == 0 || total >= week_goal ? 100 :
                   (int32_t)(total * 100 / week_goal), LV_ANIM_OFF);

  /* Only touch the task file while the page is actually on screen; the other
   * lines come straight out of memory on every tick.
   */

  if (g_ui.tools_active)
    {
      int pending = 0;
      int completed = 0;

      read_task_counts(&pending, &completed);
      lv_label_set_text_fmt(g_ui.report_tasks, "学习任务 %d 项待办，已完成 %d 项",
                            pending, completed);
    }

  /* The muted line carries whichever secondary fact matters most right now: a
   * problem worth acting on, or the best day when everything is fine.
   */

  if (g_ui.focus_save_error)
    {
      lv_label_set_text(g_ui.report_note, "专注记录未保存，下次完成后重试");
    }
  else if (!synced)
    {
      lv_label_set_text(g_ui.report_note, "日期未同步，今日数据待校时");
    }
  else if (total == 0)
    {
      lv_label_set_text(g_ui.report_note, "还没有记录，先开始一轮 25 分钟");
    }
  else
    {
      lv_label_set_text_fmt(g_ui.report_note, "最佳一天 %lu 分钟，日均 %llu 分钟",
                            (unsigned long)best,
                            (unsigned long long)(active ? total / active : 0));
    }
}

static void refresh_local_tools(void)
{
  if (!g_ui.reminder_status) return;
  if (g_ui.reminder_request)
    {
      cron_reminder_create_result_t result;
      int ret = cron_get_reminder_create_result(g_ui.reminder_request, &result);
      if ((ret == 0 && result.result != -EINPROGRESS) || ret == -ESTALE)
        {
          int status = ret == 0 ? result.result : ret;
          lv_label_set_text(g_ui.reminder_status, status == 0 ? "已添加提醒" :
                            status == -ENOSPC ? "提醒已满，请先取消一些" : "添加失败，请重试");
          g_ui.reminder_request = 0;
          lv_obj_remove_state(g_ui.reminder_add, LV_STATE_DISABLED);
          for (unsigned i = 0; i < 3; i++)
            lv_obj_remove_state(g_ui.reminder_quick[i], LV_STATE_DISABLED);
          if (g_ui.notice_snooze_pending)
            {
              g_ui.notice_snooze_pending = false;
              if (status == 0)
                {
                  close_local_notice(NULL);
                }
              else if (g_ui.notice_box)
                {
                  lv_obj_remove_state(g_ui.notice_snooze_button, LV_STATE_DISABLED);
                  lv_obj_remove_state(g_ui.notice_ack_button, LV_STATE_DISABLED);
                  lv_label_set_text(lv_obj_get_child(g_ui.notice_snooze_button, 0), "添加失败，重试");
                }
            }
        }
    }
  int count = cron_list_reminders(g_ui.reminder_items, UI_REMINDER_LIMIT);
  if (count >= 0)
    {
      g_ui.reminder_count = count;
      if (g_ui.reminder_badge)
        lv_obj_set_style_text_color(g_ui.reminder_badge, color(count ? COLOR_AMBER : COLOR_WHITE), 0);
      if (count) lv_obj_add_flag(g_ui.reminder_empty, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_remove_flag(g_ui.reminder_empty, LV_OBJ_FLAG_HIDDEN);
      bool cancel_found = false;
      for (unsigned i = 0; i < UI_REMINDER_LIMIT; i++)
        {
          struct reminder_row_s *row = &g_ui.reminder_rows[i];
          if (i >= (unsigned)count)
            {
              lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
              row->id[0] = '\0';
              continue;
            }
          const cron_reminder_snapshot_t *item = &g_ui.reminder_items[i];
          lv_obj_remove_flag(row->row, LV_OBJ_FLAG_HIDDEN);
          memcpy(row->id, item->id, sizeof(row->id));
          lv_label_set_text(row->message, item->message);
          int64_t remaining = reminder_remaining(item);
          if (item->cancel_result == -EINPROGRESS)
            lv_label_set_text(row->remaining, "正在取消");
          else if (item->cancel_result < 0)
            lv_label_set_text(row->remaining, "取消失败，请重试");
          else if (remaining < 0) lv_label_set_text(row->remaining, "等待校时");
          else lv_label_set_text_fmt(row->remaining, "%02llu:%02llu 后到时",
                                    (unsigned long long)remaining / 60,
                                    (unsigned long long)remaining % 60);
          if (item->cancel_result == -EINPROGRESS) lv_obj_add_state(row->cancel, LV_STATE_DISABLED);
          else lv_obj_remove_state(row->cancel, LV_STATE_DISABLED);
          if (strcmp(item->id, g_ui.reminder_cancel_id) == 0)
            {
              cancel_found = true;
              if (item->cancel_result < 0 && item->cancel_result != -EINPROGRESS)
                {
                  lv_label_set_text(g_ui.reminder_status, "取消失败，请重试");
                  g_ui.reminder_cancel_id[0] = '\0';
                }
            }
        }
      if (g_ui.reminder_cancel_id[0] && !cancel_found)
        {
          lv_label_set_text(g_ui.reminder_status, "提醒已移除");
          g_ui.reminder_cancel_id[0] = '\0';
        }
    }
  if (!g_ui.notice_box && !g_ui.voice_active && !g_ui.ptt_request_id)
    {
      cron_reminder_event_t event;
      if (cron_poll_reminder_event(&event) == 1)
        show_local_notice("提醒到时", event.message, true);
      else if (g_ui.pending_focus_title[0])
        {
          char title[sizeof(g_ui.pending_focus_title)];
          char message[sizeof(g_ui.pending_focus_message)];
          memcpy(title, g_ui.pending_focus_title, sizeof(title));
          memcpy(message, g_ui.pending_focus_message, sizeof(message));
          g_ui.pending_focus_title[0] = '\0';
          show_local_notice(title, message, false);
        }
    }
}

/* Cancel every listed reminder.  cron_cancel_reminder() only queues the work
 * for the cron thread, so the status line reports how many were accepted
 * rather than claiming the list is already empty.
 */

static void reminder_clear_event_cb(lv_event_t *event)
{
  int queued = 0;

  (void)event;
  note_user_activity();

  if (g_ui.reminder_count <= 0)
    {
      lv_label_set_text(g_ui.reminder_status, "没有可清除的提醒");
      return;
    }

  for (int i = 0; i < g_ui.reminder_count && i < UI_REMINDER_LIMIT; i++)
    {
      if (g_ui.reminder_items[i].id[0] != '\0' &&
          g_ui.reminder_items[i].cancel_result != -EINPROGRESS &&
          cron_cancel_reminder(g_ui.reminder_items[i].id) == 0)
        {
          queued++;
        }
    }

  refresh_local_tools();

  /* After refresh_local_tools(), which may rewrite the status for a pending
   * single cancellation.
   */

  lv_label_set_text_fmt(g_ui.reminder_status, "已请求清除 %d 条提醒", queued);
}

/* Stock LVGL chrome -- rounded grey box, drop shadow, theme border -- is what
 * made this page read as a form dropped into the app rather than part of it.
 * Match the home page's flat white card instead: white fill, hairline border,
 * 6 px radius, no shadow.
 */

static lv_obj_t *tools_dropdown(lv_obj_t *parent, const char *options)
{
  lv_obj_t *dropdown = lv_dropdown_create(parent);
  lv_obj_t *list;

  lv_obj_set_size(dropdown, 1, 40);
  lv_obj_set_flex_grow(dropdown, 1);
  lv_obj_set_style_text_font(dropdown, ui_font(), 0);
  lv_obj_set_style_text_color(dropdown, color(COLOR_TEXT), 0);
  lv_obj_set_style_bg_color(dropdown, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(dropdown, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(dropdown, color(COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_border_width(dropdown, 1, 0);
  lv_obj_set_style_radius(dropdown, 6, 0);
  lv_obj_set_style_shadow_width(dropdown, 0, 0);
  lv_obj_set_style_pad_left(dropdown, 8, 0);
  lv_obj_set_style_pad_right(dropdown, 6, 0);
  lv_dropdown_set_options(dropdown, options);

  /* The popup list is a separate object and keeps the theme's own colours. */

  list = lv_dropdown_get_list(dropdown);
  if (list != NULL)
    {
      lv_obj_set_style_text_font(list, ui_font(), 0);
      lv_obj_set_style_text_color(list, color(COLOR_TEXT), 0);
      lv_obj_set_style_bg_color(list, color(COLOR_SURFACE), 0);
      lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(list, color(COLOR_SURFACE_ALT), 0);
      lv_obj_set_style_border_width(list, 1, 0);
      lv_obj_set_style_radius(list, 6, 0);
      lv_obj_set_style_shadow_width(list, 0, 0);
    }

  return dropdown;
}

/* Chrome constants for the full-screen tool pages.  These are the same literals
 * create_ui() and create_header() use for the main tabview, so the tools screen
 * stops looking like a second, plainer application.
 */

#define TOOLS_CHROME      0x0a1b2f
#define TOOLS_CHROME_OFF  0x9eb0c2
#define TOOLS_CHROME_ON   0x75a0ff
#define TOOLS_CHROME_PILL 0x17314f

/* White translucent card, built exactly like create_panel(), so a tool page
 * groups its content the way the home page does instead of stacking bare rows
 * on a flat background.
 */

static lv_obj_t *tools_card(lv_obj_t *parent)
{
  lv_obj_t *card = create_group(parent, LV_FLEX_FLOW_COLUMN);

  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, color(COLOR_SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
  lv_obj_set_style_border_color(card, color(COLOR_BORDER), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_pad_row(card, 6, 0);
  lv_obj_set_style_shadow_color(card, color(0x9fb4de), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_width(card, 8, 0);
  lv_obj_set_style_shadow_ofs_y(card, 2, 0);
  return card;
}

/* One styled bar.  The stock LVGL bar keeps a very pale track, which vanished
 * against the translucent cards and made the seven-day chart read as empty
 * columns with numbers floating above them.
 */

static lv_obj_t *tools_bar(lv_obj_t *parent, uint32_t accent, int32_t width,
                           int32_t height)
{
  lv_obj_t *bar = lv_bar_create(parent);

  lv_obj_set_size(bar, width, height);
  lv_obj_set_style_bg_color(bar, color(COLOR_SURFACE_ALT), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, color(accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
  return bar;
}

/* One figure row for the report tab: grey title on the left, coloured value
 * right-aligned.  This is create_dark_status_row() in light colours, so the
 * report reads like the home page's status rail rather than a new idiom.
 */

static lv_obj_t *tools_stat_row(lv_obj_t *parent, const char *title,
                                uint32_t accent, lv_obj_t **value_label)
{
  lv_obj_t *row = create_group(parent, LV_FLEX_FLOW_ROW);
  lv_obj_t *title_label;

  lv_obj_set_size(row, LV_PCT(100), 26);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_style_border_color(row, color(COLOR_SURFACE_ALT), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  title_label = create_label(row, title, ui_font(), COLOR_MUTED);
  lv_obj_set_flex_grow(title_label, 1);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title_label, 1);

  *value_label = create_label(row, "--", ui_font(), accent);
  lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(*value_label, g_compact_layout ? 96 : 140);
  lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_RIGHT, 0);
  return row;
}

static void create_tools_screen(void)
{
  lv_obj_t *reminders;
  lv_obj_t *history;
  lv_obj_t *report;
  lv_obj_t *pages[3];
  lv_obj_t *row;
  lv_obj_t *header;
  lv_obj_t *tabbar;
  lv_obj_t *content;

  g_ui.tools_screen = lv_obj_create(NULL);
  lv_obj_remove_flag(g_ui.tools_screen, LV_OBJ_FLAG_SCROLLABLE);

  /* Built the same way as g_ui.screen: flat base colour, then the baked
   * wallpaper on top.  initialize_wallpaper() bails out on non-compact
   * displays, so the base colour has to be the light one -- a dark literal
   * here would leave the 1280x800 check rendering a black page.
   */

  lv_obj_set_style_bg_color(g_ui.tools_screen, color(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(g_ui.tools_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(g_ui.tools_screen, &lv_font_montserrat_16, 0);
  create_wallpaper(g_ui.tools_screen);
  lv_obj_set_layout(g_ui.tools_screen, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_ui.tools_screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(g_ui.tools_screen, 0, 0);
  lv_obj_set_style_pad_row(g_ui.tools_screen, 0, 0);

  /* Dark translucent header, matching create_header().  This page used to open
   * with an opaque light bar, which is the first thing that made it read as a
   * different application.
   */

  header = create_group(g_ui.tools_screen, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(header, LV_PCT(100), g_compact_layout ? 42 : 58);
  lv_obj_set_style_pad_hor(header, g_compact_layout ? 8 : 16, 0);
  lv_obj_set_style_pad_column(header, g_compact_layout ? 8 : 12, 0);
  lv_obj_set_style_bg_color(header, color(TOOLS_CHROME), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_60, 0);
  lv_obj_set_style_border_color(header, color(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(header, LV_OPA_10, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  {
    lv_obj_t *back = tools_button(header, LV_SYMBOL_LEFT,
                                  g_compact_layout ? 32 : 40,
                                  tools_close_event_cb, NULL);

    /* Reuse the header's clock pill, so the one control on this screen that is
     * not a card still speaks the same language as the main header.
     */

    lv_obj_set_height(back, g_compact_layout ? 28 : 34);
    lv_obj_set_style_radius(back, 15, 0);
    lv_obj_set_style_bg_color(back, color(0x071a2e), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_60, 0);
    lv_obj_set_style_border_color(back, color(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(back, LV_OPA_20, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(back, 0), color(COLOR_WHITE),
                                0);
  }

  create_label(header, "学习工具", ui_font(), COLOR_WHITE);
  if (!g_compact_layout)
    {
      create_label(header, "提醒中心 / 专注记录 / 学习报告", ui_font(),
                   TOOLS_CHROME_OFF);
    }

  g_ui.tools_tabs = lv_tabview_create(g_ui.tools_screen);
  lv_obj_set_size(g_ui.tools_tabs, LV_PCT(100), 1);
  lv_obj_set_flex_grow(g_ui.tools_tabs, 1);
  lv_tabview_set_tab_bar_size(g_ui.tools_tabs, g_compact_layout ? 34 : 46);
  lv_obj_set_style_bg_opa(g_ui.tools_tabs, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.tools_tabs, 0, 0);
  lv_obj_set_style_pad_all(g_ui.tools_tabs, 0, 0);

  content = lv_tabview_get_content(g_ui.tools_tabs);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_radius(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);

  /* Same treatment as the main tab bar in create_ui(), including the
   * LV_PART_ITEMS|LV_STATE_CHECKED overrides.  Without those the stock theme
   * keeps its own light indicator, which was the single most obvious mismatch.
   */

  tabbar = lv_tabview_get_tab_bar(g_ui.tools_tabs);
  lv_obj_set_style_bg_color(tabbar, color(TOOLS_CHROME), 0);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_80, 0);
  lv_obj_set_style_text_color(tabbar, color(TOOLS_CHROME_OFF), 0);
  lv_obj_set_style_text_font(tabbar, ui_font(), 0);
  lv_obj_set_style_text_color(tabbar, color(TOOLS_CHROME_ON),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabbar, color(TOOLS_CHROME_PILL),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER,
                          LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tabbar, color(TOOLS_CHROME_ON),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tabbar, 2,
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tabbar, LV_BORDER_SIDE_TOP,
                               LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabbar, 0, 0);
  lv_obj_set_style_pad_all(tabbar, 0, 0);

  reminders = lv_tabview_add_tab(g_ui.tools_tabs, "提醒中心");
  history = lv_tabview_add_tab(g_ui.tools_tabs, "专注记录");
  report = lv_tabview_add_tab(g_ui.tools_tabs, "学习报告");
  pages[0] = reminders;
  pages[1] = history;
  pages[2] = report;
  for (unsigned i = 0; i < 3; i++)
    {
      /* White translucent card, so the content sits on the wallpaper the way
       * the home page's panels do instead of on a flat opaque sheet.
       */

      lv_obj_set_style_bg_color(pages[i], color(COLOR_SURFACE), 0);
      lv_obj_set_style_bg_opa(pages[i], LV_OPA_80, 0);
      lv_obj_set_style_border_color(pages[i], color(COLOR_BORDER), 0);
      lv_obj_set_style_border_width(pages[i], 1, 0);
      lv_obj_set_style_radius(pages[i], 8, 0);
      lv_obj_set_style_pad_all(pages[i], 8, 0);
      lv_obj_set_style_pad_row(pages[i], 6, 0);
      lv_obj_set_layout(pages[i], LV_LAYOUT_FLEX);
      lv_obj_set_flex_flow(pages[i], LV_FLEX_FLOW_COLUMN);
      lv_obj_set_scroll_dir(pages[i], LV_DIR_VER);
    }

  row = create_group(reminders, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_pad_column(row, 8, 0);
  g_ui.reminder_kind = tools_dropdown(row, "休息\n喝水\n远眺\n学习");
  g_ui.reminder_duration = tools_dropdown(row, "10 秒\n30 秒\n1 分钟\n3 分钟\n5 分钟\n10 分钟\n20 分钟\n30 分钟\n1 小时");
  lv_dropdown_set_selected(g_ui.reminder_duration, 1);
  g_ui.reminder_add = tools_button(row, LV_SYMBOL_PLUS, 44, reminder_add_event_cb, NULL);
  row = create_group(reminders, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_pad_column(row, 8, 0);
  static const char *quick_names[] = {"喝水 5 分", "远眺 20 分", "休息 1 小时"};
  for (unsigned i = 0; i < 3; i++)
    {
      g_ui.reminder_quick[i] = tools_button(row, quick_names[i], 1,
                                           reminder_quick_event_cb, (void *)(uintptr_t)i);
      lv_obj_set_flex_grow(g_ui.reminder_quick[i], 1);
      lv_obj_set_style_bg_color(g_ui.reminder_quick[i], color(COLOR_TEAL), 0);
    }
  {
    /* The status line and the bulk-clear control share one row: the button
     * belongs next to the text it answers, and keeping it out of the control
     * row above leaves the two dropdowns their full width on a 480 px panel.
     */

    lv_obj_t *status_row = create_group(reminders, LV_FLEX_FLOW_ROW);

    lv_obj_set_size(status_row, LV_PCT(100), 24);
    lv_obj_set_style_pad_column(status_row, 8, 0);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    g_ui.reminder_status = create_label(status_row, "", ui_font(), COLOR_MUTED);
    lv_obj_set_width(g_ui.reminder_status, 1);
    lv_obj_set_flex_grow(g_ui.reminder_status, 1);
    lv_label_set_long_mode(g_ui.reminder_status, LV_LABEL_LONG_DOT);
    g_ui.reminder_clear = tools_button(status_row, "清除全部", 84,
                                       reminder_clear_event_cb, NULL);
    lv_obj_set_height(g_ui.reminder_clear, 24);
    lv_obj_set_style_bg_color(g_ui.reminder_clear, color(COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(g_ui.reminder_clear, 0),
                                color(COLOR_MUTED), 0);
  }

  lv_obj_t *list = create_group(reminders, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(list, LV_PCT(100), 1);
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_style_pad_row(list, 0, 0);
  lv_obj_set_style_pad_right(list, 8, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  g_ui.reminder_empty = create_label(list, "暂无提醒，点上面的 + 添加一个",
                                     ui_font(), COLOR_MUTED);
  for (unsigned i = 0; i < UI_REMINDER_LIMIT; i++)
    {
      struct reminder_row_s *item = &g_ui.reminder_rows[i];
      item->row = create_group(list, LV_FLEX_FLOW_ROW);
      lv_obj_set_size(item->row, LV_PCT(100), 52);
      lv_obj_set_style_pad_column(item->row, 8, 0);
      lv_obj_set_style_pad_ver(item->row, 5, 0);
      lv_obj_set_style_border_color(item->row, color(COLOR_SURFACE_ALT), 0);
      lv_obj_set_style_border_width(item->row, 1, 0);
      lv_obj_set_style_border_side(item->row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_t *copy = create_group(item->row, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_size(copy, 1, LV_PCT(100));
      lv_obj_set_flex_grow(copy, 1);
      lv_obj_set_style_pad_row(copy, 2, 0);
      item->message = create_label(copy, "", ui_font(), COLOR_TEXT);
      item->remaining = create_label(copy, "", ui_font(), COLOR_MUTED);
      lv_obj_set_width(item->message, LV_PCT(100));
      lv_obj_set_width(item->remaining, LV_PCT(100));
      lv_label_set_long_mode(item->message, LV_LABEL_LONG_DOT);
      lv_label_set_long_mode(item->remaining, LV_LABEL_LONG_DOT);
      item->cancel = tools_button(item->row, LV_SYMBOL_CLOSE, 40, reminder_cancel_event_cb, item);
      lv_obj_set_style_bg_color(item->cancel, color(COLOR_CORAL), 0);
      lv_obj_add_event_cb(item->cancel, reminder_cancel_event_cb, LV_EVENT_PRESSED, item);
      lv_obj_add_flag(item->row, LV_OBJ_FLAG_HIDDEN);
    }

  g_ui.goal_summary = create_label(history, "", ui_font(), COLOR_TEXT);
  lv_obj_set_size(g_ui.goal_summary, LV_PCT(100), 20);
  lv_label_set_long_mode(g_ui.goal_summary, LV_LABEL_LONG_DOT);
  g_ui.goal_progress = tools_bar(history, COLOR_TEAL, LV_PCT(100), 8);
  row = create_group(history, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "每日目标", ui_font(), COLOR_MUTED);
  g_ui.goal_control = lv_buttonmatrix_create(row);
  static const char *goal_map[] = {"30 分", "60 分", "90 分", ""};
  lv_buttonmatrix_set_map(g_ui.goal_control, goal_map);
  lv_obj_set_size(g_ui.goal_control, 1, 40);
  lv_obj_set_flex_grow(g_ui.goal_control, 1);
  lv_obj_set_style_text_font(g_ui.goal_control, ui_font(), LV_PART_ITEMS);
  lv_obj_set_style_pad_all(g_ui.goal_control, 0, 0);
  lv_obj_set_style_pad_column(g_ui.goal_control, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_ui.goal_control, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_ui.goal_control, 0, 0);

  /* The stock button matrix paints heavy theme borders and keeps its own
   * checked colour; restate both so the picker matches the cards around it.
   */

  lv_obj_set_style_bg_color(g_ui.goal_control, color(COLOR_SURFACE_ALT), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(g_ui.goal_control, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(g_ui.goal_control, color(COLOR_MUTED), LV_PART_ITEMS);
  lv_obj_set_style_border_width(g_ui.goal_control, 0, LV_PART_ITEMS);
  lv_obj_set_style_radius(g_ui.goal_control, 6, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(g_ui.goal_control, color(COLOR_BLUE),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(g_ui.goal_control, color(COLOR_WHITE),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_buttonmatrix_set_button_ctrl_all(g_ui.goal_control, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(g_ui.goal_control, true);
  lv_obj_add_event_cb(g_ui.goal_control, goal_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  row = create_group(history, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), 92);
  lv_obj_set_style_pad_column(row, 6, 0);
  for (unsigned i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      lv_obj_t *column = create_group(row, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_size(column, 1, LV_PCT(100));
      lv_obj_set_flex_grow(column, 1);
      lv_obj_set_style_pad_row(column, 4, 0);
      lv_obj_set_flex_align(column, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      g_ui.history_values[i] = create_label(column, "0", &lv_font_montserrat_12, COLOR_TEXT);
      lv_obj_set_width(g_ui.history_values[i], LV_PCT(100));
      lv_label_set_long_mode(g_ui.history_values[i], LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_align(g_ui.history_values[i], LV_TEXT_ALIGN_CENTER, 0);
      g_ui.history_bars[i] = tools_bar(column,
                                       i == STUDY_FOCUS_DAYS - 1 ? COLOR_BLUE : COLOR_TEAL,
                                       18, 50);
      g_ui.history_dates[i] = create_label(column, "--", &lv_font_montserrat_12, COLOR_MUTED);
    }
  g_ui.focus_history_note = create_label(history, "", ui_font(), COLOR_MUTED);
  lv_obj_set_size(g_ui.focus_history_note, LV_PCT(100), 20);
  lv_label_set_long_mode(g_ui.focus_history_note, LV_LABEL_LONG_DOT);

  /* Report tab.  Everything here is recomputed from focus_stats by
   * refresh_study_report(), so the tab stores nothing and cannot drift away
   * from the seven-day chart next door.
   */

  {
    lv_obj_t *today = tools_card(report);

    tools_stat_row(today, "今日专注", COLOR_BLUE, &g_ui.report_today);
    tools_stat_row(today, "完成轮次", COLOR_TEAL, &g_ui.report_rounds);
    tools_stat_row(today, "连续天数", COLOR_AMBER, &g_ui.report_streak);
  }

  {
    lv_obj_t *week = tools_card(report);

    g_ui.report_week_label = create_label(week, "", ui_font(), COLOR_TEXT);
    lv_obj_set_width(g_ui.report_week_label, LV_PCT(100));
    lv_label_set_long_mode(g_ui.report_week_label, LV_LABEL_LONG_DOT);
    g_ui.report_week_bar = tools_bar(week, COLOR_TEAL, LV_PCT(100), 10);
    g_ui.report_tasks = create_label(week, "", ui_font(), COLOR_TEXT);
    lv_obj_set_width(g_ui.report_tasks, LV_PCT(100));
    lv_label_set_long_mode(g_ui.report_tasks, LV_LABEL_LONG_DOT);
    g_ui.report_note = create_label(week, "", ui_font(), COLOR_MUTED);
    lv_obj_set_width(g_ui.report_note, LV_PCT(100));
    lv_label_set_long_mode(g_ui.report_note, LV_LABEL_LONG_DOT);
  }

  refresh_focus_labels();
}
#endif

static void advance_focus_timer(void)
{
  uint32_t elapsed_ms = lv_tick_elaps(g_ui.focus_tick_ms);
  g_ui.focus_tick_ms = lv_tick_get();
  if (!g_ui.focus_running) return;

  uint64_t accumulated = (uint64_t)g_ui.focus_fraction_ms + elapsed_ms;
  g_ui.focus_elapsed += (uint32_t)(accumulated / 1000);
  g_ui.focus_fraction_ms = (uint32_t)(accumulated % 1000);
  bool finished_focus = false;

  if (!g_ui.on_break && g_ui.focus_elapsed >= focus_round_seconds())
    {
      g_ui.focus_elapsed -= focus_round_seconds();
      g_ui.on_break = true;
      finished_focus = true;
#ifndef _WIN32
      study_focus_stats_record_round(&g_ui.focus_stats, focus_day_stamp(),
                                      focus_round_minutes());
#else
      g_ui.focus_rounds++;
      g_ui.focus_minutes += focus_round_minutes();
#endif
      save_focus_stats();
      refresh_focus_labels();
      lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_TEAL), LV_PART_INDICATOR);
      lv_label_set_text(g_ui.focus_status, "休息 5 分钟，起来活动一下");
    }

  if (g_ui.on_break && g_ui.focus_elapsed >= BREAK_SECONDS)
    {
      g_ui.focus_elapsed = 0;
      g_ui.focus_fraction_ms = 0;
      g_ui.on_break = false;
      g_ui.focus_running = false;
      lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE), LV_PART_INDICATOR);
      lv_label_set_text(g_ui.focus_status, "休息结束，继续下一轮");
      lv_label_set_text(g_ui.focus_button_label, LV_SYMBOL_PLAY);
      stop_focus_pulse();
#ifndef _WIN32
      show_local_notice("休息结束", "可以开始下一轮专注了", false);
#endif
    }
  else if (finished_focus)
    {
#ifndef _WIN32
      char message[128];
      snprintf(message, sizeof(message), "完成 %lu 分钟，休息一下吧",
               (unsigned long)focus_round_minutes());
      show_local_notice("专注完成", message, false);
#endif
    }

  uint32_t total = g_ui.on_break ? BREAK_SECONDS : focus_round_seconds();
  lv_arc_set_value(g_ui.focus_bar, (int32_t)(g_ui.focus_elapsed * 100 / total));
}

static void update_timer_cb(lv_timer_t *timer)
{
  static uint8_t task_update_tick;
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
  else if (!g_ui.on_standby
#ifndef _WIN32
           && !g_ui.tools_active && g_ui.notice_box == NULL
#endif
           )
    {
      g_ui.idle_seconds++;
      if (g_ui.idle_seconds >= STANDBY_IDLE_SECONDS)
        {
          show_standby_ui();
        }
    }

#ifndef _WIN32
  int today = focus_day_stamp();
  bool new_day = study_focus_stats_set_day(&g_ui.focus_stats, today);
  if (new_day)
    {
      save_focus_stats();
    }
  if (new_day || today != g_ui.focus_display_day) refresh_focus_labels();
#endif
  advance_focus_timer();

  remaining = (g_ui.on_break ? BREAK_SECONDS : focus_round_seconds()) -
              g_ui.focus_elapsed;
  lv_label_set_text_fmt(g_ui.focus_time, "%02lu:%02lu",
                        (unsigned long)(remaining / 60),
                        (unsigned long)(remaining % 60));

  task_update_tick++;
  if (task_update_tick >= 5)
    {
      task_update_tick = 0;
      update_task_widgets();
    }
#ifndef _WIN32
  refresh_reminder_ring();
  refresh_local_tools();
#endif
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

      /* Remember it for the AI page's "最近回复" line, which previously showed
       * a fixed made-up summary.
       */

      remember_recent_reply(text);

      if (g_ui.ai_recent_text != NULL)
        {
          lv_label_set_text(g_ui.ai_recent_text, g_last_reply);
        }

      /* Speak the answer. The bridge queues it on a worker thread, so this
       * returns immediately and the UI keeps redrawing.
       */

      voice_ui_bridge_speak(text);
    }

  if (got_final && g_ui.voice_status != NULL)
    {
      lv_label_set_text(g_ui.voice_status, "回答完成");
    }

  if (got_final && g_ui.ai_coach_label != NULL)
    {
      lv_label_set_text(g_ui.ai_coach_label, "再给建议");
    }

  /* A slow request is still in flight. Do not unlock recording and enqueue
   * a second question while the first response may still arrive. */
  if (!got_final && ai_chat_bridge_is_busy() &&
      ai_chat_bridge_pending_sec() >= 15 && g_ui.voice_status != NULL)
    {
      lv_label_set_text(g_ui.voice_status, "回复较慢，仍在等待");
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
  lv_obj_set_size(left, 1, LV_PCT(100));
  lv_obj_set_flex_grow(left, 1);
  lv_obj_set_style_pad_column(header, 8, 0);
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

  /* Were fixed green literals that claimed "在线" regardless of the radio.
   * update_system_widgets() drives both from read_network_address() now, so
   * the header agrees with the home page's network row.
   */

  g_ui.header_net_icon = create_label(right, LV_SYMBOL_WIFI,
                                      &lv_font_montserrat_14, COLOR_MUTED);
  g_ui.header_net_text = create_label(right, "检测中", ui_font(), COLOR_MUTED);
  g_ui.header_clock = create_label(right, "--:--", &lv_font_montserrat_12,
                                   COLOR_WHITE);
#ifndef _WIN32
  lv_obj_t *reminders = tools_button(header, LV_SYMBOL_BELL, 32,
                                     show_tools_event_cb, NULL);
  lv_obj_set_height(reminders, 32);
  lv_obj_set_ext_click_area(reminders, 4);
  g_ui.reminder_badge = lv_obj_get_child(reminders, 0);
#endif
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
  lv_obj_t *timer_controls;
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
  g_ui.home_focus_title = create_label(hero_copy, "今日专注", ui_font(), COLOR_BLUE);

  /* The subject follows the first unfinished task instead of a baked string,
   * so the hero line always names something the user actually put there.
   */

  g_ui.home_focus_subject = create_label(hero_copy, "暂无待办", ui_font(),
                                         COLOR_TEXT);
  lv_label_set_long_mode(g_ui.home_focus_subject, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_ui.home_focus_subject, LV_PCT(100));
  if (!g_compact_layout)
    {
      create_label(hero_copy, "一次只完成一件事", ui_font(), COLOR_MUTED);
    }
  g_ui.focus_status = create_label(hero_copy, "准备就绪", ui_font(),
                                   COLOR_MUTED);
  lv_label_set_long_mode(g_ui.focus_status, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_ui.focus_status, LV_PCT(100));

  timer_box = create_group(hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(timer_box, 104, LV_PCT(100));
  lv_obj_set_style_pad_row(timer_box, 4, 0);
  lv_obj_set_flex_align(timer_box, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  g_ui.focus_bar = lv_arc_create(timer_box);
  lv_obj_set_size(g_ui.focus_bar, 88, 88);
  lv_obj_remove_flag(g_ui.focus_bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_style(g_ui.focus_bar, NULL, LV_PART_KNOB);
  lv_arc_set_rotation(g_ui.focus_bar, 270);
  lv_arc_set_bg_angles(g_ui.focus_bar, 0, 360);
  lv_arc_set_range(g_ui.focus_bar, 0, 100);
  lv_arc_set_value(g_ui.focus_bar, 0);
  lv_obj_set_style_arc_width(g_ui.focus_bar, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(g_ui.focus_bar, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(g_ui.focus_bar, color(0xd3deea), LV_PART_MAIN);
  lv_obj_set_style_arc_color(g_ui.focus_bar, color(COLOR_BLUE),
                             LV_PART_INDICATOR);

  g_ui.focus_time = create_label(g_ui.focus_bar, "25:00",
                                 &lv_font_montserrat_20, COLOR_TEXT);
  lv_obj_center(g_ui.focus_time);

  timer_controls = create_group(timer_box, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(timer_controls, 104, 36);
  lv_obj_set_style_pad_column(timer_controls, 6, 0);
  lv_obj_set_flex_align(timer_controls, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  button = lv_button_create(timer_controls);
  g_ui.focus_button = button;
  lv_obj_set_size(button, 32, 32);
  lv_obj_set_ext_click_area(button, 4);
  lv_obj_set_style_radius(button, 16, 0);
  lv_obj_set_style_bg_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_color(button, color(COLOR_BLUE), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
  lv_obj_set_style_shadow_width(button, 12, 0);
  lv_obj_add_event_cb(button, focus_event_cb, LV_EVENT_CLICKED, NULL);
  g_ui.focus_button_label = create_label(button, LV_SYMBOL_PLAY,
                                         &lv_font_montserrat_14,
                                         COLOR_WHITE);
  lv_obj_center(g_ui.focus_button_label);

  /* Round length, tappable to cycle 15/25/45.  The length was previously a
   * compile-time constant with no control at all.
   */

  {
    lv_obj_t *length_chip = lv_obj_create(timer_controls);

    lv_obj_remove_flag(length_chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(length_chip, 66, 28);
    lv_obj_set_style_radius(length_chip, 6, 0);
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
  g_ui.home_health = create_label(rail_header, "检测中", ui_font(),
                                  COLOR_MUTED);
  create_dark_status_row(rail, LV_SYMBOL_SETTINGS, "CPU", "--",
                         &g_ui.home_cpu);
  create_dark_status_row(rail, LV_SYMBOL_DRIVE, "内存", "--",
                         &g_ui.home_memory);
  create_dark_status_row(rail, LV_SYMBOL_WIFI, "网络", "--",
                         &g_ui.home_network);
  create_dark_status_row(rail, LV_SYMBOL_REFRESH, "运行", "--:--",
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

  task_icon = create_icon_badge(task_strip, LV_SYMBOL_LIST, COLOR_BLUE, 26);
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
    create_label(tally, "专注记录", ui_font(), COLOR_MUTED);
#ifndef _WIN32
    lv_obj_add_flag(tally, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tally, show_tools_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)1);
#endif
    g_ui.focus_stat_value = create_label(tally, "今天还没开始", ui_font(),
                                         COLOR_TEXT);
    lv_label_set_long_mode(g_ui.focus_stat_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_ui.focus_stat_value, LV_PCT(100));
    lv_obj_set_style_text_align(g_ui.focus_stat_value, LV_TEXT_ALIGN_RIGHT, 0);
  }

  refresh_focus_labels();
  (void)task_icon;
}

static void create_status_page(lv_obj_t *page)
{
  lv_obj_t *metrics;
  lv_obj_t *panel;
  lv_obj_t *row;
  lv_obj_t *value;

  configure_page(page);
  create_page_heading(page, LV_SYMBOL_BARS, COLOR_TEAL, "系统状态",
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
  g_ui.status_agent_value = create_label(row, "检测中", ui_font(),
                                         COLOR_MUTED);
  lv_obj_set_width(g_ui.status_agent_value,
                   g_compact_layout ? 88 : 110);

  row = create_group(panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(row, LV_PCT(100), g_compact_layout ? 20 : 42);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_label(row, "演示就绪度", ui_font(), COLOR_MUTED);
  g_ui.status_readiness_value = create_label(row, "检测中", ui_font(),
                                             COLOR_MUTED);
  lv_obj_set_width(g_ui.status_readiness_value,
                   g_compact_layout ? 88 : 110);
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

  configure_page(page);
  create_page_heading(page, LV_SYMBOL_LIST, COLOR_BLUE, "学习任务",
                      "由 Study Assistant Skill 保存");

  summary = create_group(page, LV_FLEX_FLOW_ROW);
  lv_obj_set_size(summary, LV_PCT(100), g_compact_layout ? 56 : 128);
  lv_obj_set_style_pad_column(summary, g_compact_layout ? 6 : 14, 0);
  create_metric_card(summary, COLOR_AMBER, "待完成", "0",
                     "尚未勾选", &g_ui.task_pending_value);
  create_metric_card(summary, COLOR_GREEN, "已完成", "0",
                     "已经勾选", &g_ui.task_completed_value);

  /* Third card used to read "提醒 / 可用 / 主动提醒已验证" with nothing behind
   * it.  Completion rate is derived from the same counts as the other two, so
   * it stays honest for free.
   */

  create_metric_card(summary, COLOR_BLUE, "完成率", "0%",
                     "本轮进度", &g_ui.task_rate_value);

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
  lv_obj_set_width(g_ui.task_file_status, g_compact_layout ? 168 : 228);
  {
    /* Clearing finished tasks was twenty taps on the per-row delete button,
     * each one shifting the rows under the finger.
     */

    lv_obj_t *clear_label = create_action_button(button_row, COLOR_MUTED,
                                                g_compact_layout ? "清理" :
                                                "清理已完成",
                                                task_clear_done_event_cb);

    if (g_compact_layout)
      {
        lv_obj_set_height(lv_obj_get_parent(clear_label), 28);
      }
  }

  g_ui.task_plan_label = create_action_button(button_row, COLOR_TEAL,
                                              "今日计划",
                                              task_daily_plan_event_cb);
  if (g_compact_layout)
    {
      lv_obj_set_height(lv_obj_get_parent(g_ui.task_plan_label), 28);
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

  /* Was the fixed string "正在聆听", which claimed the mic was open even with
   * the agent down.  Now it names what the bridge is actually doing.
   */

  g_ui.ai_subtitle = create_label(voice_copy, "待唤醒", ui_font(), COLOR_TEXT);
  lv_label_set_long_mode(g_ui.ai_subtitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_ui.ai_subtitle, LV_PCT(100));

  if (!g_compact_layout)
    {
      create_label(voice_copy, "按住麦克风说话", ui_font(), COLOR_MUTED);
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
  g_ui.ai_check_label = create_label(button, LV_SYMBOL_AUDIO,
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
  g_ui.ai_rail_state = create_label(rail_header, "检测中", ui_font(),
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
                         &g_ui.ai_request_value);

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
  create_icon_badge(recent, LV_SYMBOL_OK, COLOR_BLUE,
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
    /* Held a made-up summary ("已生成 4 个学习重点").  It now shows the tail of
     * the last real reply, or says there has not been one yet.
     */

#ifdef _WIN32
    const char *recent_seed = "还没有对话记录";
#else
    const char *recent_seed = g_last_reply[0] != '\0' ?
                              g_last_reply : "还没有对话记录";
#endif

    g_ui.ai_recent_text = create_label(recent_copy, recent_seed,
                                       ui_font(), COLOR_TEXT);
    lv_label_set_long_mode(g_ui.ai_recent_text, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_ui.ai_recent_text, LV_PCT(100));
  }

  g_ui.ai_coach_label = create_action_button(recent, COLOR_TEAL,
                                             "智能建议",
                                             ai_coach_event_cb);
  lv_obj_set_height(lv_obj_get_parent(g_ui.ai_coach_label),
                    g_compact_layout ? 28 : 34);
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

static int focus_day_stamp(void)
{
  time_t now = time(NULL);
  struct tm parts;

  if (!clock_is_synced(now)) return 0;
  localtime_r(&now, &parts);
  return (parts.tm_year + 1900) * 10000 + (parts.tm_mon + 1) * 100 +
         parts.tm_mday;
}

static void load_focus_stats(void)
{
  int result = study_focus_stats_load(&g_ui.focus_stats, FOCUS_FILE, focus_day_stamp());
  g_ui.focus_save_error = result == -ENOENT ? 0 : result;
  g_ui.focus_preset = g_ui.focus_stats.preset;
}

static void save_focus_stats(void)
{
  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      g_ui.focus_save_error = -errno;
      return;
    }
  g_ui.focus_stats.preset = g_ui.focus_preset;
  g_ui.focus_save_error = study_focus_stats_save(&g_ui.focus_stats, FOCUS_FILE);
}

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
  /* g_ui.volume_slider rather than the event target: on RELEASED the target
   * can be a part of the slider, and lv_slider_get_value() on that does not
   * return where the user let go -- which is how the knob ended up snapping
   * back to the previous position.
   */

  lv_obj_t *slider = g_ui.volume_slider;
  int32_t value;
  FILE *file;
  char buffer[64];

  if (slider == NULL)
    {
      return;
    }

  value = lv_slider_get_value(slider);
  note_user_activity();

  if (lv_event_get_code(event) == LV_EVENT_RELEASED)
    {
      /* Sample only.  The value was already committed by the VALUE_CHANGED
       * pass, and writing it again from here is what corrupted it.
       */

      if (voice_ui_bridge_chime() < 0)
        {
          lv_label_set_text_fmt(g_ui.volume_label, "音量  %d%%  (音频忙)",
                                (int)value);
        }

      return;
    }
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

/* Current city for display: the stored value, or the service's default when
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

  options[0] = '\0';
  for (index = 0; index < CITY_CHOICE_COUNT; index++)
    {
      used += (size_t)snprintf(options + used, sizeof(options) - used,
                               index == 0 ? "%s" : "\n%s",
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
  create_page_heading(page, LV_SYMBOL_SETTINGS, COLOR_AMBER, "设置",
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
    /* RELEASED as well as VALUE_CHANGED: the first plays the sample, the
     * second keeps the label tracking the finger.
     */

    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_ui.volume_slider, volume_slider_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
  }

  /* Wake word stays OFF by default until ASR is verified on hardware. */

  g_ui.wake_switch = create_toggle_row(panel, "唤醒词 你好，openvela",
                                       "验证 ASR 后再开启", false,
                                       wake_switch_event_cb);

  /* Weather city.  Built inline rather than through create_action_row()
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
  g_ui.focus_elapsed = 0;
  g_ui.focus_tick_ms = lv_tick_get();

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
  g_ui.tabview = tabview;
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

  home = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);
  status = lv_tabview_add_tab(tabview, LV_SYMBOL_BARS);
  tasks = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST);
  ai = lv_tabview_add_tab(tabview, LV_SYMBOL_AUDIO);
  settings = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);

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
#ifndef _WIN32
  create_tools_screen();
  cron_set_ui_notifications(true);
#endif

  lv_tabview_set_active(tabview, g_initial_tab, LV_ANIM_OFF);

  /* Populate every page before the first frame is composited.  Without this
   * the UI paints placeholders for a second until update_timer_cb first runs.
   */

  update_task_widgets();
  update_system_widgets();
  update_standby_widgets();

  lv_timer_create(update_timer_cb, 1000, NULL);

#ifndef _WIN32
  /* Replies arrive asynchronously on ai_agent's dispatch thread, so poll the
   * bridge often enough that a streamed answer does not feel stalled.
   */

  lv_timer_create(voice_ptt_timer_cb, 100, NULL);
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

  /* Before the first timer tick, so the header clock and standby face never
   * render a UTC time.
   */

  apply_local_timezone();

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
  initialize_boot_logo();
  initialize_wallpaper();

  /* Boot splash, then the standby face -- same sequence the Win32 preview
   * runs.  These were fenced out with `#if 0` while the G2D interrupt-order
   * crash was being tracked down; that root cause is fixed, so the board
   * shows the full startup again.
   */

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
