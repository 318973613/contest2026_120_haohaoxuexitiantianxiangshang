#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "agent_config.h"
#include "infra/cron_service.h"
#include "infra/cron_snapshot.h"
#include "core/message_bus.h"
#include "tools/tool_cron.h"
#include "cJSON.h"

static atomic_int tasks, deliveries, tool_entered, tool_gate, fail_save;
static atomic_int save_gate, save_entered;
static atomic_llong wall_offset_s;
static char delivered[256];

time_t test_time(time_t *out) {
    time_t value = time(NULL) + (time_t)atomic_load(&wall_offset_s);
    if (out) *out = value;
    return value;
}

static void wait_for(atomic_int *value, int wanted) {
    for (int i = 0; i < 6000 && atomic_load(value) != wanted; i++) usleep(1000);
    assert(atomic_load(value) == wanted);
}
static double milliseconds(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
FILE *test_fopen(const char *path, const char *mode) {
    if (mode[0] == 'w') {
        if (atomic_load(&save_gate)) {
            atomic_store(&save_entered, 1);
            while (atomic_load(&save_gate)) usleep(1000);
        }
        if (atomic_exchange(&fail_save, 0)) {
            errno = EACCES; return NULL;
        }
    }
    return fopen(path, mode);
}
struct launch { void *(*fn)(void*); void *arg; };
static void *worker(void *p) {
    struct launch call = *(struct launch *)p; free(p);
    call.fn(call.arg); atomic_fetch_sub(&tasks, 1); return NULL;
}
int agent_task_create(void *(*fn)(void*), const char *name, int stack, void *arg, int prio) {
    (void)name; (void)stack; (void)prio;
    struct launch *call = malloc(sizeof(*call)); assert(call);
    *call = (struct launch){fn, arg}; atomic_fetch_add(&tasks, 1);
    pthread_t tid; assert(pthread_create(&tid, NULL, worker, call) == 0);
    pthread_detach(tid); return 0;
}
int message_bus_push_outbound(const agent_msg_t *msg) {
    snprintf(delivered, sizeof(delivered), "%s", msg->content);
    free(msg->content); atomic_fetch_add(&deliveries, 1); return 0;
}
int tool_registry_execute(const char *name, const char *args, char *output, size_t cap) {
    (void)args; assert(strcmp(name, "slow_fixture") == 0);
    atomic_store(&tool_entered, 1);
    while (atomic_load(&tool_gate)) usleep(1000);
    snprintf(output, cap, "OK"); return 0;
}
static cron_job_t add_after(unsigned seconds, const char *message) {
    char input[512], output[512];
    snprintf(input, sizeof(input), "{\"name\":\"timer\",\"schedule_type\":\"after\","
             "\"delay_s\":%u,\"message\":\"%s\",\"channel\":\"voice\",\"chat_id\":\"reminder\"}",
             seconds, message);
    assert(tool_cron_add_execute(input, output, sizeof(output)) == 0);
    cron_job_t jobs[AGENT_CRON_MAX_JOBS];
    int count = cron_list_jobs(jobs, AGENT_CRON_MAX_JOBS); assert(count > 0);
    return jobs[count - 1];
}
static void clear_jobs(void) {
    cron_job_t jobs[AGENT_CRON_MAX_JOBS];
    int count = cron_list_jobs(jobs, AGENT_CRON_MAX_JOBS);
    for (int i = 0; i < count; i++) assert(cron_remove_job(jobs[i].id) == 0);
}

static cron_reminder_create_result_t wait_create(uint32_t request_id) {
    cron_reminder_create_result_t result = {0};
    for (int i = 0; i < 6000; i++) {
        int ret = cron_get_reminder_create_result(request_id, &result);
        assert(ret == 0 || ret == -EBUSY);
        if (ret == 0 && result.result != -EINPROGRESS) {
            assert(result.request_id == request_id);
            return result;
        }
        usleep(1000);
    }
    fprintf(stderr, "create request %u remained pending\n", request_id);
    assert(false);
    return result;
}

static cron_reminder_create_result_t create_local(unsigned delay, const char *message) {
    uint32_t request_id = 0;
    double start = milliseconds();
    assert(cron_create_reminder(delay, message, &request_id) == 0);
    assert(milliseconds() - start < 50 && request_id != 0);
    cron_reminder_create_result_t result = wait_create(request_id);
    assert(result.result == 0 && strlen(result.id) == 8);
    return result;
}

static int wait_reminders(int wanted, cron_reminder_snapshot_t *items) {
    int count = -EBUSY;
    for (int i = 0; i < 6000; i++) {
        count = cron_list_reminders(items, AGENT_CRON_MAX_JOBS);
        assert(count >= 0 || count == -EBUSY);
        if (count == wanted) return count;
        usleep(1000);
    }
    fprintf(stderr, "reminder count %d, expected %d\n", count, wanted);
    assert(false);
    return count;
}

static cron_reminder_event_t wait_event(void) {
    cron_reminder_event_t event = {0};
    for (int i = 0; i < 6000; i++) {
        int ret = cron_poll_reminder_event(&event);
        assert(ret == 0 || ret == 1 || ret == -EBUSY);
        if (ret == 1) return event;
        usleep(1000);
    }
    fprintf(stderr, "local reminder event did not arrive\n");
    assert(false);
    return event;
}

static void expect_empty_events(void) {
    cron_reminder_event_t event;
    int ret = -EBUSY;
    for (int i = 0; i < 1000 && ret == -EBUSY; i++) {
        ret = cron_poll_reminder_event(&event);
        if (ret == -EBUSY) usleep(1000);
    }
    assert(ret == 0);
}

static void stop_service(void) {
    cron_service_stop();
    wait_for(&tasks, 0);
}

static cJSON *read_saved_jobs(void) {
    FILE *file = fopen(AGENT_CRON_FILE, "r");
    assert(file);
    char json[AGENT_CRON_FILE_MAX_SIZE + 1];
    size_t length = fread(json, 1, sizeof(json) - 1, file);
    assert(!ferror(file) && fclose(file) == 0);
    json[length] = '\0';
    cJSON *root = cJSON_Parse(json);
    assert(root);
    return root;
}

static void expect_saved_after(const char *id, unsigned duration) {
    cJSON *root = read_saved_jobs();
    cJSON *job = NULL;
    bool found = false;
    cJSON_ArrayForEach(job, cJSON_GetObjectItem(root, "jobs")) {
        const char *saved_id = cJSON_GetStringValue(cJSON_GetObjectItem(job, "id"));
        if (!saved_id || strcmp(saved_id, id)) continue;
        const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(job, "kind"));
        cJSON *interval = cJSON_GetObjectItem(job, "interval_s");
        assert(kind && !strcmp(kind, "after"));
        assert(cJSON_IsNumber(interval) && interval->valuedouble == duration);
        assert(cJSON_GetObjectItem(job, "deadline_mono_ms") == NULL);
        found = true;
    }
    cJSON_Delete(root);
    assert(found);
}

