#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum save_fault
{
  FAULT_NONE,
  FAULT_OPEN,
  FAULT_WRITE,
  FAULT_FLUSH,
  FAULT_CLOSE,
  FAULT_RENAME
};

static enum save_fault fault;

static FILE *focus_test_fopen(const char *path, const char *mode)
{
  if (mode[0] == 'w' && fault == FAULT_OPEN)
    {
      fault = FAULT_NONE;
      errno = EACCES;
      return NULL;
    }

  return fopen(path, mode);
}

static size_t focus_test_fwrite(const void *data, size_t size, size_t count,
                               FILE *file)
{
  if (fault == FAULT_WRITE)
    {
      fault = FAULT_NONE;
      errno = ENOSPC;
      return 0;
    }

  return fwrite(data, size, count, file);
}

static int focus_test_fflush(FILE *file)
{
  int result = fflush(file);
  if (fault == FAULT_FLUSH)
    {
      fault = FAULT_NONE;
      errno = ENOSPC;
      return EOF;
    }

  return result;
}

static int focus_test_fclose(FILE *file)
{
  int result = fclose(file);
  if (fault == FAULT_CLOSE)
    {
      fault = FAULT_NONE;
      errno = EIO;
      return EOF;
    }

  return result;
}

static int focus_test_rename(const char *old_path, const char *new_path)
{
  if (fault == FAULT_RENAME)
    {
      fault = FAULT_NONE;
      errno = EIO;
      return -1;
    }

  return rename(old_path, new_path);
}

/* Compile the production module itself, with only its file-I/O failure points
 * substituted. No public build flags or production test hooks are needed.
 */

#define fopen focus_test_fopen
#define fwrite focus_test_fwrite
#define fflush focus_test_fflush
#define fclose focus_test_fclose
#define rename focus_test_rename
#include "../app/hello_app/study_focus_stats.c"
#undef fopen
#undef fwrite
#undef fflush
#undef fclose
#undef rename

static void write_bytes(const char *path, const void *text, size_t size)
{
  FILE *file = fopen(path, "wb");
  assert(file != NULL);
  assert(fwrite(text, 1, size, file) == size);
  assert(fclose(file) == 0);
}

static void write_text(const char *path, const char *text)
{
  write_bytes(path, text, strlen(text));
}

static char *read_text(const char *path)
{
  char *text = calloc(1, 8192);
  FILE *file = fopen(path, "rb");
  assert(text != NULL && file != NULL);
  size_t size = fread(text, 1, 8191, file);
  assert(!ferror(file) && size < 8191);
  assert(fclose(file) == 0);
  return text;
}

static void assert_equal(const study_focus_stats_t *a,
                         const study_focus_stats_t *b)
{
  assert(a->preset == b->preset && a->goal_minutes == b->goal_minutes);
  assert(a->pending_rounds == b->pending_rounds);
  assert(a->pending_minutes == b->pending_minutes);
  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      assert(a->days[i].day == b->days[i].day);
      assert(a->days[i].rounds == b->days[i].rounds);
      assert(a->days[i].minutes == b->days[i].minutes);
    }
}

static void assert_default(const study_focus_stats_t *stats, int day)
{
  study_focus_stats_t expected;
  study_focus_stats_init(&expected, day);
  assert_equal(stats, &expected);
}

static void test_migration(const char *path)
{
  study_focus_stats_t stats;
  const char *legacy = "{\"version\":1,\"day\":20260911,\"rounds\":3,"
                       "\"minutes\":75,\"preset\":2}";

  write_text(path, legacy);
  assert(study_focus_stats_load(&stats, path, 20260912) == 0);
  assert(stats.days[0].day == 20260906 && stats.days[6].day == 20260912);
  assert(stats.days[5].rounds == 3 && stats.days[5].minutes == 75);
  assert(stats.days[6].rounds == 0 && stats.days[6].minutes == 0);
  assert(stats.preset == 2 && stats.goal_minutes == 60);
  assert(study_focus_stats_streak(&stats) == 1);
  assert(study_focus_stats_load(&stats, path, 20260913) == 0);
  assert(stats.days[4].minutes == 75 && stats.preset == 2);
  assert(study_focus_stats_streak(&stats) == 0);
  assert(study_focus_stats_load(&stats, path, 20260920) == 0);
  assert(stats.preset == 2 && stats.days[6].day == 20260920);
  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      assert(stats.days[i].minutes == 0);
    }

  assert(study_focus_stats_load(&stats, path, 20260910) == 0);
  assert(stats.days[6].day == 20260911 && stats.days[6].minutes == 75);
  puts("PASS legacy migration keeps the correct calendar slot and preset");
}

