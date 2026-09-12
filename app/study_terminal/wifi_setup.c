#include <nuttx/config.h>

#include "wifi_setup.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wireless/wapi.h>

#define WIFI_CONFIG_FILE "/data/etc/wifi/wapi.conf"
#define WIFI_SCAN_FILE "/tmp/study-wifi-scan.txt"
#define WIFI_LOG_FILE "/tmp/study-wifi.log"
#define WIFI_IFNAME "wlan0"
#define WIFI_PLACEHOLDER_IP 0x0a000002
#define WIFI_MAX_NETWORKS 8
#define WIFI_SSID_MAX 32
#define WIFI_PASSWORD_MAX 63

#define WIFI_BG 0xf3f6ff
#define WIFI_SURFACE 0xffffff
#define WIFI_BORDER 0xdce5f7
#define WIFI_TEXT 0x17213d
#define WIFI_MUTED 0x71809e
#define WIFI_BLUE 0x4f7df3
#define WIFI_GREEN 0x55bd91
#define WIFI_CORAL 0xee7b9f

enum wifi_job_e
{
  WIFI_JOB_NONE,
  WIFI_JOB_RECONNECT,
  WIFI_JOB_RECOVERY,
  WIFI_JOB_SCAN,
  WIFI_JOB_CONNECT
};

enum wifi_input_stage_e
{
  WIFI_INPUT_NONE,
  WIFI_INPUT_SSID,
  WIFI_INPUT_PASSWORD
};

struct wifi_job_args_s
{
  enum wifi_job_e job;
  char ssid[WIFI_SSID_MAX + 1];
  char password[WIFI_PASSWORD_MAX + 1];
};

struct wifi_setup_s
{
  pthread_mutex_t lock;
  enum wifi_job_e active_job;
  enum wifi_job_e finished_job;
  int finished_result;
  char networks[WIFI_MAX_NETWORKS][WIFI_SSID_MAX + 1];
  int network_count;
  char selected_ssid[WIFI_SSID_MAX + 1];
  enum wifi_input_stage_e input_stage;
  lv_obj_t *overlay;
  lv_obj_t *status;
  lv_obj_t *content;
  lv_obj_t *textarea;
  lv_obj_t *keyboard;
  const lv_font_t *font;
  bool compact;
  bool visible;
  uint8_t recovery_failures;
};

static struct wifi_setup_s g_wifi;

static lv_color_t wifi_color(uint32_t value)
{
  return lv_color_hex(value);
}

static void wifi_log(const char *message)
{
  FILE *file;

  file = fopen(WIFI_LOG_FILE, "a");
  if (file == NULL)
    {
      return;
    }

  fprintf(file, "%s\n", message);
  fclose(file);
}

static bool wifi_mac_nonzero(const struct ether_addr *ap)
{
  int index;

  for (index = 0; index < 6; index++)
    {
      if (ap->ether_addr_octet[index] != 0)
        {
          return true;
        }
    }

  return false;
}

static bool wifi_is_associated(struct ether_addr *ap_out)
{
  int sock;
  struct ether_addr ap;
  bool associated = false;

  memset(&ap, 0, sizeof(ap));
  sock = wapi_make_socket();
  if (sock < 0)
    {
      return false;
    }

  if (wapi_get_ap(sock, WIFI_IFNAME, &ap) >= 0 && wifi_mac_nonzero(&ap))
    {
      associated = true;
    }

  close(sock);
  if (ap_out != NULL)
    {
      *ap_out = ap;
    }

  return associated;
}

static bool wifi_has_ipv4(uint32_t *ip_out)
{
  struct ifaddrs *interfaces;
  struct ifaddrs *item;
  bool has_ip = false;
  uint32_t ip = 0;

  if (getifaddrs(&interfaces) != 0)
    {
      return false;
    }

  for (item = interfaces; item != NULL; item = item->ifa_next)
    {
      struct sockaddr_in *address;

      if (item->ifa_addr == NULL ||
          item->ifa_addr->sa_family != AF_INET ||
          strcmp(item->ifa_name, WIFI_IFNAME) != 0 ||
          (item->ifa_flags & IFF_UP) == 0)
        {
          continue;
        }

      address = (struct sockaddr_in *)item->ifa_addr;
      if (address->sin_addr.s_addr != htonl(INADDR_ANY))
        {
          ip = ntohl(address->sin_addr.s_addr);
          has_ip = true;
          break;
        }
    }

  freeifaddrs(interfaces);
  if (ip_out != NULL)
    {
      *ip_out = ip;
    }

  return has_ip;
}