static int64_t saved_epoch(const char *id) {
    cJSON *root = read_saved_jobs();
    cJSON *job = NULL;
    int64_t value = -1;
    cJSON_ArrayForEach(job, cJSON_GetObjectItem(root, "jobs")) {
        const char *saved_id = cJSON_GetStringValue(cJSON_GetObjectItem(job, "id"));
        if (!saved_id || strcmp(saved_id, id)) continue;
        cJSON *epoch = cJSON_GetObjectItem(job, "at_epoch");
        assert(cJSON_IsNumber(epoch));
        value = (int64_t)epoch->valuedouble;
        break;
    }
    cJSON_Delete(root);
    return value;
}

static cron_job_t add_slow_action(void) {
    cron_job_t job = {0};
    strcpy(job.name, "slow"); strcpy(job.message, "action complete");
    strcpy(job.channel, "voice"); strcpy(job.action, "slow_fixture");
    job.kind = CRON_KIND_AFTER; job.interval_s = 1; job.delete_after_run = true;
    atomic_store(&tool_entered, 0);
    atomic_store(&tool_gate, 1);
    assert(cron_add_job(&job) == 0);
    return job;
}

static void check_async_creation(void) {
    uint32_t id = 0, ignored = 0xdeadbeefu;
    cron_reminder_create_result_t result;
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_reminder_event_t event;
    char too_long[257]; memset(too_long, 'x', sizeof(too_long));
    too_long[sizeof(too_long) - 1] = '\0';
    assert(cron_create_reminder(30, "stopped", &id) == -ENODEV);
    assert(cron_service_start() == 0);
    assert(cron_create_reminder(0, "zero", &id) == -EINVAL);
    assert(cron_create_reminder(604801, "large", &id) == -EINVAL);
    assert(cron_create_reminder(UINT32_MAX, "overflow", &id) == -EINVAL);
    assert(cron_create_reminder(30, NULL, &id) == -EINVAL);
    assert(cron_create_reminder(30, "", &id) == -EINVAL);
    assert(cron_create_reminder(30, too_long, &id) == -EINVAL);
    assert(cron_create_reminder(30, "no id", NULL) == -EINVAL);
    assert(cron_get_reminder_create_result(0, &result) == -EINVAL);
    assert(cron_get_reminder_create_result(1, NULL) == -EINVAL);
    assert(cron_list_reminders(NULL, 1) == -EINVAL);
    assert(cron_list_reminders(items, 0) == -EINVAL);
    assert(cron_poll_reminder_event(NULL) == -EINVAL);

    atomic_store(&save_entered, 0); atomic_store(&save_gate, 1);
    double start = milliseconds();
    assert(cron_create_reminder(45, "saved local reminder", &id) == 0);
    assert(milliseconds() - start < 50 && id != 0);
    wait_for(&save_entered, 1);
    start = milliseconds();
    assert(cron_get_reminder_create_result(id, &result) == 0);
    assert(result.request_id == id && result.result == -EINPROGRESS && !result.id[0]);
    assert(cron_create_reminder(45, "saved local reminder", &ignored) == -EBUSY);
    assert(cron_create_reminder(10, "different click", &ignored) == -EBUSY);
    assert(ignored == 0xdeadbeefu);
    assert(cron_list_reminders(items, AGENT_CRON_MAX_JOBS) == -EBUSY);
    assert(cron_poll_reminder_event(&event) == 0);
    assert(milliseconds() - start < 50);
    atomic_store(&save_gate, 0);
    result = wait_create(id);
    assert(result.result == 0 && strlen(result.id) == 8);
    wait_reminders(1, items);
    assert(!strcmp(items[0].id, result.id) && items[0].duration_s == 45);
    assert(!items[0].awaiting_clock && items[0].deadline_mono_ms > 0);
    assert(items[0].deadline_mono_ms - (int64_t)milliseconds() <= 45000);
    expect_saved_after(result.id, 45);
    puts("PASS async creation returns before persistence; pending duplicate clicks do not enqueue twice");

    atomic_store(&fail_save, 1);
    uint32_t failed_id;
    assert(cron_create_reminder(30, "must not persist", &failed_id) == 0);
    assert(failed_id != id);
    result = wait_create(failed_id);
    assert(result.result == -EIO && !result.id[0]);
    wait_reminders(1, items);
    assert(cron_get_reminder_create_result(id, &result) == -ESTALE);
    cron_reminder_create_result_t retry = create_local(30, "saved retry");
    wait_reminders(2, items);
    assert(!strcmp(items[0].id, retry.id));
    expect_saved_after(retry.id, 30);
    puts("PASS async save failure remains queryable, preserves existing timers and accepts a fresh retry");

    for (int i = 2; i < AGENT_CRON_MAX_JOBS; i++) add_after(120, "capacity fixture");
    wait_reminders(AGENT_CRON_MAX_JOBS, items);
    uint32_t full_id;
    start = milliseconds();
    assert(cron_create_reminder(30, "too many timers", &full_id) == 0);
    assert(milliseconds() - start < 50);
    result = wait_create(full_id);
    assert(result.result == -ENOSPC && !result.id[0]);
    wait_reminders(AGENT_CRON_MAX_JOBS, items);
    assert(cron_remove_job(items[0].id) == 0);
    retry = create_local(60, "capacity recovered");
    wait_reminders(AGENT_CRON_MAX_JOBS, items);
    expect_saved_after(retry.id, 60);
    clear_jobs(); stop_service();
    puts("PASS full job capacity reports ENOSPC without phantom IDs and recovers after removal");
}

