/****************************************************************************
 * ai_chat_bridge.c - UI <-> ai_agent chat plumbing
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "ai_chat_bridge.h"
#include <voice/voice_dialogue.h>

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_CHANNEL   "lvgl_ui"
#define UI_CHAT_ID   "ui"
#define REPLY_SLOTS  4

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Mirrors agent_msg_t from ai_agent/src/core/message_bus.h. That header is
 * private to the package, so the layout is restated here rather than adding
 * an include path into ai_agent's internals.
 */

typedef struct
{
  char channel[16];
  char chat_id[64];
  char *content;
  char *image_b64;
} bridge_msg_t;

typedef void (*bridge_tap_cb_t)(const bridge_msg_t *msg, void *cookie);

/****************************************************************************
 * External Function Prototypes
 ****************************************************************************/

extern int message_bus_init(void);
extern int message_bus_push_inbound(const bridge_msg_t *msg);
extern int mbus_tap_register(const char *channel, bridge_tap_cb_t cb,
                            void *cookie);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_replies[REPLY_SLOTS][AI_CHAT_TEXT_MAX];
static bool g_reply_interim[REPLY_SLOTS];
static int g_reply_head;
static int g_reply_count;
static bool g_busy;
static time_t g_sent_at;
static bool g_ready;

/* ai_agent emits one of these before the real answer (agent_loop.c
 * send_working_status). They are progress notices, not answers: show them,
 * keep waiting, and never treat them as the final reply.
 */