static bool wifi_is_connected(void)
{
  static bool logged_placeholder;
  static bool logged_connected;
  static uint32_t last_ip;
  struct ether_addr ap;
  uint32_t ip = 0;
  bool associated;
  bool has_ip;
  char line[96];

  associated = wifi_is_associated(&ap);
  has_ip = wifi_has_ipv4(&ip);

  /* Real connection requires association. The NETINIT placeholder IP
   * 10.0.0.2 alone must never count as connected.
   */
  if (!associated)
    {
      logged_connected = false;
      last_ip = 0;
      if (has_ip && ip == WIFI_PLACEHOLDER_IP && !logged_placeholder)
        {
          wifi_log("not connected: placeholder ip without ap");
          logged_placeholder = true;
        }

      return false;
    }

  if (!has_ip)
    {
      logged_connected = false;
      last_ip = 0;
      return false;
    }

  if (!logged_connected || last_ip != ip)
    {
      snprintf(line, sizeof(line),
               "connected ip=%u.%u.%u.%u ap=%02x:%02x:%02x:%02x:%02x:%02x",
               (unsigned int)((ip >> 24) & 0xff),
               (unsigned int)((ip >> 16) & 0xff),
               (unsigned int)((ip >> 8) & 0xff),
               (unsigned int)(ip & 0xff),
               ap.ether_addr_octet[0], ap.ether_addr_octet[1],
               ap.ether_addr_octet[2], ap.ether_addr_octet[3],
               ap.ether_addr_octet[4], ap.ether_addr_octet[5]);
      wifi_log(line);
      logged_connected = true;
      last_ip = ip;
      logged_placeholder = false;
    }

  return true;
}

static bool wifi_has_saved_config(void)
{
  FILE *file;
  char line[192];
  bool found = false;

  file = fopen(WIFI_CONFIG_FILE, "r");
  if (file == NULL)
    {
      return false;
    }

  while (fgets(line, sizeof(line), file) != NULL)
    {
      char *ssid = strstr(line, "\"ssid\"");
      char *colon;
      char *quote;

      if (ssid == NULL)
        {
          continue;
        }

      colon = strchr(ssid, ':');
      quote = colon == NULL ? NULL : strchr(colon, '"');
      if (quote != NULL && quote[1] != '\0' && quote[1] != '"')
        {
          found = true;
          break;
        }
    }

  fclose(file);
  return found;
}

static int shell_quote(const char *source, char *buffer, size_t buffer_size)
{
  size_t output = 0;
  const char *cursor;

  if (source == NULL || buffer_size < 3)
    {
      return -1;
    }

  buffer[output++] = '\'';
  for (cursor = source; *cursor != '\0'; cursor++)
    {
      if (*cursor == '\'')
        {
          if (output + 4 >= buffer_size)
            {
              return -1;
            }

          buffer[output++] = '\'';
          buffer[output++] = '\\';
          buffer[output++] = '\'';
          buffer[output++] = '\'';
        }
      else
        {
          if (output + 1 >= buffer_size)
            {
              return -1;
            }

          buffer[output++] = *cursor;
        }
    }

  if (output + 1 >= buffer_size)
    {
      return -1;
    }

  buffer[output++] = '\'';
  buffer[output] = '\0';
  return 0;
}

static int wait_for_ipv4(unsigned int seconds)
{
  /* This runs only in the WiFi worker, never an LVGL timer. A real success
   * requires both a non-zero AP BSSID and a non-placeholder IPv4 address.
   */
  while (seconds-- > 0)
    {
      if (wifi_is_connected())
        {
          return 0;
        }

      sleep(1);
    }

  wifi_log("connection verification timed out");
  return -1;
}

static int run_reconnect(void)
{
  wifi_log("reconnect begin");
  system("ifup wlan0 >>" WIFI_LOG_FILE " 2>&1");
  system("wapi mode wlan0 2 >>" WIFI_LOG_FILE " 2>&1");
  if (system("wapi reconnect wlan0 >>" WIFI_LOG_FILE " 2>&1") != 0)
    {
      return -1;
    }

  sleep(2);
  if (system("renew wlan0 >>" WIFI_LOG_FILE " 2>&1") != 0)
    {
      return -1;
    }

  return wait_for_ipv4(12);
}