static void check_ordered_timers(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    assert(cron_service_start() == 0);
    cron_job_t late = add_after(80, "late");
    cron_job_t early = add_after(20, "early");
    cron_job_t middle = add_after(60, "middle");
    cron_job_t next = add_after(40, "next");
    wait_reminders(4, items);
    assert(!strcmp(items[0].id, early.id) && !strcmp(items[1].id, next.id));
    assert(!strcmp(items[2].id, middle.id) && !strcmp(items[3].id, late.id));
    struct {
        cron_reminder_snapshot_t entries[2];
        uint32_t guard;
    } limited = {.guard = 0xcafe1234u};
    int ret = -EBUSY;
    for (int i = 0; i < 1000 && ret == -EBUSY; i++) {
        ret = cron_list_reminders(limited.entries, 2);
        if (ret == -EBUSY) usleep(1000);
    }
    assert(ret == 2 && limited.guard == 0xcafe1234u);
    assert(!strcmp(limited.entries[0].id, early.id) && !strcmp(limited.entries[1].id, next.id));
    assert(cron_cancel_reminder(middle.id) == 0);
    wait_reminders(3, items);
    assert(!strcmp(items[0].id, early.id) && !strcmp(items[1].id, next.id));
    assert(!strcmp(items[2].id, late.id));
    assert(cron_cancel_reminder(early.id) == 0);
    wait_reminders(2, items);
    assert(!strcmp(items[0].id, next.id) && !strcmp(items[1].id, late.id));
    clear_jobs(); stop_service();
    puts("PASS multiple reminders sort by remaining time, honor bounded capacity and cancel by stable ID");
}

