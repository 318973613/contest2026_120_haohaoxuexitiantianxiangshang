/****************************************************************************
 * ui_status_bridge.h - System status source for the study terminal UI
 ****************************************************************************/

#ifndef __UI_STATUS_BRIDGE_H
#define __UI_STATUS_BRIDGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  int version;
  char cpu_text[48];        /* e.g. "armv7-a / 1 核" */
  int cpu_count;
  float mem_used_percent;
  int mem_used_mb;
  int mem_free_mb;
  int mem_total_mb;
  char network_ip[64];
  bool network_online;
  int uptime_sec;
} ui_status_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int ui_status_bridge_init(void);
bool ui_status_read(ui_status_t *status);
bool ui_status_read_network(char *buffer, size_t buffer_size);

#endif /* __UI_STATUS_BRIDGE_H */