static int parse_scan_results(
  char networks[WIFI_MAX_NETWORKS][WIFI_SSID_MAX + 1])
{
  FILE *file;
  char line[256];
  int count = 0;

  file = fopen(WIFI_SCAN_FILE, "r");
  if (file == NULL)
    {
      return 0;
    }

  while (count < WIFI_MAX_NETWORKS && fgets(line, sizeof(line), file) != NULL)
    {
      char field[4][40];
      char *cursor = line;
      char *ssid;
      int field_index;
      int duplicate;

      for (field_index = 0; field_index < 4; field_index++)
        {
          size_t length = 0;

          while (*cursor == ' ' || *cursor == '\t')
            {
              cursor++;
            }

          while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n' &&
                 *cursor != ' ' && *cursor != '\t')
            {
              if (length + 1 < sizeof(field[field_index]))
                {
                  field[field_index][length++] = *cursor;
                }

              cursor++;
            }

          field[field_index][length] = '\0';
        }

      if (strchr(field[0], ':') == NULL)
        {
          continue;
        }

      while (*cursor == ' ' || *cursor == '\t')
        {
          cursor++;
        }

      ssid = cursor;
      ssid[strcspn(ssid, "\r\n")] = '\0';
      if (ssid[0] == '\0')
        {
          continue;
        }

      duplicate = 0;
      for (field_index = 0; field_index < count; field_index++)
        {
          if (strncmp(networks[field_index], ssid, WIFI_SSID_MAX) == 0)
            {
              duplicate = 1;
              break;
            }
        }

      if (!duplicate)
        {
          strncpy(networks[count], ssid, WIFI_SSID_MAX);
          networks[count][WIFI_SSID_MAX] = '\0';
          count++;
        }
    }

  fclose(file);
  return count;
}

static int run_scan(void)
{
  char networks[WIFI_MAX_NETWORKS][WIFI_SSID_MAX + 1];
  int count;

  memset(networks, 0, sizeof(networks));
  wifi_log("scan begin");
  system("ifup wlan0 >>" WIFI_LOG_FILE " 2>&1");
  system("wapi mode wlan0 2 >>" WIFI_LOG_FILE " 2>&1");
  if (system("wapi scan wlan0 >" WIFI_SCAN_FILE " 2>&1") != 0)
    {
      wifi_log("scan command failed");
      return -1;
    }

  count = parse_scan_results(networks);
  pthread_mutex_lock(&g_wifi.lock);
  memcpy(g_wifi.networks, networks, sizeof(networks));
  g_wifi.network_count = count;
  pthread_mutex_unlock(&g_wifi.lock);
  {
    char line[48];
    snprintf(line, sizeof(line), "scan done count=%d", count);
    wifi_log(line);
  }
  return 0;
}

static int run_connect(const char *ssid, const char *password)
{
  char quoted_ssid[140];
  char quoted_password[280];
  char command[512];

  wifi_log("connect begin");

  if (shell_quote(ssid, quoted_ssid, sizeof(quoted_ssid)) != 0 ||
      shell_quote(password, quoted_password, sizeof(quoted_password)) != 0)
    {
      return -1;
    }

  system("ifup wlan0 >>" WIFI_LOG_FILE " 2>&1");
  system("wapi disconnect wlan0 >>" WIFI_LOG_FILE " 2>&1");
  system("wapi mode wlan0 2 >>" WIFI_LOG_FILE " 2>&1");

  snprintf(command, sizeof(command),
           "wapi psk wlan0 %s 3 2 >>" WIFI_LOG_FILE " 2>&1",
           quoted_password);
  if (system(command) != 0)
    {
      return -1;
    }

  snprintf(command, sizeof(command),
           "wapi essid wlan0 %s 1 >>" WIFI_LOG_FILE " 2>&1",
           quoted_ssid);
  if (system(command) != 0)
    {
      return -1;
    }

  sleep(2);
  if (system("renew wlan0 >>" WIFI_LOG_FILE " 2>&1") != 0 ||
      wait_for_ipv4(15) != 0)
    {
      return -1;
    }

  if (system("wapi save_config wlan0 >>" WIFI_LOG_FILE " 2>&1") != 0)
    {
      wifi_log("credential save failed");
      return -1;
    }

  sync();
  wifi_log("connect verified and saved");
  return 0;
}