static void check_ui_events(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_set_ui_notifications(true);
    expect_empty_events();
    int before = atomic_load(&deliveries);
    assert(cron_service_start() == 0);
    cron_reminder_create_result_t reminder = create_local(1, "offline event");
    cron_reminder_event_t event = wait_event();
    assert(event.sequence && !strcmp(event.id, reminder.id));
    assert(!strcmp(event.message, "offline event"));
    wait_reminders(0, items);
    usleep(1100000); expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    uint32_t sequence = event.sequence;
    reminder = create_local(1, "second offline event");
    event = wait_event();
    assert(event.sequence > sequence && !strcmp(event.id, reminder.id));
    wait_reminders(0, items);
    assert(atomic_load(&deliveries) == before);
    cron_set_ui_notifications(false);
    add_after(1, "headless still delivered");
    wait_for(&deliveries, before + 1);
    assert(!strcmp(delivered, "headless still delivered"));
    wait_reminders(0, items); expect_empty_events();
    stop_service();
    puts("PASS UI opt-in delivers one local event without chat outbound; headless delivery remains intact");

    cron_set_ui_notifications(true);
    assert(cron_service_start() == 0);
    char ids[AGENT_CRON_MAX_JOBS + 1][9];
    before = atomic_load(&deliveries);
    for (int i = 0; i < AGENT_CRON_MAX_JOBS; i++) {
        cron_job_t job = add_after(1, "backlog event");
        memcpy(ids[i], job.id, sizeof(ids[i]));
    }
    wait_reminders(0, items);
    cron_job_t overflow = add_after(1, "newest event");
    memcpy(ids[AGENT_CRON_MAX_JOBS], overflow.id, sizeof(overflow.id));
    wait_reminders(0, items);
    for (int i = 1; i <= AGENT_CRON_MAX_JOBS; i++) {
        event = wait_event();
        assert(!strcmp(event.id, ids[i]));
        assert(event.sequence > sequence);
        sequence = event.sequence;
    }
    expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    stop_service(); cron_set_ui_notifications(false);
    puts("PASS bounded event backlog preserves newest notifications, ordered sequence IDs and no outbound fallback");
}

static void check_busy_tool_apis(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_reminder_create_result_t result;
    cron_reminder_event_t event;
    assert(cron_service_start() == 0);
    cron_job_t future = add_after(90, "cancel while tool runs");
    add_slow_action(); wait_for(&tool_entered, 1);
    uint32_t id = 0, duplicate = 0;
    double start = milliseconds();
    assert(cron_list_reminders(items, AGENT_CRON_MAX_JOBS) == -EBUSY);
    assert(cron_get_next_reminder(items) == -EBUSY);
    assert(cron_create_reminder(60, "create while tool runs", &id) == 0);
    assert(cron_get_reminder_create_result(id, &result) == 0 && result.result == -EINPROGRESS);
    assert(cron_create_reminder(60, "duplicate while busy", &duplicate) == -EBUSY);
    assert(cron_poll_reminder_event(&event) == 0);
    assert(cron_cancel_reminder(future.id) == 0);
    assert(milliseconds() - start < 50);
    atomic_store(&tool_gate, 0);
    result = wait_create(id);
    assert(result.result == 0);
    wait_reminders(1, items);
    assert(!strcmp(items[0].id, result.id));
    clear_jobs(); stop_service();
    puts("PASS create/result/list/event/cancel UI APIs never wait for a tool holding the scheduler lock");
}

