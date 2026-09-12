/****************************************************************************
 * voice_ui_bridge.h - Bridge between voice_channel and LVGL UI
 ****************************************************************************/

#ifndef __VOICE_UI_BRIDGE_H
#define __VOICE_UI_BRIDGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct
{
  char text[1024];
  char model[64];
  time_t updated_at;
} last_reply_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int voice_ui_bridge_init(void);
bool voice_ui_bridge_read_reply(last_reply_t *reply);
int voice_ui_bridge_start_recording(void);
int voice_ui_bridge_stop_recording(char *text_out, size_t text_cap);
int voice_ui_bridge_wake_start(void);
int voice_ui_bridge_wake_stop(void);
bool voice_ui_bridge_wake_is_running(void);

#endif /* __VOICE_UI_BRIDGE_H */