static void *wifi_worker(void *argument)
{
  struct wifi_job_args_s *args = argument;
  int result = -1;

  if (args->job == WIFI_JOB_RECONNECT || args->job == WIFI_JOB_RECOVERY)
    {
      result = run_reconnect();
    }
  else if (args->job == WIFI_JOB_SCAN)
    {
      result = run_scan();
    }
  else if (args->job == WIFI_JOB_CONNECT)
    {
      result = run_connect(args->ssid, args->password);
    }

  pthread_mutex_lock(&g_wifi.lock);
  g_wifi.finished_job = args->job;
  g_wifi.finished_result = result;
  g_wifi.active_job = WIFI_JOB_NONE;
  pthread_mutex_unlock(&g_wifi.lock);

  memset(args->password, 0, sizeof(args->password));
  free(args);
  return NULL;
}

static void *wifi_health_worker(void *argument)
{
  (void)argument;

  for (;;)
    {
      int result;

      /* Network queries and recovery are deliberately kept out of the LVGL
       * timer. The UI only consumes the completion state below.
       */
      sleep(15);
      pthread_mutex_lock(&g_wifi.lock);
      if (g_wifi.active_job != WIFI_JOB_NONE)
        {
          pthread_mutex_unlock(&g_wifi.lock);
          continue;
        }

      pthread_mutex_unlock(&g_wifi.lock);
      if (wifi_is_connected())
        {
          pthread_mutex_lock(&g_wifi.lock);
          g_wifi.recovery_failures = 0;
          pthread_mutex_unlock(&g_wifi.lock);
          continue;
        }

      pthread_mutex_lock(&g_wifi.lock);
      if (g_wifi.active_job != WIFI_JOB_NONE)
        {
          pthread_mutex_unlock(&g_wifi.lock);
          continue;
        }

      g_wifi.active_job = WIFI_JOB_RECOVERY;
      g_wifi.finished_job = WIFI_JOB_NONE;
      pthread_mutex_unlock(&g_wifi.lock);

      wifi_log("connection lost; automatic recovery begin");
      result = run_reconnect();

      pthread_mutex_lock(&g_wifi.lock);
      if (result == 0)
        {
          g_wifi.recovery_failures = 0;
        }
      else if (g_wifi.recovery_failures < UINT8_MAX)
        {
          g_wifi.recovery_failures++;
        }

      g_wifi.finished_job = WIFI_JOB_RECOVERY;
      g_wifi.finished_result = result;
      g_wifi.active_job = WIFI_JOB_NONE;
      pthread_mutex_unlock(&g_wifi.lock);
    }
}

static void start_health_monitor(void)
{
  pthread_t thread;
  pthread_attr_t attributes;

  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, 24576);
  if (pthread_create(&thread, &attributes, wifi_health_worker, NULL) == 0)
    {
      pthread_detach(thread);
    }
  else
    {
      wifi_log("health monitor start failed");
    }

  pthread_attr_destroy(&attributes);
}

static bool start_job(enum wifi_job_e job, const char *ssid,
                      const char *password)
{
  struct wifi_job_args_s *args;
  pthread_t thread;
  pthread_attr_t attributes;
  int result;

  pthread_mutex_lock(&g_wifi.lock);
  if (g_wifi.active_job != WIFI_JOB_NONE)
    {
      pthread_mutex_unlock(&g_wifi.lock);
      return false;
    }

  g_wifi.active_job = job;
  g_wifi.finished_job = WIFI_JOB_NONE;
  pthread_mutex_unlock(&g_wifi.lock);

  args = calloc(1, sizeof(*args));
  if (args == NULL)
    {
      pthread_mutex_lock(&g_wifi.lock);
      g_wifi.active_job = WIFI_JOB_NONE;
      pthread_mutex_unlock(&g_wifi.lock);
      return false;
    }

  args->job = job;
  if (ssid != NULL)
    {
      strncpy(args->ssid, ssid, WIFI_SSID_MAX);
    }

  if (password != NULL)
    {
      strncpy(args->password, password, WIFI_PASSWORD_MAX);
    }

  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, 24576);
  result = pthread_create(&thread, &attributes, wifi_worker, args);
  pthread_attr_destroy(&attributes);
  if (result != 0)
    {
      memset(args->password, 0, sizeof(args->password));
      free(args);
      pthread_mutex_lock(&g_wifi.lock);
      g_wifi.active_job = WIFI_JOB_NONE;
      pthread_mutex_unlock(&g_wifi.lock);
      return false;
    }

  pthread_detach(thread);
  return true;
}