static void check_clock_jumps(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_set_ui_notifications(true);
    assert(cron_service_start() == 0);
    int before = atomic_load(&deliveries);
    cron_reminder_create_result_t reminder = create_local(6, "clock jump timer");
    wait_reminders(1, items);
    int64_t deadline = items[0].deadline_mono_ms;
    assert(deadline > (int64_t)milliseconds());
    atomic_store(&wall_offset_s, 86400);
    usleep(1200000);
    wait_reminders(1, items);
    assert(items[0].deadline_mono_ms == deadline && !items[0].awaiting_clock);
    assert(items[0].deadline > (int64_t)time(NULL) + 86000);
    expect_empty_events();
    atomic_store(&wall_offset_s, -86400);
    usleep(1200000);
    wait_reminders(1, items);
    assert(items[0].deadline_mono_ms == deadline && !items[0].awaiting_clock);
    assert(items[0].deadline < (int64_t)time(NULL) - 86000);
    expect_empty_events();
    atomic_store(&wall_offset_s, 0);
    cron_reminder_event_t event = wait_event();
    int64_t lateness = (int64_t)milliseconds() - deadline;
    assert(!strcmp(event.id, reminder.id));
    assert(lateness >= 0 && lateness < 2000);
    wait_reminders(0, items); expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    stop_service();
    puts("PASS forward/backward wall-clock jumps re-anchor persistence without changing the live monotonic deadline");

    atomic_store(&wall_offset_s, 120 - (int64_t)time(NULL));
    assert(cron_service_start() == 0);
    reminder = create_local(1, "offline before NTP");
    wait_reminders(1, items);
    assert(items[0].deadline < 1000 && items[0].deadline_mono_ms > 0 && !items[0].awaiting_clock);
    event = wait_event();
    assert(!strcmp(event.id, reminder.id));
    wait_reminders(0, items); stop_service();
    atomic_store(&wall_offset_s, 0);
    cron_set_ui_notifications(false);
    puts("PASS newly created AFTER timer works before NTP without a valid wall clock or cloud delivery");
}

static void check_restart_boundaries(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_set_ui_notifications(true);
    int before = atomic_load(&deliveries);
    cron_job_t saved = add_after(60, "future across restart");
    assert(cron_service_init() == 0);
    wait_reminders(1, items);
    assert(!strcmp(items[0].id, saved.id) && items[0].deadline == saved.at_epoch);
    assert(items[0].duration_s == 60 && !items[0].awaiting_clock);
    assert(llabs(items[0].deadline_mono_ms - saved.deadline_mono_ms) < 1100);
    assert(cron_service_start() == 0);
    assert(cron_service_init() == -EBUSY);
    stop_service();
    atomic_store(&wall_offset_s, 120 - (int64_t)time(NULL));
    assert(cron_service_init() == 0);
    wait_reminders(1, items);
    assert(items[0].awaiting_clock && items[0].deadline_mono_ms == 0);
    assert(cron_service_start() == 0);
    usleep(1200000); expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    atomic_store(&wall_offset_s, 0);
    for (int i = 0; i < 2500; i++) {
        if (cron_list_reminders(items, AGENT_CRON_MAX_JOBS) == 1 && !items[0].awaiting_clock) break;
        usleep(1000);
    }
    assert(!items[0].awaiting_clock && items[0].deadline_mono_ms > (int64_t)milliseconds());
    stop_service();
    puts("PASS valid persisted countdown restores once; invalid reboot clock waits for NTP before resuming");

    atomic_store(&wall_offset_s, 120);
    assert(cron_service_init() == 0);
    wait_reminders(0, items);
    assert(cron_service_start() == 0);
    usleep(1200000); expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    clear_jobs(); stop_service();
    atomic_store(&wall_offset_s, 120 - (int64_t)time(NULL));
    saved = add_after(60, "no trustworthy saved clock");
    assert(saved.at_epoch < 1000);
    assert(cron_service_init() == 0);
    wait_reminders(0, items);
    atomic_store(&wall_offset_s, 0);
    assert(cron_service_start() == 0);
    usleep(1200000); expect_empty_events();
    assert(atomic_load(&deliveries) == before);
    clear_jobs(); stop_service(); cron_set_ui_notifications(false);
    puts("PASS reboot never replays expired or unanchored reminders as fresh countdowns");
}

