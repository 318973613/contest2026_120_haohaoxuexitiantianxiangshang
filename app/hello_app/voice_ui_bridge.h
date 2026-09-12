/****************************************************************************
 * voice_ui_bridge.h - Bridge between ai_agent's voice channel and the UI
 *
 * Recorder and TTS requests run on workers. The UI polls recorder state
 * from its own timer; workers never call LVGL.
 *
 * Agent replies do NOT come through here — see ai_chat_bridge.h.
 ****************************************************************************/

#ifndef __VOICE_UI_BRIDGE_H
#define __VOICE_UI_BRIDGE_H

#include <voice/voice_dialogue.h>

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

#define VOICE_UI_PTT_TEXT_MAX 256

enum voice_ui_ptt_state_e
{
  VOICE_UI_PTT_IDLE = 0,
  VOICE_UI_PTT_STARTING,
  VOICE_UI_PTT_RECORDING,
  VOICE_UI_PTT_RECOGNIZING,
  VOICE_UI_PTT_CANCELLING,
  VOICE_UI_PTT_DONE,
  VOICE_UI_PTT_START_FAILED,
  VOICE_UI_PTT_ASR_FAILED
};

struct voice_ui_ptt_status_s
{
  uint32_t request_id;
  enum voice_ui_ptt_state_e state;
  int result;
  char text[VOICE_UI_PTT_TEXT_MAX];
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* Register the MiMo ASR/TTS backends. Safe to call more than once. */

int voice_ui_bridge_init(void);

/* Nonblocking readiness snapshot; false while initialization is in progress. */

bool voice_ui_bridge_is_ready(void);

/* Nonblocking push-to-talk. start() assigns a nonzero request ID. A release
 * during STARTING is remembered and stops the recorder as soon as it opens.
 * New starts return -EBUSY until the previous backend operation has ended.
 */

int voice_ui_bridge_ptt_start(uint32_t *request_id);
int voice_ui_bridge_ptt_stop(uint32_t request_id);

/* Invalidate this request and close the recorder on its worker. An in-flight
 * ASR call is allowed to finish; its result is discarded. CANCELLING remains
 * busy until that call returns. Stale IDs cannot cancel a newer recording.
 */

void voice_ui_bridge_ptt_cancel(uint32_t request_id);

/* Copy a snapshot under a short lock. DONE/FAILED stays available until the
 * next start or cancel. Only act on a result whose ID the caller still owns.
 */

void voice_ui_bridge_ptt_get_status(struct voice_ui_ptt_status_s *status);

/* Legacy blocking API, serialized through the same worker. Do not call
 * these from LVGL events or timers; use the request/snapshot API above.
 */

int voice_ui_bridge_start_recording(void);
int voice_ui_bridge_stop_recording(char *text_out, size_t text_cap);

/* Queue text for TTS playback. Returns as soon as the request is queued;
 * synthesis runs on a worker thread so the UI never blocks.
 */

int voice_ui_bridge_speak(const char *text);

/* Queue a short local reminder cue without network/TTS work. Returns 0 when
 * accepted, -EBUSY while a UI recording is active or the queue is full, or
 * another negative errno on submission failure. Later playback failures are
 * logged once; the UI must retain its visual notification regardless.
 */

int voice_ui_bridge_chime(void);

/* True while a queued or in-flight TTS/local playback request exists. */

bool voice_ui_bridge_is_speaking(void);

/* Cloud-ASR wake phrase: "你好，openvela" (contest rule).  The neutral legacy
 * aliases 小维同学 / 小维 / 学习助手 remain accepted, and matching folds ASCII
 * case plus separators, so "Hello，OpenVela" wakes the device too. */

int voice_ui_bridge_wake_start(void);
int voice_ui_bridge_wake_stop(void);
bool voice_ui_bridge_wake_is_running(void);
void voice_ui_bridge_get_dialogue(voice_dialogue_status_t *out);
void voice_ui_bridge_cancel_dialogue(void);
void voice_ui_bridge_set_frontend_busy(bool busy);

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_UI_BRIDGE_H */
