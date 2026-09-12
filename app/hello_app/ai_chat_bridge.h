/****************************************************************************
 * ai_chat_bridge.h - UI <-> ai_agent chat plumbing
 *
 * The UI task and ai_agent live in the same flat address space
 * (CONFIG_BUILD_FLAT=y), so they share ai_agent's message bus directly.
 *
 * Outgoing: push an inbound message on the AGENT_CHAN_LVGL_UI channel.
 * Incoming: a message-bus tap on that same channel intercepts the reply
 *           before ai_agent's own dispatch logic sees it.
 *
 * The tap fires on ai_agent's outbound dispatch thread, so replies are
 * copied into a small mutex-protected queue and drained by the LVGL timer.
 * No LVGL call is ever made from the tap.
 ****************************************************************************/

#ifndef __AI_CHAT_BRIDGE_H
#define __AI_CHAT_BRIDGE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AI_CHAT_TEXT_MAX 512

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* Initialize the bus and install the reply tap. Idempotent. */

int ai_chat_bridge_init(void);

/* True when an "ai_agent" task is present in /proc. */

bool ai_chat_bridge_agent_running(void);

/* Hand a user utterance to the agent. Returns 0 on success. */

int ai_chat_bridge_send(const char *text);

/* Drain one queued reply. Returns false when the queue is empty.
 * *is_interim is set for ai_agent's "working" placeholder phrases,
 * which should be shown but never spoken.
 */

bool ai_chat_bridge_poll(char *buffer, size_t buffer_size, bool *is_interim);

/* True while a request is outstanding (sent, no final reply yet). */

bool ai_chat_bridge_is_busy(void);

/* Seconds since the outstanding request was sent, 0 when idle. */

int ai_chat_bridge_pending_sec(void);

/* Give up on the outstanding request. */

void ai_chat_bridge_clear_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_BRIDGE_H */