static void check_reanchor_save_retry(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    atomic_store(&wall_offset_s, 120 - (int64_t)time(NULL));
    assert(cron_service_start() == 0);
    cron_reminder_create_result_t reminder = create_local(60, "retry clock anchor");
    wait_reminders(1, items);
    int64_t deadline = items[0].deadline_mono_ms;
    assert(saved_epoch(reminder.id) < 1000);
    atomic_store(&fail_save, 1);
    atomic_store(&wall_offset_s, 0);
    wait_for(&fail_save, 0);
    int64_t epoch = 0;
    for (int i = 0; i < 5000; i++) {
        epoch = saved_epoch(reminder.id);
        if (epoch >= 1577836800LL) break;
        usleep(1000);
    }
    assert(epoch >= 1577836800LL);
    wait_reminders(1, items);
    assert(items[0].deadline_mono_ms == deadline);
    clear_jobs(); stop_service();
    puts("PASS failed NTP re-anchor persistence retries without another user operation or deadline shift");
}

static void check_hidden_capacity_recovery(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    atomic_store(&wall_offset_s, 120 - (int64_t)time(NULL));
    for (int i = 0; i < AGENT_CRON_MAX_JOBS; i++) add_after(60, "old unanchored timer");
    assert(cron_service_init() == 0);
    wait_reminders(0, items);
    atomic_store(&wall_offset_s, 0);
    assert(cron_service_start() == 0);
    cron_reminder_create_result_t reminder = create_local(60, "visible after reboot");
    wait_reminders(1, items);
    assert(!strcmp(items[0].id, reminder.id));
    clear_jobs(); stop_service();
    puts("PASS discarded one-shot timers after reboot cannot invisibly exhaust all 16 creation slots");
}

static void check_far_future_snapshot(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    cron_job_t nearby = add_after(60, "nearby timer");
    char output[512];
    int ret = tool_cron_add_execute("{\"name\":\"huge epoch\",\"schedule_type\":\"at\","
        "\"at_epoch\":10000000000000000,\"message\":\"far future\",\"channel\":\"voice\"}",
        output, sizeof(output));
    wait_reminders(ret == 0 ? 2 : 1, items);
    assert(!strcmp(items[0].id, nearby.id));
    if (ret == 0) assert(items[1].deadline == 10000000000000000LL);
    clear_jobs();
    puts("PASS very large AT timestamps reject safely or sort behind normal countdowns without integer overflow");
}

static void check_stop_pending_request(void) {
    cron_reminder_snapshot_t items[AGENT_CRON_MAX_JOBS];
    assert(cron_service_start() == 0);
    cron_job_t cancelled = add_after(90, "cancel at shutdown");
    add_slow_action(); wait_for(&tool_entered, 1);
    uint32_t id;
    assert(cron_create_reminder(60, "pending at shutdown", &id) == 0);
    assert(cron_cancel_reminder(cancelled.id) == 0);
    double start = milliseconds();
    cron_service_stop();
    assert(milliseconds() - start < 50);
    uint32_t ignored;
    assert(cron_create_reminder(60, "after stop", &ignored) == -ENODEV);
    atomic_store(&tool_gate, 0); wait_for(&tasks, 0);
    cron_reminder_create_result_t result;
    assert(cron_get_reminder_create_result(id, &result) == 0);
    assert(result.result != -EINPROGRESS);
    assert(result.result == 0 || result.result == -ECANCELED || result.result == -ENODEV);
    int count = cron_list_reminders(items, AGENT_CRON_MAX_JOBS);
    assert(count >= 0);
    int created = 0;
    for (int i = 0; i < count; i++) {
        if (!strcmp(items[i].id, cancelled.id)) {
            assert(items[i].cancel_result == -ECANCELED || items[i].cancel_result == -ENODEV);
        } else {
            assert(result.result == 0 && !strcmp(items[i].id, result.id));
            created++;
        }
    }
    assert(created == (result.result == 0 ? 1 : 0));
    clear_jobs();
    assert(cron_service_init() == 0 && cron_service_start() == 0);
    cron_reminder_create_result_t retry = create_local(60, "fresh after stop");
    wait_reminders(1, items);
    assert(!strcmp(items[0].id, retry.id) && retry.request_id != id);
    assert(cron_cancel_reminder(retry.id) == 0);
    wait_reminders(0, items); stop_service();
    puts("PASS stopping settles accepted create/cancel requests; restart has no ghost timers or stuck requests");
}