static void set_status(const char *text, uint32_t color_value)
{
  lv_label_set_text(g_wifi.status, text);
  lv_obj_set_style_text_color(g_wifi.status, wifi_color(color_value), 0);
}

static void show_overlay(void)
{
  lv_obj_remove_flag(g_wifi.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(g_wifi.overlay, -1);
  g_wifi.visible = true;
}

static void hide_overlay(void)
{
  lv_obj_add_flag(g_wifi.overlay, LV_OBJ_FLAG_HIDDEN);
  g_wifi.visible = false;
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text,
                               uint32_t background,
                               lv_event_cb_t callback)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label = lv_label_create(button);

  lv_obj_set_width(button, LV_PCT(100));
  lv_obj_set_height(button, g_wifi.compact ? 34 : 44);
  lv_obj_set_style_radius(button, 6, 0);
  lv_obj_set_style_bg_color(button, wifi_color(background), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_text_font(button, g_wifi.font, 0);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label,
                              wifi_color(background == WIFI_SURFACE ?
                                         WIFI_TEXT : 0xffffff), 0);
  lv_obj_center(label);
  if (callback != NULL)
    {
      lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    }

  return button;
}

static void begin_scan(void);

static void close_overlay_event_cb(lv_event_t *event)
{
  (void)event;
  hide_overlay();
  set_status("已关闭配网页，主界面可用", WIFI_GREEN);
  wifi_log("overlay closed by user");
}

static void show_password_page(bool failed);

static void network_event_cb(lv_event_t *event)
{
  lv_obj_t *button = lv_event_get_current_target(event);
  lv_obj_t *label = lv_obj_get_child(button, 0);

  strncpy(g_wifi.selected_ssid, lv_label_get_text(label), WIFI_SSID_MAX);
  g_wifi.selected_ssid[WIFI_SSID_MAX] = '\0';
  show_password_page(false);
}

static void refresh_event_cb(lv_event_t *event)
{
  (void)event;
  begin_scan();
}

static void reconnect_event_cb(lv_event_t *event)
{
  (void)event;
  lv_obj_clean(g_wifi.content);
  set_status("正在连接已保存的 WiFi...", WIFI_BLUE);
  create_button(g_wifi.content, "正在连接", WIFI_BLUE, NULL);
  if (!start_job(WIFI_JOB_RECONNECT, NULL, NULL))
    {
      set_status("连接任务启动失败，请重试", WIFI_CORAL);
    }
}

static void textarea_event_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  const char *text;

  if (code == LV_EVENT_CANCEL)
    {
      begin_scan();
      return;
    }

  if (code != LV_EVENT_READY)
    {
      return;
    }

  text = lv_textarea_get_text(g_wifi.textarea);
  if (g_wifi.input_stage == WIFI_INPUT_SSID)
    {
      if (text[0] == '\0')
        {
          set_status("请输入 WiFi 名称", WIFI_CORAL);
          return;
        }

      strncpy(g_wifi.selected_ssid, text, WIFI_SSID_MAX);
      g_wifi.selected_ssid[WIFI_SSID_MAX] = '\0';
      show_password_page(false);
    }
  else if (g_wifi.input_stage == WIFI_INPUT_PASSWORD)
    {
      size_t length = strlen(text);

      if (length < 8 || length > WIFI_PASSWORD_MAX)
        {
          set_status("密码需要 8 到 63 个字符", WIFI_CORAL);
          return;
        }

      if (!start_job(WIFI_JOB_CONNECT, g_wifi.selected_ssid, text))
        {
          set_status("连接任务启动失败，请重试", WIFI_CORAL);
          return;
        }

      lv_textarea_set_text(g_wifi.textarea, "");
      lv_obj_clean(g_wifi.content);
      g_wifi.textarea = NULL;
      g_wifi.keyboard = NULL;
      g_wifi.input_stage = WIFI_INPUT_NONE;
      set_status("正在连接并获取 IP...", WIFI_BLUE);
      create_button(g_wifi.content, "正在连接", WIFI_BLUE, NULL);
    }
}