static void test_calendar(void)
{
  study_focus_stats_t stats;
  study_focus_stats_init(&stats, 20251231);
  stats.preset = 2;
  stats.goal_minutes = 90;
  assert(study_focus_stats_record_round(&stats, 20251231, 45));
  assert(study_focus_stats_set_day(&stats, 20260101));
  assert(stats.days[0].day == 20251226 && stats.days[5].day == 20251231);
  assert(stats.days[5].minutes == 45 && stats.days[6].minutes == 0);
  assert(stats.preset == 2 && stats.goal_minutes == 90);
  study_focus_stats_t saved = stats;
  assert(!study_focus_stats_set_day(&stats, 20260101));
  assert_equal(&stats, &saved);

  const int cases[][4] =
  {
    {20260131, 20260201, 20260131, 20260130},
    {20240228, 20240301, 20240229, 20240228},
    {20260228, 20260301, 20260228, 20260227},
    {20000228, 20000301, 20000229, 20000228},
    {21000228, 21000301, 21000228, 21000227}
  };
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
      study_focus_stats_init(&stats, cases[i][0]);
      assert(study_focus_stats_record_round(&stats, cases[i][0], 25));
      assert(study_focus_stats_set_day(&stats, cases[i][1]));
      assert(stats.days[5].day == cases[i][2]);
      assert(stats.days[4].day == cases[i][3]);
      uint32_t total = 0;
      for (int j = 0; j < STUDY_FOCUS_DAYS; j++)
        {
          total += stats.days[j].minutes;
        }

      assert(total == 25);
    }

  study_focus_stats_init(&stats, 20260901);
  for (int day = 20260901; day <= 20260909; day++)
    {
      assert(study_focus_stats_record_round(&stats, day, 15));
    }

  assert(stats.days[0].day == 20260903);
  assert(stats.days[6].day == 20260909);
  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      assert(stats.days[i].rounds == 1 && stats.days[i].minutes == 15);
    }

  assert(study_focus_stats_set_day(&stats, 20261231));
  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      assert(stats.days[i].minutes == 0);
    }

  puts("PASS calendar rollover covers months, leap rules, years and long gaps");
}

static void test_unsynced(const char *path)
{
  study_focus_stats_t stats;
  study_focus_stats_t reload;
  study_focus_stats_init(&stats, 0);
  assert(study_focus_stats_record_round(&stats, 0, 25));
  assert(study_focus_stats_record_round(&stats, 0, 15));
  assert(stats.days[6].day == 0 && stats.days[6].minutes == 40);
  assert(study_focus_stats_streak(&stats) == 0);
  assert(study_focus_stats_save(&stats, path) == 0);
  assert(study_focus_stats_load(&reload, path, 0) == 0);
  assert_equal(&stats, &reload);
  assert(study_focus_stats_set_day(&stats, 20260912));
  assert(stats.days[6].rounds == 2 && stats.days[6].minutes == 40);
  assert(!study_focus_stats_set_day(&stats, 20260912));
  assert(study_focus_stats_load(&reload, path, 20260912) == 0);
  assert_equal(&stats, &reload);
  assert(study_focus_stats_load(&reload, path, 20260912) == 0);
  assert_equal(&stats, &reload);

  write_text(path, "{\"version\":1,\"day\":0,\"rounds\":1,"
                   "\"minutes\":45,\"preset\":2}");
  assert(study_focus_stats_load(&reload, path, 20260912) == 0);
  assert(reload.days[6].minutes == 45 && reload.preset == 2);
  puts("PASS undated rounds survive reload and are assigned only once");
}

