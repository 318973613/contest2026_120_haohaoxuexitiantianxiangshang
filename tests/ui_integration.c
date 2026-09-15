#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <lvgl/lvgl.h>
#include "voice_ui_bridge.h"
#include <infra/cron_snapshot.h>

typedef struct { int unused; } lv_nuttx_dsc_t;
typedef struct { lv_display_t *disp; lv_indev_t *indev; } lv_nuttx_result_t;
void lv_nuttx_dsc_init(lv_nuttx_dsc_t *dsc);
void lv_nuttx_init(lv_nuttx_dsc_t *dsc, lv_nuttx_result_t *result);
void lv_nuttx_deinit(lv_nuttx_result_t *result);

static time_t fixture_now = 1789164000;
static voice_dialogue_status_t fixture_dialogue;
static cron_reminder_snapshot_t fixture_reminder;
static cron_reminder_snapshot_t fixture_other_reminders[15];
static int fixture_other_count;
static cron_reminder_create_result_t fixture_creation;
static cron_reminder_event_t fixture_event;
static bool fixture_event_ready, fixture_notifications;
static uint32_t fixture_created_seconds;
static int fixture_create_error, fixture_chimes, fixture_tts;
static uint64_t fixture_mono_ms = 50000;
static bool fixture_cron_busy, fixture_chat_busy, fixture_speaking;
static char cancelled[9];
static const char *data_dir = "/home/openvela/openvela/vendor/allwinnertech/lichee/board/r528s3/dshanpi_nand/data/usrdata";
static FILE *fixture_fopen(const char *path, const char *mode) {
    if (mode[0] != 'r') { errno = EACCES; return NULL; }
    const char *asset = NULL;
    if (!strcmp(path, "/data/NotoSansSC-Regular.ttf")) asset = "NotoSansSC-Regular.ttf";
    if (!strcmp(path, "/data/study-terminal-wallpaper.rgb565")) asset = "study-terminal-wallpaper.rgb565";
    if (!strcmp(path, "/data/openvela-64x64.rgb565")) asset = "openvela-64x64.rgb565";
    if (asset) {
        char full[512]; snprintf(full, sizeof(full), "%s/%s", data_dir, asset);
        return fopen(full, mode);
    }
    if (!strcmp(path, "/data/ai_agent/STUDY_TASKS.md")) {
        FILE *f = tmpfile(); assert(f);
        fputs("- [ ] Review C pointers\n- [x] Read documentation\n", f); rewind(f); return f;
    }
    /* Never read the private config seed while rendering test screenshots. */
    errno = ENOENT; return NULL;
}
static time_t fixture_time(time_t *out) { if (out) *out = fixture_now; return fixture_now; }
static int fixture_clock(clockid_t clock, struct timespec *out) {
    if (clock == CLOCK_MONOTONIC) {
        out->tv_sec = fixture_mono_ms / 1000;
        out->tv_nsec = fixture_mono_ms % 1000 * 1000000;
    } else { out->tv_sec = fixture_now; out->tv_nsec = 0; }
    return 0;
}
static lv_font_t *fixture_font(const char *name, lv_freetype_font_render_mode_t mode,
    uint32_t size, lv_freetype_font_style_t style) {
    (void)name;
    char path[512]; snprintf(path, sizeof(path), "%s/NotoSansSC-Regular.ttf", data_dir);
    return lv_freetype_font_create(path, mode, size, style);
}

#define main unused_board_main
#define fopen fixture_fopen
#define time fixture_time
#define clock_gettime fixture_clock
#define lv_freetype_font_create fixture_font
#include "../app/hello_app/study_terminal_main.c"
#undef lv_freetype_font_create
#undef time
#undef clock_gettime
#undef fopen
#undef main

