/****************************************************************************
 * ui_status_bridge.h - Bridge between ai_agent heartbeat and LVGL UI
 ****************************************************************************/

#ifndef __UI_STATUS_BRIDGE_H
#define __UI_STATUS_BRIDGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  int version;
  float cpu_percent;
  int mem_used_mb;
  int mem_total_mb;
  char network_ip[64];
  int uptime_sec;
} ui_status_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int ui_status_bridge_init(void);
bool ui_status_read(ui_status_t *status);

#endif /* __UI_STATUS_BRIDGE_H */