static void test_pending(const char *path)
{
  study_focus_stats_t stats;
  study_focus_stats_t reload;
  study_focus_stats_init(&stats, 20260910);
  assert(study_focus_stats_record_round(&stats, 20260910, 25));
  stats.preset = 2;
  stats.goal_minutes = 90;
  assert(study_focus_stats_record_round(&stats, 0, 15));
  assert(study_focus_stats_record_round(&stats, 20260909, 45));
  assert(stats.pending_rounds == 2 && stats.pending_minutes == 60);
  assert(stats.days[6].day == 20260910 && stats.days[6].minutes == 25);
  study_focus_stats_t saved = stats;
  assert(!study_focus_stats_set_day(&stats, 20260909));
  assert(!study_focus_stats_set_day(&stats, 0));
  assert_equal(&stats, &saved);
  assert(study_focus_stats_save(&stats, path) == 0);
  assert(study_focus_stats_load(&reload, path, 0) == 0);
  assert_equal(&reload, &stats);
  assert(study_focus_stats_load(&reload, path, 20260909) == 0);
  assert_equal(&reload, &stats);
  assert(study_focus_stats_load(&reload, path, 20260910) == 0);
  assert(reload.days[6].rounds == 3 && reload.days[6].minutes == 85);
  assert(reload.pending_rounds == 0 && reload.pending_minutes == 0);
  assert(reload.preset == 2 && reload.goal_minutes == 90);
  assert(!study_focus_stats_set_day(&reload, 20260910));
  assert(study_focus_stats_save(&reload, path) == 0);
  assert(study_focus_stats_load(&stats, path, 20260910) == 0);
  assert_equal(&reload, &stats);

  assert(study_focus_stats_record_round(&stats, 0, 30));
  assert(study_focus_stats_record_round(&stats, 20260912, 15));
  assert(stats.days[4].day == 20260910 && stats.days[4].minutes == 85);
  assert(stats.days[6].day == 20260912 && stats.days[6].minutes == 45);
  assert(stats.days[6].rounds == 2 && stats.pending_rounds == 0);
  assert(study_focus_stats_record_round(&stats, 20260901, 25));
  assert(study_focus_stats_set_day(&stats, 20261001));
  assert(stats.days[6].minutes == 25 && stats.pending_minutes == 0);
  puts("PASS clock loss and rollback retain pending rounds through restart");
}

static void test_streak(void)
{
  study_focus_stats_t stats;
  study_focus_stats_init(&stats, 20260901);
  assert(study_focus_stats_streak(&stats) == 0);
  for (int day = 20260901; day <= 20260907; day++)
    {
      assert(study_focus_stats_record_round(&stats, day, 15));
      assert(study_focus_stats_streak(&stats) == (unsigned)(day - 20260900));
    }

  assert(study_focus_stats_set_day(&stats, 20260908));
  assert(study_focus_stats_streak(&stats) == 6);
  assert(study_focus_stats_set_day(&stats, 20260909));
  assert(study_focus_stats_streak(&stats) == 0);
  assert(study_focus_stats_record_round(&stats, 20260909, 25));
  assert(study_focus_stats_streak(&stats) == 1);
  assert(study_focus_stats_record_round(&stats, 0, 25));
  assert(study_focus_stats_streak(&stats) == 1);
  puts("PASS streak counts real consecutive dates and ignores pending rounds");
}