int ai_chat_bridge_init(void) { return 0; }
bool ai_chat_bridge_agent_running(void) { return true; }
bool ai_chat_bridge_is_busy(void) { return fixture_chat_busy; }
int ai_chat_bridge_pending_sec(void) { return fixture_chat_busy ? 90 : 0; }
void ai_chat_bridge_clear_busy(void) { fixture_chat_busy = false; }
bool ai_chat_bridge_poll(char *out, size_t cap, bool *interim) {
    (void)out; (void)cap; (void)interim; return false;
}
int ai_chat_bridge_send(const char *text) { (void)text; fixture_chat_busy = true; return 0; }
int voice_ui_bridge_init(void) { return 0; }
bool voice_ui_bridge_is_ready(void) { return true; }
bool voice_ui_bridge_is_speaking(void) { return fixture_speaking; }
int voice_ui_bridge_speak(const char *text) { assert(text); fixture_tts++; return 0; }
int voice_ui_bridge_chime(void) { fixture_chimes++; return 0; }
bool voice_ui_bridge_wake_is_running(void) { return true; }
int voice_ui_bridge_wake_start(void) { return 0; }
int voice_ui_bridge_wake_stop(void) { return 0; }
int voice_ui_bridge_ptt_start(uint32_t *id) { *id = 1; return 0; }
int voice_ui_bridge_ptt_stop(uint32_t id) { (void)id; return 0; }
void voice_ui_bridge_ptt_cancel(uint32_t id) { (void)id; }
void voice_ui_bridge_ptt_get_status(struct voice_ui_ptt_status_s *out) {
    memset(out, 0, sizeof(*out));
}
void voice_ui_bridge_get_dialogue(voice_dialogue_status_t *out) { *out = fixture_dialogue; }
void voice_ui_bridge_cancel_dialogue(void) {
    fixture_dialogue.active = false; fixture_dialogue.revision++;
}
void voice_ui_bridge_set_frontend_busy(bool busy) { (void)busy; }
int cron_get_next_reminder(cron_reminder_snapshot_t *out) {
    if (fixture_cron_busy) return -EBUSY;
    *out = fixture_reminder; return 0;
}
int cron_list_reminders(cron_reminder_snapshot_t *out, size_t capacity) {
    if (fixture_cron_busy) return -EBUSY;
    size_t count = 0;
    if (fixture_reminder.id[0] && capacity) out[count++] = fixture_reminder;
    for (int i = 0; i < fixture_other_count && count < capacity; i++)
        out[count++] = fixture_other_reminders[i];
    return (int)count;
}
int cron_create_reminder(uint32_t seconds, const char *message, uint32_t *id) {
    assert(message && seconds > 0);
    if (fixture_create_error) return fixture_create_error;
    *id = ++fixture_creation.request_id;
    fixture_creation.result = -EINPROGRESS;
    fixture_created_seconds = seconds;
    return 0;
}
int cron_get_reminder_create_result(uint32_t id, cron_reminder_create_result_t *out) {
    assert(id == fixture_creation.request_id); *out = fixture_creation; return 0;
}
void cron_set_ui_notifications(bool enabled) { fixture_notifications = enabled; }
int cron_poll_reminder_event(cron_reminder_event_t *out) {
    if (!fixture_event_ready) return 0;
    *out = fixture_event; fixture_event_ready = false; return 1;
}
int cron_cancel_reminder(const char *id) { snprintf(cancelled, sizeof(cancelled), "%s", id); return 0; }
void wifi_setup_init(lv_obj_t *parent, const lv_font_t *font, bool compact) {
    (void)parent; (void)font; (void)compact;
}