static const char *const g_working_phrases[] =
{
  "稍等，处理中...",
  "让我查一下...",
  "正在分析...",
  "马上好...",
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static bool is_working_phrase(const char *text)
{
  size_t i;

  for (i = 0; i < sizeof(g_working_phrases) /
                  sizeof(g_working_phrases[0]); i++)
    {
      if (strcmp(text, g_working_phrases[i]) == 0)
        {
          return true;
        }
    }

  return false;
}

/* Runs on ai_agent's outbound dispatch thread. Copy and return fast;
 * touching LVGL here would race the UI task.
 */

static void reply_tap_cb(const bridge_msg_t *msg, void *cookie)
{
  bool interim;
  int slot;

  (void)cookie;

  if (msg == NULL || msg->content == NULL || msg->content[0] == '\0')
    {
      return;
    }

  interim = is_working_phrase(msg->content);

  pthread_mutex_lock(&g_lock);

  if (g_reply_count >= REPLY_SLOTS)
    {
      /* Drop the oldest rather than the newest: the newest is more
       * likely to be the actual answer.
       */

      g_reply_head = (g_reply_head + 1) % REPLY_SLOTS;
      g_reply_count--;
    }

  slot = (g_reply_head + g_reply_count) % REPLY_SLOTS;
  strncpy(g_replies[slot], msg->content, AI_CHAT_TEXT_MAX - 1);
  g_replies[slot][AI_CHAT_TEXT_MAX - 1] = '\0';
  g_reply_interim[slot] = interim;
  g_reply_count++;

  if (!interim && strcmp(msg->chat_id, UI_CHAT_ID) == 0)
    {
      g_busy = false;
    }

  pthread_mutex_unlock(&g_lock);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int ai_chat_bridge_init(void)
{
  if (g_ready)
    {
      return 0;
    }

  /* Safe if ai_agent already did it: message_bus_init() returns early
   * when g_initialized is set.
   */

  message_bus_init();

  if (mbus_tap_register(UI_CHANNEL, reply_tap_cb, NULL) != 0)
    {
      /* Already tapped (e.g. re-entry after a UI restart) — the existing
       * tap is ours, so carry on.
       */

      syslog(LOG_WARNING, "[ui_chat] tap already registered\n");
    }

  g_ready = true;
  return 0;
}

/* Cached liveness probe.
 *
 * scan_agent_running() below is expensive: it enumerates /proc and reads a
 * cmdline file per process.  Every caller of the public wrapper is an LVGL
 * event callback, so paying that cost inline blocks lv_timer_handler() and
 * the panel stops tracking touch until the walk finishes.
 *
 * Cache the answer for AGENT_PROBE_CACHE_SEC.  The agent's up/down state does
 * not change between two taps a few hundred milliseconds apart, so a stale
 * reading inside that window is indistinguishable from a fresh one -- while a
 * shell-side start or kill still surfaces within a few seconds.
 */

#define AGENT_PROBE_CACHE_SEC 3

static bool scan_agent_running(void);

bool ai_chat_bridge_agent_running(void)
{
  static time_t last_probe;
  static bool cached;
  static bool primed;
  time_t now = time(NULL);

  if (primed && now >= last_probe && now - last_probe < AGENT_PROBE_CACHE_SEC)
    {
      return cached;
    }

  cached = scan_agent_running();
  last_probe = now;
  primed = true;
  return cached;
}

static bool scan_agent_running(void)
{
  DIR *dir;
  struct dirent *entry;
  bool found = false;

  dir = opendir("/proc");
  if (dir == NULL)
    {
      return false;
    }

  while ((entry = readdir(dir)) != NULL)
    {
      char path[64];
      char name[64];
      FILE *fp;

      if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
        {
          continue;
        }

      snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
      fp = fopen(path, "r");
      if (fp == NULL)
        {
          continue;
        }

      if (fgets(name, sizeof(name), fp) != NULL &&
          strstr(name, "ai_agent") != NULL)
        {
          found = true;
        }

      fclose(fp);

      if (found)
        {
          break;
        }
    }

  closedir(dir);
  return found;
}

int ai_chat_bridge_send(const char *text)
{
  bridge_msg_t msg;
  voice_dialogue_status_t dialogue;
  struct timespec sent;

  if (text == NULL || text[0] == '\0')
    {
      return -EINVAL;
    }

  if (ai_chat_bridge_init() != 0)
    {
      return -EIO;
    }

  voice_wake_get_dialogue(&dialogue);
  if (dialogue.response_pending)
    {
      return -EBUSY;
    }

  memset(&msg, 0, sizeof(msg));
  strncpy(msg.channel, UI_CHANNEL, sizeof(msg.channel) - 1);
  strncpy(msg.chat_id, UI_CHAT_ID, sizeof(msg.chat_id) - 1);

  msg.content = strdup(text);
  if (msg.content == NULL)
    {
      return -ENOMEM;
    }

  /* Reserve before enqueue: local tools may answer synchronously. A slow
   * response is not cancelled just because a UI deadline has elapsed. */
  pthread_mutex_lock(&g_lock);
  if (g_busy)
    {
      pthread_mutex_unlock(&g_lock);
      free(msg.content);
      return -EBUSY;
    }
  g_busy = true;
  clock_gettime(CLOCK_MONOTONIC, &sent);
  g_sent_at = sent.tv_sec;
  pthread_mutex_unlock(&g_lock);
  voice_wake_set_frontend_busy(true);

  /* The wake thread may have submitted after our first snapshot. Holding
   * the foreground gate prevents it from submitting after this recheck. */
  voice_wake_get_dialogue(&dialogue);
  if (dialogue.response_pending)
    {
      free(msg.content);
      ai_chat_bridge_clear_busy();
      voice_wake_set_frontend_busy(false);
      return -EBUSY;
    }

  if (message_bus_push_inbound(&msg) != 0)
    {
      free(msg.content);
      ai_chat_bridge_clear_busy();
      voice_wake_set_frontend_busy(false);
      return -EIO;
    }

  return 0;
}

bool ai_chat_bridge_poll(char *buffer, size_t buffer_size, bool *is_interim)
{
  bool got = false;

  if (buffer == NULL || buffer_size == 0)
    {
      return false;
    }

  pthread_mutex_lock(&g_lock);

  if (g_reply_count > 0)
    {
      strncpy(buffer, g_replies[g_reply_head], buffer_size - 1);
      buffer[buffer_size - 1] = '\0';

      if (is_interim != NULL)
        {
          *is_interim = g_reply_interim[g_reply_head];
        }

      g_reply_head = (g_reply_head + 1) % REPLY_SLOTS;
      g_reply_count--;
      got = true;
    }

  pthread_mutex_unlock(&g_lock);
  return got;
}

bool ai_chat_bridge_is_busy(void)
{
  bool busy;

  pthread_mutex_lock(&g_lock);
  busy = g_busy;
  pthread_mutex_unlock(&g_lock);
  return busy;
}

int ai_chat_bridge_pending_sec(void)
{
  int elapsed = 0;
  struct timespec now;

  pthread_mutex_lock(&g_lock);

  if (g_busy)
    {
      clock_gettime(CLOCK_MONOTONIC, &now);
      elapsed = (int)(now.tv_sec - g_sent_at);
    }

  pthread_mutex_unlock(&g_lock);
  return elapsed;
}

void ai_chat_bridge_clear_busy(void)
{
  pthread_mutex_lock(&g_lock);
  g_busy = false;
  g_sent_at = 0;
  pthread_mutex_unlock(&g_lock);
}