static void test_validation(const char *path)
{
  const char *invalid[] =
  {
    "", "{", "[]", "null", "[[[[[0]]]]]",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25}",
    "{\"version\":0,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1}",
    "{\"version\":3,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1}",
    "{\"version\":1.5,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20260229,\"rounds\":1,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20261301,\"rounds\":1,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":-1,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1.5,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":4294967296,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":1e999,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":\"25\",\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":0,\"minutes\":25,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":0,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":2,\"minutes\":1,\"preset\":1}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":3}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1,\"preset\":2}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1,\"goal_minutes\":45}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1,\"goal_minutes\":30,\"goal_minutes\":60}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1,\"pending_rounds\":1}",
    "{\"version\":1,\"day\":0,\"rounds\":1,\"minutes\":25,\"preset\":1,\"pending_rounds\":1,\"pending_minutes\":25}",
    "{\"version\":1,\"day\":20260912,\"rounds\":1,\"minutes\":25,\"preset\":1} trailing"
  };
  study_focus_stats_t stats;
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
      write_text(path, invalid[i]);
      memset(&stats, 0xa5, sizeof(stats));
      assert(study_focus_stats_load(&stats, path, 20260912) < 0);
      assert_default(&stats, 20260912);
    }

  const char embedded[] = "{\"version\":1,\"day\":0,\"rounds\":0,"
                          "\"minutes\":0,\"preset\":1}\0hidden";
  write_bytes(path, embedded, sizeof(embedded) - 1);
  assert(study_focus_stats_load(&stats, path, 20260912) < 0);
  assert_default(&stats, 20260912);
  char *large = malloc(FOCUS_JSON_LIMIT + 2);
  assert(large != NULL);
  memset(large, ' ', FOCUS_JSON_LIMIT + 2);
  write_bytes(path, large, FOCUS_JSON_LIMIT + 2);
  free(large);
  assert(study_focus_stats_load(&stats, path, 20260912) == -EFBIG);
  assert_default(&stats, 20260912);

  study_focus_stats_init(&stats, 20260912);
  assert(study_focus_stats_save(&stats, path) == 0);
  char *valid = read_text(path);
  for (int variant = 0; variant < 4; variant++)
    {
      cJSON *root = cJSON_Parse(valid);
      assert(root != NULL);
      cJSON *days = cJSON_GetObjectItemCaseSensitive(root, "days");
      if (variant == 0)
        {
          cJSON_DeleteItemFromArray(days, 0);
        }
      else
        {
          cJSON *entry = variant == 3 ? root : cJSON_GetArrayItem(days, 0);
          cJSON *field = cJSON_GetObjectItemCaseSensitive(entry, "day");
          cJSON_SetNumberValue(field, variant == 1 ? 20260907 : 20260911);
          if (variant == 2)
            {
              cJSON_AddNumberToObject(entry, "minutes", 10);
            }
        }

      char *text = cJSON_PrintUnformatted(root);
      assert(text != NULL);
      write_text(path, text);
      cJSON_free(text);
      cJSON_Delete(root);
      assert(study_focus_stats_load(&stats, path, 20260912) < 0);
      assert_default(&stats, 20260912);
    }

  free(valid);
  puts("PASS malformed, duplicate, oversized, fractional and inconsistent JSON is rejected");
}

static void test_roundtrip_and_limits(const char *path)
{
  study_focus_stats_t stats;
  study_focus_stats_t reload;
  study_focus_stats_init(&stats, 20260912);
  study_focus_stats_t before = stats;
  const int invalid_days[] = {-1, 1, 20260012, 20260431, 20260229, 100000101};
  for (unsigned i = 0; i < sizeof(invalid_days) / sizeof(invalid_days[0]); i++)
    {
      assert(!study_focus_stats_set_day(&stats, invalid_days[i]));
      assert(!study_focus_stats_record_round(&stats, invalid_days[i], 25));
      assert_equal(&stats, &before);
    }

  assert(!study_focus_stats_record_round(&stats, 20260912, 0));
  assert_equal(&stats, &before);
  for (unsigned goal = 30; goal <= 90; goal += 30)
    {
      stats.goal_minutes = goal;
      stats.preset = 2;
      assert(study_focus_stats_record_round(&stats, 20260912, 45));
      assert(study_focus_stats_save(&stats, path) == 0);
      assert(study_focus_stats_load(&reload, path, 20260912) == 0);
      assert_equal(&stats, &reload);
      char *text = read_text(path);
      cJSON *root = cJSON_Parse(text);
      assert(root != NULL);
      assert(cJSON_GetObjectItemCaseSensitive(root, "day")->valueint == 20260912);
      assert(cJSON_GetObjectItemCaseSensitive(root, "rounds")->valuedouble ==
             stats.days[6].rounds);
      assert(cJSON_GetObjectItemCaseSensitive(root, "minutes")->valuedouble ==
             stats.days[6].minutes);
      cJSON_Delete(root);
      free(text);
    }

  stats.days[6].rounds = UINT32_MAX;
  stats.days[6].minutes = UINT32_MAX;
  before = stats;
  assert(!study_focus_stats_record_round(&stats, 20260912, 1));
  assert_equal(&stats, &before);
  assert(study_focus_stats_save(&stats, path) == 0);
  assert(study_focus_stats_load(&reload, path, 20260912) == 0);
  assert_equal(&stats, &reload);
  assert(study_focus_stats_record_round(&stats, 0, 1));
  before = stats;
  assert(!study_focus_stats_set_day(&stats, 20260912));
  assert(!study_focus_stats_record_round(&stats, 20260912, 1));
  assert_equal(&stats, &before);
  assert(study_focus_stats_set_day(&stats, 20260913));
  assert(stats.days[6].minutes == 1 && stats.pending_minutes == 0);
  stats.pending_rounds = UINT32_MAX;
  stats.pending_minutes = UINT32_MAX;
  before = stats;
  assert(!study_focus_stats_record_round(&stats, 0, 1));
  assert(!study_focus_stats_set_day(&stats, 20260913));
  assert_equal(&stats, &before);
  stats.goal_minutes = 45;
  assert(study_focus_stats_save(&stats, path) == -EINVAL);
  stats.goal_minutes = 60;
  stats.preset = 3;
  assert(study_focus_stats_save(&stats, path) == -EINVAL);
  assert(study_focus_stats_save(NULL, path) == -EINVAL);
  assert(study_focus_stats_load(NULL, path, 20260912) == -EINVAL);
  assert(study_focus_stats_load(&stats, NULL, 20260912) == -EINVAL);
  assert_default(&stats, 20260912);
  puts("PASS goals and full uint32 counts round-trip without overflow or partial updates");
}