static void create_input_page(const char *title, bool password_mode)
{
  lv_obj_t *label;

  lv_obj_clean(g_wifi.content);
  label = lv_label_create(g_wifi.content);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, g_wifi.font, 0);
  lv_obj_set_style_text_color(label, wifi_color(WIFI_TEXT), 0);

  g_wifi.textarea = lv_textarea_create(g_wifi.content);
  lv_obj_set_width(g_wifi.textarea, LV_PCT(100));
  lv_obj_set_height(g_wifi.textarea, 38);
  lv_textarea_set_one_line(g_wifi.textarea, true);
  lv_textarea_set_max_length(g_wifi.textarea,
                             password_mode ? WIFI_PASSWORD_MAX : WIFI_SSID_MAX);
  lv_textarea_set_password_mode(g_wifi.textarea, password_mode);
  lv_obj_set_style_text_font(g_wifi.textarea, g_wifi.font, 0);

  g_wifi.keyboard = lv_keyboard_create(g_wifi.content);
  lv_obj_set_width(g_wifi.keyboard, LV_PCT(100));
  lv_obj_set_flex_grow(g_wifi.keyboard, 1);
  lv_keyboard_set_textarea(g_wifi.keyboard, g_wifi.textarea);
  lv_obj_add_event_cb(g_wifi.textarea, textarea_event_cb, LV_EVENT_ALL, NULL);
}

static void manual_event_cb(lv_event_t *event)
{
  (void)event;
  g_wifi.input_stage = WIFI_INPUT_SSID;
  set_status("手动输入 WiFi 名称", WIFI_BLUE);
  create_input_page("WiFi 名称", false);
}

static void show_password_page(bool failed)
{
  char title[64];

  g_wifi.input_stage = WIFI_INPUT_PASSWORD;
  snprintf(title, sizeof(title), "连接：%s", g_wifi.selected_ssid);
  set_status(failed ? "连接失败，请检查密码" : "请输入 WiFi 密码",
             failed ? WIFI_CORAL : WIFI_BLUE);
  create_input_page(title, true);
}

static void show_scan_results(void)
{
  char networks[WIFI_MAX_NETWORKS][WIFI_SSID_MAX + 1];
  int count;
  int index;

  pthread_mutex_lock(&g_wifi.lock);
  count = g_wifi.network_count;
  memcpy(networks, g_wifi.networks, sizeof(networks));
  pthread_mutex_unlock(&g_wifi.lock);

  lv_obj_clean(g_wifi.content);
  g_wifi.input_stage = WIFI_INPUT_NONE;
  if (count == 0)
    {
      set_status("没有发现热点，可刷新或手动输入", WIFI_CORAL);
    }
  else
    {
      set_status("请选择 WiFi 网络", WIFI_BLUE);
      for (index = 0; index < count; index++)
        {
          create_button(g_wifi.content, networks[index], WIFI_SURFACE,
                        network_event_cb);
        }
    }

  create_button(g_wifi.content, "重新扫描", WIFI_BLUE, refresh_event_cb);
  create_button(g_wifi.content, "手动输入", WIFI_GREEN, manual_event_cb);
}

static void begin_scan(void)
{
  show_overlay();
  lv_obj_clean(g_wifi.content);
  g_wifi.input_stage = WIFI_INPUT_NONE;
  set_status("正在扫描附近 WiFi...", WIFI_BLUE);
  create_button(g_wifi.content, "扫描中", WIFI_BLUE, NULL);
  if (!start_job(WIFI_JOB_SCAN, NULL, NULL))
    {
      set_status("扫描任务启动失败", WIFI_CORAL);
    }
}

static void monitor_timer_cb(lv_timer_t *timer)
{
  enum wifi_job_e finished;
  enum wifi_job_e active;
  int result;

  (void)timer;
  pthread_mutex_lock(&g_wifi.lock);
  finished = g_wifi.finished_job;
  result = g_wifi.finished_result;
  active = g_wifi.active_job;
  g_wifi.finished_job = WIFI_JOB_NONE;
  pthread_mutex_unlock(&g_wifi.lock);

  /* Never call wapi/getifaddrs from the LVGL timer. Only consume job results. */
  if (finished == WIFI_JOB_RECONNECT)
    {
      if (result == 0)
        {
          wifi_log("job reconnect verified");
          hide_overlay();
        }
      else
        {
          wifi_log("job reconnect failed -> scan");
          begin_scan();
        }

      return;
    }

  if (finished == WIFI_JOB_RECOVERY)
    {
      if (result == 0)
        {
          wifi_log("automatic recovery verified");
        }
      else
        {
          uint8_t failures;

          pthread_mutex_lock(&g_wifi.lock);
          failures = g_wifi.recovery_failures;
          pthread_mutex_unlock(&g_wifi.lock);
          if (failures >= 2)
            {
              wifi_log("automatic recovery failed twice -> scan");
              begin_scan();
            }
        }

      return;
    }

  if (finished == WIFI_JOB_SCAN)
    {
      show_scan_results();
      return;
    }

  if (finished == WIFI_JOB_CONNECT)
    {
      if (result == 0)
        {
          wifi_log("job connect verified");
          hide_overlay();
        }
      else
        {
          wifi_log("job connect failed");
          show_password_page(true);
        }

      return;
    }

  (void)active;
}


