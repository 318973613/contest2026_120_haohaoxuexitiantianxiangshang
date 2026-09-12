/****************************************************************************
 * ui_status_bridge.c - Bridge between ai_agent heartbeat and LVGL UI
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "ui_status_bridge.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef CONFIG_SYSTEM_CJSON
#  include <cjson/cJSON.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_STATUS_JSON "/data/ai_agent/UI_STATUS.json"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int ui_status_bridge_init(void)
{
  return 0;
}

bool ui_status_read(ui_status_t *status)
{
#ifdef CONFIG_SYSTEM_CJSON
  FILE *fp;
  char buffer[512];
  cJSON *root;
  cJSON *item;
  size_t nread;
  bool success = false;

  if (status == NULL)
    {
      return false;
    }

  /* Initialize with default values */
  memset(status, 0, sizeof(ui_status_t));
  status->version = 1;
  strcpy(status->network_ip, "---");

  fp = fopen(UI_STATUS_JSON, "r");
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

  /* Parse JSON fields */
  item = cJSON_GetObjectItem(root, "version");
  if (cJSON_IsNumber(item))
    {
      status->version = item->valueint;
    }

  item = cJSON_GetObjectItem(root, "cpu_percent");
  if (cJSON_IsNumber(item))
    {
      status->cpu_percent = (float)item->valuedouble;
    }

  item = cJSON_GetObjectItem(root, "mem_used_mb");
  if (cJSON_IsNumber(item))
    {
      status->mem_used_mb = item->valueint;
    }

  item = cJSON_GetObjectItem(root, "mem_total_mb");
  if (cJSON_IsNumber(item))
    {
      status->mem_total_mb = item->valueint;
    }

  item = cJSON_GetObjectItem(root, "network_ip");
  if (cJSON_IsString(item) && item->valuestring != NULL)
    {
      snprintf(status->network_ip, sizeof(status->network_ip), "%s",
               item->valuestring);
    }

  item = cJSON_GetObjectItem(root, "uptime_sec");
  if (cJSON_IsNumber(item))
    {
      status->uptime_sec = item->valueint;
    }

  success = true;
  cJSON_Delete(root);
  return success;
#else
  /* cJSON not available */
  return false;
#endif
}
