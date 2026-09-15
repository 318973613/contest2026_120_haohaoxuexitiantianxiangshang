#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ai_chat_bridge.h"
#include "voice/voice_dialogue.h"

typedef struct { char channel[16], chat_id[64]; char *content, *image_b64; } message_t;
static void (*tap)(const message_t*, void*);
static bool synchronous, queue_failure, frontend_busy, wake_pending, wake_race;
static int clock_shift;
static void deliver(const char *id, const char *text) {
    message_t message = {0}; strcpy(message.channel, "lvgl_ui");
    snprintf(message.chat_id, sizeof(message.chat_id), "%s", id);
    message.content = (char*)text; tap(&message, NULL);
}
int test_clock_gettime(clockid_t clock, struct timespec *value) {
    int ret = clock_gettime(clock, value);
    if (clock == CLOCK_MONOTONIC) value->tv_sec += clock_shift;
    return ret;
}
int message_bus_init(void) { return 0; }
int mbus_tap_register(const char *channel, void (*fn)(const message_t*, void*), void *cookie) {
    (void)cookie; assert(!strcmp(channel, "lvgl_ui")); tap = fn; return 0;
}
void voice_wake_get_dialogue(voice_dialogue_status_t *out) {
    memset(out, 0, sizeof(*out)); out->response_pending = wake_pending;
}
void voice_wake_set_frontend_busy(bool busy) {
    if (busy && wake_race) wake_pending = true;
    frontend_busy = busy;
}
int message_bus_push_inbound(const message_t *message) {
    if (queue_failure) return -1;
    if (synchronous) deliver(message->chat_id, "immediate answer");
    free(message->content); return 0;
}
int main(void) {
    char text[AI_CHAT_TEXT_MAX]; bool interim;
    assert(ai_chat_bridge_init() == 0);
    synchronous = true;
    assert(ai_chat_bridge_send("first") == 0 && !ai_chat_bridge_is_busy());
    assert(ai_chat_bridge_poll(text, sizeof(text), &interim) && !interim);
    assert(!strcmp(text, "immediate answer"));
    puts("PASS immediate answer during enqueue does not leave UI falsely busy");
    synchronous = false;
    assert(ai_chat_bridge_send("slow") == 0 && frontend_busy);
    assert(ai_chat_bridge_send("duplicate") == -EBUSY);
    deliver("reminder", "independent notification");
    assert(ai_chat_bridge_is_busy());
    assert(ai_chat_bridge_poll(text, sizeof(text), &interim));
    clock_shift = 90;
    assert(!ai_chat_bridge_poll(text, sizeof(text), &interim));
    assert(ai_chat_bridge_is_busy() && ai_chat_bridge_pending_sec() >= 90);
    deliver("ui", "late but valid answer");
    assert(!ai_chat_bridge_is_busy());
    assert(ai_chat_bridge_poll(text, sizeof(text), &interim));
    assert(!strcmp(text, "late but valid answer"));
    puts("PASS slow answer remains reserved; independent reminder does not clear it");
    queue_failure = true;
    assert(ai_chat_bridge_send("queue failure") == -EIO);
    assert(!ai_chat_bridge_is_busy() && !frontend_busy);
    queue_failure = false; wake_pending = true;
    assert(ai_chat_bridge_send("wake overlap") == -EBUSY);
    wake_pending = false; wake_race = true;
    assert(ai_chat_bridge_send("wake submitted during reservation") == -EBUSY);
    assert(!ai_chat_bridge_is_busy() && !frontend_busy);
    wake_race = false;
    wake_pending = false; synchronous = true;
    assert(ai_chat_bridge_send("retry") == 0 && !ai_chat_bridge_is_busy());
    puts("PASS queue failure and pending wake request reject safely and permit retry");
    return 0;
}