static unsigned char *frame;
static int screen_width, screen_height;
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    int width = lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(frame + (y * screen_width + area->x1) * 2,
               pixels + (y - area->y1) * width * 2, width * 2);
    }
    lv_display_flush_ready(display);
}
static void pump(void) {
    for (int i = 0; i < 25; i++) { fixture_mono_ms += 10; lv_tick_inc(10); lv_timer_handler(); }
    lv_refr_now(NULL);
}
static void screenshot(const char *name) {
    pump();
    char path[128]; snprintf(path, sizeof(path), "%s-%dx%d.ppm", name, screen_width, screen_height);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%d %d\n255\n", screen_width, screen_height);
    size_t nonzero = 0;
    for (int i = 0; i < screen_width * screen_height; i++) {
        uint16_t pixel = frame[2 * i] | (frame[2 * i + 1] << 8);
        if (pixel) nonzero++;
        unsigned char rgb[] = {(pixel >> 11) * 255 / 31,
                               ((pixel >> 5) & 63) * 255 / 63, (pixel & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, f) == 3);
    }
    fclose(f); assert(nonzero > (size_t)screen_width * screen_height / 2);
}
static void inspect_voice_geometry(void) {
    lv_obj_update_layout(g_ui.voice_screen);
    lv_obj_t *stage = lv_obj_get_parent(g_ui.voice_avatar);
    lv_area_t status, visual, chat, footer;
    lv_obj_get_coords(g_ui.voice_status, &status);
    lv_obj_get_coords(stage, &visual);
    lv_obj_get_coords(lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(g_ui.voice_reply))), &chat);
    lv_obj_get_coords(lv_obj_get_parent(g_ui.voice_ptt_button), &footer);
    printf("voice geometry status=%d:%d visual=%d:%d chat=%d:%d footer=%d:%d\n",
           status.y1, status.y2, visual.y1, visual.y2, chat.y1, chat.y2, footer.y1, footer.y2);
    assert(status.y2 < visual.y1 && visual.y2 < chat.y1);
    assert(chat.y2 < footer.y1 && footer.y2 < screen_height);
}
static void inside_screen(lv_obj_t *object) {
    lv_area_t area; lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.y1 >= 0 && area.x2 < screen_width && area.y2 < screen_height);
}
static void inspect_tools_geometry(void) {
    lv_obj_update_layout(g_ui.tools_screen);
    inside_screen(g_ui.tools_screen);
    uint32_t active = lv_tabview_get_tab_active(g_ui.tools_tabs);
    if (active == 0) {
        inside_screen(g_ui.reminder_kind); inside_screen(g_ui.reminder_duration);
        inside_screen(g_ui.reminder_add); inside_screen(g_ui.reminder_quick[2]);
        inside_screen(g_ui.reminder_status); inside_screen(g_ui.reminder_clear);
        lv_area_t kind, duration, add, quick, status, clear;
        lv_obj_get_coords(g_ui.reminder_kind, &kind);
        lv_obj_get_coords(g_ui.reminder_duration, &duration);
        lv_obj_get_coords(g_ui.reminder_add, &add);
        lv_obj_get_coords(g_ui.reminder_quick[0], &quick);
        lv_obj_get_coords(g_ui.reminder_status, &status);
        lv_obj_get_coords(g_ui.reminder_clear, &clear);
        assert(kind.x2 < duration.x1 && duration.x2 < add.x1);
        assert(kind.y2 < quick.y1 && quick.y2 < status.y1);
        assert(status.x2 < clear.x1);
        assert(lv_obj_get_height(lv_obj_get_parent(g_ui.reminder_empty)) >= 80);
    } else if (active == 1) {
        inside_screen(g_ui.goal_summary); inside_screen(g_ui.goal_control);
        inside_screen(g_ui.history_bars[0]); inside_screen(g_ui.history_dates[6]);
        inside_screen(g_ui.focus_history_note);
        lv_area_t previous, current;
        lv_obj_get_coords(g_ui.history_dates[0], &previous);
        for (unsigned i = 1; i < STUDY_FOCUS_DAYS; i++) {
            lv_obj_get_coords(g_ui.history_dates[i], &current);
            assert(previous.x2 < current.x1); previous = current;
        }
    } else {
        /* The report tab is the tallest of the three, so it is also the one
         * that would silently squash its cards if the layout overflowed.
         * Every widget is checked to be on screen, which only holds while the
         * content fits without the flex container shrinking it.
         */
        inside_screen(g_ui.report_today); inside_screen(g_ui.report_rounds);
        inside_screen(g_ui.report_streak); inside_screen(g_ui.report_week_label);
        inside_screen(g_ui.report_week_bar); inside_screen(g_ui.report_tasks);
        inside_screen(g_ui.report_note);
        lv_area_t today, rounds, streak;
        lv_obj_get_coords(g_ui.report_today, &today);
        lv_obj_get_coords(g_ui.report_rounds, &rounds);
        lv_obj_get_coords(g_ui.report_streak, &streak);
        assert(today.y2 <= rounds.y1 && rounds.y2 <= streak.y1);
    }
}
static void check_local_tools(void) {
    assert(fixture_notifications);
    show_tools_event_cb(NULL); screenshot("reminders-empty"); inspect_tools_geometry();
    assert(lv_screen_active() == g_ui.tools_screen && g_ui.tools_active);
    fixture_create_error = -ENODEV;
    lv_obj_send_event(g_ui.reminder_add, LV_EVENT_CLICKED, NULL);
    assert(!g_ui.reminder_request && !lv_obj_has_state(g_ui.reminder_add, LV_STATE_DISABLED));
    assert(strstr(lv_label_get_text(g_ui.reminder_status), "\u672a\u542f\u52a8"));
    fixture_create_error = 0;
    lv_dropdown_set_selected(g_ui.reminder_duration, 0);
    lv_obj_send_event(g_ui.reminder_add, LV_EVENT_CLICKED, NULL);
    assert(g_ui.reminder_request && fixture_created_seconds == 10);
    uint32_t request = g_ui.reminder_request;
    lv_obj_send_event(g_ui.reminder_add, LV_EVENT_CLICKED, NULL);
    assert(g_ui.reminder_request == request);
    assert(lv_obj_has_state(g_ui.reminder_add, LV_STATE_DISABLED));
    fixture_creation.result = -ENOSPC; refresh_local_tools();
    assert(!g_ui.reminder_request && !lv_obj_has_state(g_ui.reminder_add, LV_STATE_DISABLED));
    assert(strstr(lv_label_get_text(g_ui.reminder_status), "\u5df2\u6ee1"));
    lv_obj_send_event(g_ui.reminder_quick[1], LV_EVENT_CLICKED, NULL);
    assert(fixture_created_seconds == 1200);
    fixture_creation.result = 0; refresh_local_tools();
    assert(!g_ui.reminder_request);
    puts("PASS reminder controls use async creation, reject double taps and recover from missing service/full capacity");

    strcpy(fixture_reminder.id, "1234abcd");
    strcpy(fixture_reminder.message, "\u559d\u6c34\u540e\u8d77\u6765\u6d3b\u52a8\u4e00\u4e0b");
    fixture_reminder.started_at = fixture_now;
    fixture_reminder.deadline = fixture_now + 30;
    fixture_reminder.deadline_mono_ms = fixture_mono_ms + 30000;
    fixture_reminder.duration_s = 30;
    fixture_other_count = 15;
    for (int i = 0; i < fixture_other_count; i++) {
        fixture_other_reminders[i] = fixture_reminder;
        snprintf(fixture_other_reminders[i].id, 9, "%08x", i + 42);
        snprintf(fixture_other_reminders[i].message, 256, "\u5b66\u4e60\u63d0\u9192 %d", i + 2);
        fixture_other_reminders[i].deadline_mono_ms += (i + 1) * 60000;
    }
    refresh_local_tools(); refresh_reminder_ring();
    char remaining[80]; snprintf(remaining, sizeof(remaining), "%s", lv_label_get_text(g_ui.focus_time));
    fixture_now += 86400; refresh_reminder_ring();
    assert(!strcmp(remaining, lv_label_get_text(g_ui.focus_time)));
    fixture_now -= 86400 * 2; refresh_reminder_ring();
    assert(!strcmp(remaining, lv_label_get_text(g_ui.focus_time)));
    fixture_now += 86400;
    screenshot("reminders-list"); inspect_tools_geometry();
    lv_obj_t *list = lv_obj_get_parent(g_ui.reminder_rows[0].row);
    assert(lv_obj_get_scroll_bottom(list) > 0);
    char pressed[9]; strcpy(pressed, g_ui.reminder_rows[0].id);
    lv_obj_send_event(g_ui.reminder_rows[0].cancel, LV_EVENT_PRESSED, NULL);
    strcpy(fixture_reminder.id, "cafebabe"); refresh_local_tools();
    lv_obj_send_event(g_ui.reminder_rows[0].cancel, LV_EVENT_CLICKED, NULL);
    assert(!strcmp(cancelled, pressed));
    fixture_other_count = 0; memset(&fixture_reminder, 0, sizeof(fixture_reminder));
    refresh_local_tools(); refresh_reminder_ring();

    /* Bulk clear: every listed id is queued, and the status reports how many
     * were accepted instead of claiming the list is already empty.
     */
    strcpy(fixture_reminder.id, "beef0001");
    strcpy(fixture_reminder.message, "clear-all fixture");
    fixture_reminder.cancel_result = 0;
    fixture_other_count = 15;
    for (int i = 0; i < fixture_other_count; i++) {
        fixture_other_reminders[i].cancel_result = 0;
        snprintf(fixture_other_reminders[i].id, 9, "%08x", i + 42);
    }
    refresh_local_tools();
    assert(g_ui.reminder_count == 16);
    cancelled[0] = '\0';
    lv_obj_send_event(g_ui.reminder_clear, LV_EVENT_CLICKED, NULL);
    assert(strstr(lv_label_get_text(g_ui.reminder_status), "16"));
    assert(!strcmp(cancelled, fixture_other_reminders[fixture_other_count - 1].id));
    fixture_other_count = 0; memset(&fixture_reminder, 0, sizeof(fixture_reminder));
    refresh_local_tools(); refresh_reminder_ring();
    assert(g_ui.reminder_count == 0);
    lv_obj_send_event(g_ui.reminder_clear, LV_EVENT_CLICKED, NULL);
    assert(strstr(lv_label_get_text(g_ui.reminder_status), "\u6ca1\u6709"));
    puts("PASS reminder list scrolls all 16 items, keeps pressed cancellation identity and ignores NTP jumps");

    strcpy(fixture_event.id, "11111111");
    strcpy(fixture_event.message, "\u8be5\u559d\u6c34\u4e86\uff0c\u4f11\u606f\u4e00\u4e0b");
    fixture_event_ready = true;
    int chimes = fixture_chimes, tts = fixture_tts;
    refresh_local_tools(); assert(g_ui.notice_box && !fixture_event_ready);
    assert(fixture_chimes == chimes + 1 && fixture_tts == tts);
    refresh_local_tools(); assert(fixture_chimes == chimes + 1);
    screenshot("reminder-due");
    inside_screen(lv_obj_get_child(g_ui.notice_box, 0));
    snooze_notice_event_cb(NULL);
    assert(g_ui.notice_box && fixture_created_seconds == 300 && g_ui.reminder_request);
    assert(g_ui.notice_snooze_pending && lv_obj_has_state(g_ui.notice_ack_button, LV_STATE_DISABLED));
    fixture_creation.result = -EIO; refresh_local_tools();
    assert(!g_ui.reminder_request && strstr(lv_label_get_text(g_ui.reminder_status), "\u5931\u8d25"));
    assert(g_ui.notice_box && !g_ui.notice_snooze_pending);
    assert(!lv_obj_has_state(g_ui.notice_snooze_button, LV_STATE_DISABLED));
    snooze_notice_event_cb(NULL);
    fixture_creation.result = 0; refresh_local_tools();
    assert(!g_ui.notice_box && !g_ui.reminder_request);
    lv_slider_set_value(g_ui.volume_slider, 0, LV_ANIM_OFF);
    fixture_event_ready = true; refresh_local_tools();
    assert(g_ui.notice_box && fixture_chimes == chimes + 1);
    close_local_notice(NULL);
    lv_slider_set_value(g_ui.volume_slider, 70, LV_ANIM_OFF);
    puts("PASS due notifications are visible once, use local sound only, respect mute and queue five-minute snooze");

    tools_close_event_cb(NULL);
    g_ui.focus_elapsed = 0; g_ui.focus_fraction_ms = 0;
    g_ui.focus_running = true; g_ui.on_break = false;
    g_ui.focus_tick_ms = lv_tick_get();
    lv_tick_inc(7300); advance_focus_timer();
    assert(g_ui.focus_elapsed == 7 && g_ui.focus_fraction_ms == 300);
    g_ui.focus_running = false; lv_tick_inc(11000); advance_focus_timer();
    assert(g_ui.focus_elapsed == 7);
    g_ui.focus_running = true;
    g_ui.focus_elapsed = focus_round_seconds() - 1; g_ui.focus_fraction_ms = 0;
    uint32_t rounds = g_ui.focus_rounds;
    uint32_t minutes = g_ui.focus_minutes;
    lv_tick_inc(1200); advance_focus_timer();
    assert(g_ui.on_break && g_ui.focus_elapsed == 0 && g_ui.notice_box);
    assert(g_ui.focus_rounds == rounds + 1 && g_ui.focus_minutes == minutes + focus_round_minutes());
    advance_focus_timer(); assert(g_ui.focus_rounds == rounds + 1);
    screenshot("focus-complete"); close_local_notice(NULL);
    g_ui.focus_elapsed = BREAK_SECONDS - 1; g_ui.focus_fraction_ms = 0;
    lv_tick_inc(1000); advance_focus_timer();
    assert(!g_ui.on_break && !g_ui.focus_running && g_ui.focus_rounds == rounds + 1);
    close_local_notice(NULL);
    puts("PASS focus uses elapsed ticks rather than callback count, credits once and completes break without cloud TTS");

    g_ui.on_break = true; g_ui.focus_running = true;
    g_ui.focus_elapsed = BREAK_SECONDS - 1; g_ui.focus_fraction_ms = 0;
    g_ui.focus_tick_ms = lv_tick_get(); lv_tick_inc(1100);
    focus_event_cb(NULL);
    assert(!g_ui.focus_running && !g_ui.on_break && g_ui.notice_box);
    close_local_notice(NULL);
    g_ui.voice_active = true;
    show_local_notice("Focus complete", "Older focus cue", false);
    assert(!g_ui.notice_box && g_ui.pending_focus_title[0]);
    show_local_notice("Break complete", "Latest focus cue", false);
    g_ui.voice_active = false; refresh_local_tools();
    assert(g_ui.notice_box && !g_ui.pending_focus_title[0]);
    assert(!strcmp(g_ui.notice_message, "Latest focus cue"));
    close_local_notice(NULL);
    puts("PASS boundary stop clicks do not restart focus, and the latest focus notification survives voice or modal activity");

    show_tools_event_cb(NULL); lv_tabview_set_active(g_ui.tools_tabs, 1, LV_ANIM_OFF);
    lv_buttonmatrix_set_selected_button(g_ui.goal_control, 2);
    lv_obj_send_event(g_ui.goal_control, LV_EVENT_VALUE_CHANGED, NULL);
    assert(g_ui.focus_stats.goal_minutes == 90);
    fixture_now += 86400; update_timer_cb(NULL);
    assert(g_ui.focus_minutes == 0 && g_ui.focus_rounds == 0);
    assert(g_ui.focus_stats.days[STUDY_FOCUS_DAYS - 2].rounds == rounds + 1);
    time_t synced_time = fixture_now; fixture_now = 0; update_timer_cb(NULL);
    assert(strstr(lv_label_get_text(g_ui.goal_summary), "\u5f85\u6821\u65f6"));
    assert(study_focus_stats_record_round(&g_ui.focus_stats, 0, 15));
    refresh_focus_labels(); assert(g_ui.focus_minutes == 15 && g_ui.focus_stats.pending_minutes == 15);
    fixture_now = synced_time; update_timer_cb(NULL);
    assert(g_ui.focus_minutes == 15 && g_ui.focus_stats.pending_minutes == 0);
    update_timer_cb(NULL); assert(g_ui.focus_minutes == 15);
    g_ui.focus_save_error = 0; refresh_focus_labels();
    screenshot("focus-history"); inspect_tools_geometry();
    g_ui.focus_save_error = -EIO; refresh_focus_labels();
    assert(strstr(lv_label_get_text(g_ui.focus_history_note), "\u672a\u4fdd\u5b58"));
    puts("PASS daily target, seven-day history, midnight rollover, pending offline rounds and save-error state agree in LVGL");

    /* Third tab: the report must restate the same focus_stats the two tabs
     * above just exercised, including the save-error wording it inherits.
     */
    g_ui.focus_save_error = -EIO; refresh_focus_labels();
    show_tools_event_cb(NULL); lv_tabview_set_active(g_ui.tools_tabs, 2, LV_ANIM_OFF);
    refresh_focus_labels();
    assert(strstr(lv_label_get_text(g_ui.report_note), "\u672a\u4fdd\u5b58"));
    g_ui.focus_save_error = 0; refresh_focus_labels();
    assert(strstr(lv_label_get_text(g_ui.report_today), "15"));
    assert(strstr(lv_label_get_text(g_ui.report_streak), "\u5929"));
    uint64_t week_total = 0;
    for (unsigned i = 0; i < STUDY_FOCUS_DAYS; i++) week_total += g_ui.focus_stats.days[i].minutes;
    char week_text[64]; snprintf(week_text, sizeof(week_text), "%llu", (unsigned long long)week_total);
    assert(strstr(lv_label_get_text(g_ui.report_week_label), week_text));
    assert(strstr(lv_label_get_text(g_ui.report_note), "\u6700\u4f73"));
    assert(lv_bar_get_value(g_ui.report_week_bar) >= 0 && lv_bar_get_value(g_ui.report_week_bar) <= 100);
    screenshot("study-report"); inspect_tools_geometry();
    puts("PASS study report restates today, the seven-day total, the streak and the task tally from the same focus stats");
    tools_close_event_cb(NULL);
}
int main(int argc, char **argv) {
    screen_width = argc > 1 ? atoi(argv[1]) : 480;
    screen_height = argc > 2 ? atoi(argv[2]) : 320;
    setvbuf(stdout, NULL, _IOLBF, 0);
    char preview[160] = "";
    for (int i = 0; i < 40; i++) strcat(preview, "\u5b66");
    remember_recent_reply(preview);
    assert(strlen(g_last_reply) == 93 && !strncmp(g_last_reply, preview, 93));
    remember_recent_reply("Short answer");
    assert(!strcmp(g_last_reply, "Short answer"));
    memset(preview, 'A', 93); strcpy(preview + 93, "\U0001f600");
    remember_recent_reply(preview); assert(strlen(g_last_reply) == 93);
    remember_recent_reply(""); assert(!g_last_reply[0]);
    puts("PASS recent reply truncation preserves UTF-8 boundaries for Chinese and four-byte characters");
    lv_init(); lv_display_t *display = lv_display_create(screen_width, screen_height); assert(display);
    size_t bytes = screen_width * screen_height * 2;
    frame = calloc(1, bytes); void *draw = malloc(bytes); assert(frame && draw);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw, NULL, bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    configure_display_orientation(display); initialize_ui_font(); initialize_boot_logo(); initialize_wallpaper();
    assert(g_dynamic_cjk_font);
    create_ui(); show_main_ui(); screenshot("home");
    lv_area_t play, length, arc;
    lv_obj_get_coords(g_ui.focus_button, &play);
    lv_obj_get_coords(lv_obj_get_parent(g_ui.focus_length_label), &length);
    lv_obj_get_coords(g_ui.focus_bar, &arc);
    assert(play.x2 < length.x1 && arc.y2 < play.y1 && arc.y2 < length.y1);

    g_ui.focus_elapsed = 42; g_ui.focus_running = true;
    strcpy(fixture_reminder.id, "abcd1234");
    strcpy(fixture_reminder.message, "\u4f11\u606f\u4e00\u4e0b\uff0c\u8d77\u6765\u6d3b\u52a8");
    fixture_reminder.started_at = fixture_now; fixture_reminder.deadline = fixture_now + 30;
    refresh_reminder_ring(); assert(!strcmp(lv_label_get_text(g_ui.focus_time), "00:30"));
    fixture_now += 10; refresh_reminder_ring();
    assert(!strcmp(lv_label_get_text(g_ui.focus_time), "00:20") && lv_arc_get_value(g_ui.focus_bar) == 33);
    screenshot("countdown");
    fixture_cron_busy = true; fixture_now++; refresh_reminder_ring();
    assert(!strcmp(lv_label_get_text(g_ui.focus_time), "00:19")); fixture_cron_busy = false;
    focus_event_cb(NULL); assert(!strcmp(cancelled, fixture_reminder.id));
    assert(lv_obj_has_state(g_ui.focus_button, LV_STATE_DISABLED));
    fixture_reminder.cancel_result = -EIO; refresh_reminder_ring();
    assert(!lv_obj_has_state(g_ui.focus_button, LV_STATE_DISABLED));
    memset(&fixture_reminder, 0, sizeof(fixture_reminder)); refresh_reminder_ring();
    assert(!g_ui.reminder.id[0] && g_ui.focus_running && g_ui.focus_elapsed >= 42);
    g_ui.focus_running = false;
    puts("PASS real LVGL ring follows scheduler deadline, caches busy snapshots and restores focus after cancellation");
    check_local_tools();

    fixture_dialogue.session = 1; fixture_dialogue.revision++;
    fixture_dialogue.active = true; fixture_dialogue.phase = VOICE_DIALOGUE_LISTENING;
    strcpy(fixture_dialogue.reply, "\u6211\u5728\uff0c\u8bf7\u8bf4\u3002");
    poll_wake_dialogue(); pump();
    assert(g_ui.voice_active && lv_screen_active() == g_ui.voice_screen);
    assert(lv_obj_has_state(g_ui.voice_ptt_button, LV_STATE_DISABLED));
    screenshot("wake-listening"); inspect_voice_geometry();
    fixture_dialogue.phase = VOICE_DIALOGUE_WAITING; fixture_dialogue.response_pending = true;
    fixture_dialogue.revision++; strcpy(fixture_dialogue.heard, "\u4ecb\u7ecd\u4e00\u4e0b MiMo");
    poll_wake_dialogue(); assert(!strcmp(lv_label_get_text(g_ui.voice_heard), fixture_dialogue.heard));
    fixture_dialogue.phase = VOICE_DIALOGUE_SLOW; fixture_dialogue.revision++;
    poll_wake_dialogue(); screenshot("wake-waiting");
    assert(!strcmp(lv_label_get_text(g_ui.voice_status), "\u56de\u590d\u8f83\u6162\uff0c\u4ecd\u5728\u7b49\u5f85"));
    fixture_dialogue.phase = VOICE_DIALOGUE_SPEAKING; fixture_dialogue.revision++;
    strcpy(fixture_dialogue.reply, "MiMo \u53ef\u4ee5\u56de\u7b54\u95ee\u9898\uff0c\u4e5f\u80fd\u5e2e\u4f60\u8bbe\u7f6e\u5b66\u4e60\u63d0\u9192\u3002");
    poll_wake_dialogue(); screenshot("wake-reply");
    assert(!strcmp(lv_label_get_text(g_ui.voice_reply), fixture_dialogue.reply));
    fixture_dialogue.reply[0] = '\0';
    const char *sentence = "\u957f\u56de\u590d\u4fdd\u7559\u5728\u5bf9\u8bdd\u533a\u57df\uff0c\u4e0d\u906e\u6321\u5e95\u90e8\u6309\u94ae\u3002";
    for (int i = 0; i < 16; i++) {
        assert(strlen(fixture_dialogue.reply) + strlen(sentence) < sizeof(fixture_dialogue.reply));
        strcat(fixture_dialogue.reply, sentence);
    }
    fixture_dialogue.revision++; poll_wake_dialogue(); pump();
    assert(strlen(g_last_reply) == 93);
    lv_obj_t *chat = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(g_ui.voice_reply)));
    assert(lv_obj_has_flag(chat, LV_OBJ_FLAG_SCROLLABLE));
    assert(lv_obj_get_scroll_bottom(chat) > 0);
    lv_obj_scroll_to_y(chat, lv_obj_get_scroll_bottom(chat), LV_ANIM_OFF);
    screenshot("wake-long-reply"); inspect_voice_geometry();
    fixture_dialogue.phase = VOICE_DIALOGUE_FINISHED; fixture_dialogue.active = false;
    fixture_dialogue.response_pending = false; fixture_dialogue.revision++;
    poll_wake_dialogue(); assert(!lv_obj_has_state(g_ui.voice_ptt_button, LV_STATE_DISABLED));
    hide_voice_ui(); pump(); show_voice_ui();
    lv_label_set_text(g_ui.voice_reply, "Manual answer"); fixture_dialogue.revision++;
    poll_wake_dialogue(); assert(!strcmp(lv_label_get_text(g_ui.voice_reply), "Manual answer"));
    puts("PASS real LVGL wake auto-opens once, shows transcript/reply/state and does not replay stale text into manual UI");
    fixture_event_ready = true;
    int prior_chimes = fixture_chimes;
    refresh_local_tools(); assert(fixture_event_ready && !g_ui.notice_box);
    assert(!strcmp(lv_label_get_text(g_ui.voice_reply), "Manual answer") && fixture_chimes == prior_chimes);
    hide_voice_ui(); refresh_local_tools();
    assert(!fixture_event_ready && g_ui.notice_box && fixture_chimes == prior_chimes + 1);
    close_local_notice(NULL);
    puts("PASS reminders wait through an active voice screen without touching its request or reply");
    lv_deinit(); free(draw); free(frame); free(g_boot_logo_data); free(g_wallpaper_data);
    return 0;
}