static long allocations_left;

static void *limited_malloc(size_t size)
{
  if (allocations_left-- <= 0)
    {
      return NULL;
    }

  return malloc(size);
}

static void test_save_failures(const char *path)
{
  study_focus_stats_t stats;
  study_focus_stats_t before;
  study_focus_stats_init(&stats, 20260912);
  assert(study_focus_stats_record_round(&stats, 20260912, 25));
  before = stats;
  const char *sentinel = "previous file must stay intact\n";
  for (enum save_fault f = FAULT_OPEN; f <= FAULT_RENAME; f++)
    {
      write_text(path, sentinel);
      fault = f;
      assert(study_focus_stats_save(&stats, path) < 0);
      assert(fault == FAULT_NONE);
      assert_equal(&stats, &before);
      char *saved = read_text(path);
      assert(strcmp(saved, sentinel) == 0);
      free(saved);
    }

  cJSON_Hooks hooks = {limited_malloc, free};
  bool success = false;
  unsigned failures = 0;
  for (long limit = 0; limit < 200; limit++)
    {
      write_text(path, sentinel);
      allocations_left = limit;
      cJSON_InitHooks(&hooks);
      int result = study_focus_stats_save(&stats, path);
      cJSON_InitHooks(NULL);
      assert_equal(&stats, &before);
      if (result == 0)
        {
          success = true;
          break;
        }

      assert(result == -ENOMEM);
      failures++;
      char *saved = read_text(path);
      assert(strcmp(saved, sentinel) == 0);
      free(saved);
    }

  assert(success && failures > 20);
  success = false;
  failures = 0;
  for (long limit = 0; limit < 200; limit++)
    {
      allocations_left = limit;
      cJSON_InitHooks(&hooks);
      int result = study_focus_stats_load(&stats, path, 20260912);
      cJSON_InitHooks(NULL);
      if (result == 0)
        {
          assert_equal(&stats, &before);
          success = true;
          break;
        }

      assert(result < 0);
      assert_default(&stats, 20260912);
      failures++;
    }

  assert(success && failures > 20);
  puts("PASS write/flush/close/rename and allocation failures preserve prior data");
}

int main(void)
{
  char directory[] = "/tmp/study-focus-XXXXXX";
  char path[256];
  char missing[256];
  setvbuf(stdout, NULL, _IOLBF, 0);
  assert(mkdtemp(directory) != NULL);
  snprintf(path, sizeof(path), "%s/STUDY_FOCUS.json", directory);
  snprintf(missing, sizeof(missing), "%s/missing/STUDY_FOCUS.json", directory);
  study_focus_stats_t stats;
  assert(study_focus_stats_load(&stats, missing, 20260912) == -ENOENT);
  assert_default(&stats, 20260912);
  assert(study_focus_stats_save(&stats, missing) == -ENOENT);
  test_migration(path);
  test_calendar();
  test_unsynced(path);
  test_pending(path);
  test_streak();
  test_validation(path);
  test_roundtrip_and_limits(path);
  test_save_failures(path);
  assert(unlink(path) == 0);
  assert(rmdir(directory) == 0);
  puts("focus stats: 8 scenario groups passed");
  return 0;
}