static int fail_allocation_at, allocation_number;
static void *cron_fixture_allocate(size_t bytes) {
    if (allocation_number++ == fail_allocation_at) return NULL;
    return malloc(bytes);
}
static size_t read_saved_bytes(char *bytes, size_t capacity) {
    FILE *file = fopen(AGENT_CRON_FILE, "r"); assert(file);
    size_t count = fread(bytes, 1, capacity, file);
    assert(count < capacity && !ferror(file) && fclose(file) == 0);
    return count;
}
static void check_serialization_allocation_failures(void) {
    assert(atomic_load(&tasks) == 0);
    atomic_store(&wall_offset_s, 0);
    clear_jobs();
    cron_job_t original = add_after(60, "keep this reminder");
    char before[4096], after[4096];
    size_t before_count = read_saved_bytes(before, sizeof(before));
    unsigned failed = 0, succeeded = 0;
    for (int i = 0; i < 192; i++) {
        cron_job_t candidate = original;
        strcpy(candidate.action, "slow_fixture");
        strcpy(candidate.action_args, "{}");
        fail_allocation_at = i; allocation_number = 0;
        cJSON_Hooks hooks = {cron_fixture_allocate, free};
        cJSON_InitHooks(&hooks);
        int result = cron_add_job(&candidate);
        cJSON_InitHooks(NULL);
        cron_job_t jobs[2];
        if (result == 0) {
            succeeded++;
            assert(cron_list_jobs(jobs, 2) == 2);
            assert(cron_remove_job(candidate.id) == 0);
        } else {
            failed++;
            assert(cron_list_jobs(jobs, 2) == 1 && !strcmp(jobs[0].id, original.id));
        }
        size_t after_count = read_saved_bytes(after, sizeof(after));
        assert(before_count == after_count && !memcmp(before, after, before_count));
    }
    assert(failed > 60 && succeeded > 0);
    clear_jobs();
    puts("PASS every cJSON allocation failure preserves existing reminders and their complete persisted JSON");
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    unlink(AGENT_CRON_FILE); unlink(AGENT_CRON_FILE ".tmp");
    assert(cron_service_init() == 0);
    char input[512], output[512];
    const char *invalid[] = {"0", "-1", "0.5", "604801", "1e999", "\"nan\"", "\"inf\"", "\"1s\""};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        snprintf(input, sizeof(input), "{\"name\":\"bad\",\"schedule_type\":\"after\",\"delay_s\":%s,\"message\":\"test\"}", invalid[i]);
        assert(tool_cron_add_execute(input, output, sizeof(output)) != 0);
    }
    puts("PASS after rejects zero/negative/fractional/oversized/nonfinite/malformed delay");
    cron_job_t notification = {0};
    strcpy(notification.name, "notification"); strcpy(notification.message, "test");
    strcpy(notification.channel, "voice"); strcpy(notification.chat_id, "wake-12");
    notification.kind = CRON_KIND_AT; notification.at_epoch = time(NULL) + 60;
    assert(cron_add_job(&notification) == 0);
    assert(!strcmp(notification.chat_id, "reminder"));
    assert(cron_remove_job(notification.id) == 0);
    strcpy(notification.channel, "lvgl_ui"); strcpy(notification.chat_id, "ui");
    assert(cron_add_job(&notification) == 0);
    assert(!strcmp(notification.chat_id, "reminder"));
    assert(cron_remove_job(notification.id) == 0);
    puts("PASS local reminders cannot reuse a wake or manual conversation ID");
    cron_job_t first = add_after(30, "first reminder");
    cron_reminder_snapshot_t snapshot;
    assert(cron_get_next_reminder(&snapshot) == 0);
    assert(!strcmp(snapshot.id, first.id) && snapshot.deadline == first.next_run);
    assert(snapshot.started_at == first.created_at && snapshot.deadline - snapshot.started_at == 30);
    cron_job_t earlier = add_after(20, "earlier reminder");
    assert(cron_get_next_reminder(&snapshot) == 0 && !strcmp(snapshot.id, earlier.id));
    assert(cron_service_init() == 0);
    assert(cron_get_next_reminder(&snapshot) == 0 && snapshot.deadline == earlier.next_run);
    assert(snapshot.started_at == earlier.created_at);
    puts("PASS earliest reminder and its original deadline/progress survive persistence reload");

    atomic_store(&fail_save, 1);
    snprintf(input, sizeof(input), "{\"name\":\"failed\",\"schedule_type\":\"after\",\"delay_s\":10,\"message\":\"bad save\"}");
    assert(tool_cron_add_execute(input, output, sizeof(output)) != 0);
    assert(cron_service_init() == 0);
    assert(cron_get_next_reminder(&snapshot) == 0 && !strcmp(snapshot.id, earlier.id));
    atomic_store(&fail_save, 1); assert(cron_remove_job(earlier.id) != 0);
    assert(cron_get_next_reminder(&snapshot) == 0 && !strcmp(snapshot.id, earlier.id));
    puts("PASS failed persistence does not report a new timer or lose existing jobs");

    assert(cron_service_start() == 0);
    atomic_store(&fail_save, 1);
    assert(cron_cancel_reminder(earlier.id) == 0);
    for (int i = 0; i < 2500; i++) {
        if (cron_get_next_reminder(&snapshot) == 0 && snapshot.cancel_result == -EIO) break;
        usleep(1000);
    }
    assert(snapshot.cancel_result == -EIO && !strcmp(snapshot.id, earlier.id));
    assert(cron_cancel_reminder(earlier.id) == 0);
    for (int i = 0; i < 2500; i++) {
        if (cron_get_next_reminder(&snapshot) == 0 && strcmp(snapshot.id, earlier.id)) break;
        usleep(1000);
    }
    assert(!strcmp(snapshot.id, first.id));
    clear_jobs();
    puts("PASS UI cancellation is asynchronous, reports failures and can retry");

    cron_job_t action = {0};
    strcpy(action.name, "slow"); strcpy(action.message, "action done");
    strcpy(action.channel, "voice"); strcpy(action.chat_id, "reminder");
    strcpy(action.action, "slow_fixture"); action.kind = CRON_KIND_AT;
    action.at_epoch = time(NULL) + 1; action.delete_after_run = true;
    assert(cron_add_job(&action) == 0); first = add_after(30, "visible reminder");
    atomic_store(&tool_gate, 1); wait_for(&tool_entered, 1);
    double start = milliseconds();
    assert(cron_get_next_reminder(&snapshot) == -EBUSY);
    assert(cron_cancel_reminder(first.id) == 0);
    assert(milliseconds() - start < 50);
    atomic_store(&tool_gate, 0);
    for (int i = 0; i < 2500; i++) {
        if (cron_get_next_reminder(&snapshot) == 0 && !snapshot.id[0]) break;
        usleep(1000);
    }
    assert(!snapshot.id[0]);
    puts("PASS UI polling and cancellation never wait behind a running cron tool");

    int before = atomic_load(&deliveries);
    first = add_after(2, "countdown complete");
    wait_for(&deliveries, before + 1);
    assert(!strcmp(delivered, "countdown complete"));
    assert(time(NULL) - first.next_run <= 1);
    int snapshot_result = -EBUSY;
    for (int i = 0; i < 2000; i++) {
        snapshot_result = cron_get_next_reminder(&snapshot);
        if (snapshot_result == 0 && !snapshot.id[0]) break;
        usleep(1000);
    }
    assert(snapshot_result == 0 && !snapshot.id[0]);
    usleep(1100000); assert(atomic_load(&deliveries) == before + 1);
    cron_service_stop(); wait_for(&tasks, 0);
    puts("PASS deadline fires once within one scheduler tick and clears the UI snapshot");
    check_async_creation();
    check_ordered_timers();
    check_ui_events();
    check_busy_tool_apis();
    check_clock_jumps();
    check_restart_boundaries();
    check_reanchor_save_retry();
    check_hidden_capacity_recovery();
    check_far_future_snapshot();
    check_stop_pending_request();
    check_serialization_allocation_failures();
    return 0;
}
