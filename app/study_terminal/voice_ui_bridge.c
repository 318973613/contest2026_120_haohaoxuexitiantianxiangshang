/****************************************************************************
 * voice_ui_bridge.c - Bridge between voice_channel and LVGL UI
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "voice_ui_bridge.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef CONFIG_SYSTEM_CJSON
#  include <cjson/cJSON.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LAST_REPLY_JSON "/data/ai_agent/LAST_REPLY.json"

/****************************************************************************
 * External Function Prototypes
 ****************************************************************************/

/* From ai_agent voice_channel.c */
extern int voice_channel_start(void);
extern int voice_channel_stop_with_text(char *text_out, size_t text_cap);

/* From ai_agent voice_wake.c */
extern int voice_wake_start(void);
extern int voice_wake_stop(void);
extern bool voice_wake_is_running(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int voice_ui_bridge_init(void)
{
  return 0;
}

bool voice_ui_bridge_read_reply(last_reply_t *reply)
{
#ifdef CONFIG_SYSTEM_CJSON
  FILE *fp;
  char buffer[2048];
  cJSON *root;
  cJSON *item;
  size_t nread;
  bool success = false;

  if (reply == NULL)
    {
      return false;
    }

  memset(reply, 0, sizeof(last_reply_t));

  fp = fopen(LAST_REPLY_JSON, "r");
  if (fp == NULL)
    {
      return false;
    }

  nread = fread(buffer, 1, sizeof(buffer) - 1, fp);
  fclose(fp);

  if (nread == 0)
    {
      return false;
    }

  buffer[nread] = '\0';

  root = cJSON_Parse(buffer);
  if (root == NULL)
    {
      return false;
    }

  item = cJSON_GetObjectItem(root, "text");
  if (cJSON_IsString(item) && item->valuestring != NULL)
    {
      snprintf(reply->text, sizeof(reply->text), "%s", item->valuestring);
    }

  item = cJSON_GetObjectItem(root, "model");
  if (cJSON_IsString(item) && item->valuestring != NULL)
    {
      snprintf(reply->model, sizeof(reply->model), "%s", item->valuestring);
    }

  item = cJSON_GetObjectItem(root, "updated_at");
  if (cJSON_IsNumber(item))
    {
      reply->updated_at = (time_t)item->valuedouble;
    }

  success = true;
  cJSON_Delete(root);
  return success;
#else
  return false;
#endif
}

int voice_ui_bridge_start_recording(void)
{
  return voice_channel_start();
}

int voice_ui_bridge_stop_recording(char *text_out, size_t text_cap)
{
  return voice_channel_stop_with_text(text_out, text_cap);
}

int voice_ui_bridge_wake_start(void)
{
  return voice_wake_start();
}

int voice_ui_bridge_wake_stop(void)
{
  return voice_wake_stop();
}

bool voice_ui_bridge_wake_is_running(void)
{
  return voice_wake_is_running();
}