void wifi_setup_init(lv_obj_t *parent, const lv_font_t *font, bool compact)
{
#ifdef CONFIG_IEEE80211_REALTEK_WIFI
  lv_obj_t *title;

  memset(&g_wifi, 0, sizeof(g_wifi));
  pthread_mutex_init(&g_wifi.lock, NULL);
  g_wifi.font = font;
  g_wifi.compact = compact;

  g_wifi.overlay = lv_obj_create(parent);
  lv_obj_add_flag(g_wifi.overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_remove_flag(g_wifi.overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(g_wifi.overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_align(g_wifi.overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(g_wifi.overlay, wifi_color(WIFI_BG), 0);
  lv_obj_set_style_bg_opa(g_wifi.overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_wifi.overlay, 0, 0);
  lv_obj_set_style_radius(g_wifi.overlay, 0, 0);
  lv_obj_set_style_pad_all(g_wifi.overlay, compact ? 10 : 20, 0);
  lv_obj_set_style_pad_row(g_wifi.overlay, 6, 0);
  lv_obj_set_layout(g_wifi.overlay, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_wifi.overlay, LV_FLEX_FLOW_COLUMN);

  title = lv_label_create(g_wifi.overlay);
  lv_label_set_text(title, "WiFi 配网");
  lv_obj_set_style_text_font(title, font, 0);
  lv_obj_set_style_text_color(title, wifi_color(WIFI_TEXT), 0);

  g_wifi.status = lv_label_create(g_wifi.overlay);
  lv_obj_set_width(g_wifi.status, LV_PCT(100));
  lv_label_set_long_mode(g_wifi.status, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(g_wifi.status, font, 0);

  g_wifi.content = lv_obj_create(g_wifi.overlay);
  lv_obj_set_width(g_wifi.content, LV_PCT(100));
  lv_obj_set_flex_grow(g_wifi.content, 1);
  lv_obj_set_style_bg_color(g_wifi.content, wifi_color(WIFI_SURFACE), 0);
  lv_obj_set_style_bg_opa(g_wifi.content, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(g_wifi.content, wifi_color(WIFI_BORDER), 0);
  lv_obj_set_style_border_width(g_wifi.content, 1, 0);
  lv_obj_set_style_radius(g_wifi.content, 6, 0);
  lv_obj_set_style_pad_all(g_wifi.content, 6, 0);
  lv_obj_set_style_pad_row(g_wifi.content, 5, 0);
  lv_obj_set_layout(g_wifi.content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(g_wifi.content, LV_FLEX_FLOW_COLUMN);

  show_overlay();
  wifi_log("wifi setup ready");
  if (wifi_has_saved_config())
    {
      set_status("正在自动连接已保存 WiFi...", WIFI_BLUE);
      create_button(g_wifi.content, "正在自动连接", WIFI_GREEN, NULL);
      if (!start_job(WIFI_JOB_RECONNECT, NULL, NULL))
        {
          set_status("自动连接启动失败，可手动重试", WIFI_CORAL);
          create_button(g_wifi.content, "连接已保存 WiFi", WIFI_GREEN,
                        reconnect_event_cb);
        }
    }
  else
    {
      set_status("请扫描 WiFi 后完成配网", WIFI_BLUE);
    }

  create_button(g_wifi.content, "扫描 WiFi", WIFI_BLUE, refresh_event_cb);
  create_button(g_wifi.content, "关闭配网页", WIFI_SURFACE, close_overlay_event_cb);
  lv_timer_create(monitor_timer_cb, 500, NULL);
  start_health_monitor();
#else
  (void)parent;
  (void)font;
  (void)compact;
#endif
}
