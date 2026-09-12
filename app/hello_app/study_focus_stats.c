/****************************************************************************
 * study_focus_stats.c - Persistent, calendar-based focus history
 ****************************************************************************/

#include "study_focus_stats.h"

#include <errno.h>
#include <math.h>
#include <netutils/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOCUS_JSON_LIMIT 4096
#define FOCUS_VERSION 2

static int month_days(int year, int month)
{
  static const uint8_t lengths[] =
  {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

  if (month == 2 && year % 4 == 0 &&
      (year % 100 != 0 || year % 400 == 0))
    {
      return 29;
    }

  return lengths[month - 1];
}

static bool valid_day(int day)
{
  int year = day / 10000;
  int month = day / 100 % 100;
  int date = day % 100;

  return year >= 1 && year <= 9999 && month >= 1 && month <= 12 &&
         date >= 1 && date <= month_days(year, month);
}

static int previous_day(int day)
{
  int year = day / 10000;
  int month = day / 100 % 100;
  int date = day % 100;

  if (day <= 10101)
    {
      return 0;
    }

  if (--date == 0)
    {
      if (--month == 0)
        {
          year--;
          month = 12;
        }

      date = month_days(year, month);
    }

  return year * 10000 + month * 100 + date;
}

static void init_window(study_focus_day_t *days, int day)
{
  memset(days, 0, sizeof(*days) * STUDY_FOCUS_DAYS);
  for (int i = STUDY_FOCUS_DAYS - 1; i >= 0; i--)
    {
      days[i].day = day;
      day = previous_day(day);
    }
}

static bool valid_counts(uint32_t rounds, uint32_t minutes)
{
  return (rounds == 0) == (minutes == 0) && rounds <= minutes;
}

static bool add_counts(uint32_t *rounds, uint32_t *minutes,
                       uint32_t new_rounds, uint32_t new_minutes)
{
  if (new_rounds > UINT32_MAX - *rounds ||
      new_minutes > UINT32_MAX - *minutes)
    {
      return false;
    }

  *rounds += new_rounds;
  *minutes += new_minutes;
  return true;
}

static bool valid_stats(const study_focus_stats_t *stats)
{
  int day;

  if (stats == NULL || stats->preset > 2 ||
      (stats->goal_minutes != 30 && stats->goal_minutes != 60 &&
       stats->goal_minutes != 90) ||
      !valid_counts(stats->pending_rounds, stats->pending_minutes))
    {
      return false;
    }

  day = stats->days[STUDY_FOCUS_DAYS - 1].day;
  if ((day != 0 && !valid_day(day)) ||
      (day == 0 && stats->pending_rounds != 0))
    {
      return false;
    }

  for (int i = STUDY_FOCUS_DAYS - 1; i >= 0; i--)
    {
      const study_focus_day_t *entry = &stats->days[i];

      if (entry->day != day ||
          !valid_counts(entry->rounds, entry->minutes) ||
          (day == 0 && i != STUDY_FOCUS_DAYS - 1 && entry->rounds != 0))
        {
          return false;
        }

      day = previous_day(day);
    }

  return true;
}

void study_focus_stats_init(study_focus_stats_t *stats, int day)
{
  if (stats == NULL)
    {
      return;
    }

  memset(stats, 0, sizeof(*stats));
  stats->preset = 1;
  stats->goal_minutes = 60;
  init_window(stats->days, valid_day(day) ? day : 0);
}

bool study_focus_stats_set_day(study_focus_stats_t *stats, int day)
{
  study_focus_day_t window[STUDY_FOCUS_DAYS];
  int old_day;

  if (!valid_stats(stats) || !valid_day(day))
    {
      return false;
    }

  old_day = stats->days[STUDY_FOCUS_DAYS - 1].day;
  if (day < old_day || (day == old_day && stats->pending_rounds == 0))
    {
      return false;
    }

  init_window(window, day);
  if (old_day == 0)
    {
      window[STUDY_FOCUS_DAYS - 1].rounds =
        stats->days[STUDY_FOCUS_DAYS - 1].rounds;
      window[STUDY_FOCUS_DAYS - 1].minutes =
        stats->days[STUDY_FOCUS_DAYS - 1].minutes;
    }
  else
    {
      /* Copy matching calendar slots, never add them. Repeated clock updates
       * and reloads must not credit the same persisted tally again.
       */

      for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
        {
          for (int j = 0; j < STUDY_FOCUS_DAYS; j++)
            {
              if (window[i].day != 0 &&
                  window[i].day == stats->days[j].day)
                {
                  window[i] = stats->days[j];
                  break;
                }
            }
        }
    }

  if (!add_counts(&window[STUDY_FOCUS_DAYS - 1].rounds,
                  &window[STUDY_FOCUS_DAYS - 1].minutes,
                  stats->pending_rounds, stats->pending_minutes))
    {
      return false;
    }

  memcpy(stats->days, window, sizeof(window));
  stats->pending_rounds = 0;
  stats->pending_minutes = 0;
  return true;
}

bool study_focus_stats_record_round(study_focus_stats_t *stats, int day,
                                    uint32_t minutes)
{
  study_focus_stats_t next;
  study_focus_day_t *today;

  if (!valid_stats(stats) || minutes == 0 ||
      (day != 0 && !valid_day(day)))
    {
      return false;
    }

  next = *stats;
  if (day < next.days[STUDY_FOCUS_DAYS - 1].day)
    {
      if (!add_counts(&next.pending_rounds, &next.pending_minutes, 1, minutes))
        {
          return false;
        }

      *stats = next;
      return true;
    }

  study_focus_stats_set_day(&next, day);
  today = &next.days[STUDY_FOCUS_DAYS - 1];
  if (next.pending_rounds != 0 || today->day != day ||
      !add_counts(&today->rounds, &today->minutes, 1, minutes))
    {
      return false;
    }

  *stats = next;
  return true;
}

unsigned study_focus_stats_streak(const study_focus_stats_t *stats)
{
  unsigned streak = 0;
  int i = STUDY_FOCUS_DAYS - 1;

  if (!valid_stats(stats) || stats->days[i].day == 0)
    {
      return 0;
    }

  if (stats->days[i].minutes == 0)
    {
      i--;
    }

  for (; i >= 0 && stats->days[i].day != 0 && stats->days[i].minutes != 0;
       i--)
    {
      streak++;
    }

  return streak;
}

static bool json_field(const cJSON *object, const char *name,
                       const cJSON **field)
{
  *field = NULL;
  if (!cJSON_IsObject(object))
    {
      return false;
    }

  for (const cJSON *item = object->child; item != NULL; item = item->next)
    {
      if (item->string != NULL && strcmp(item->string, name) == 0)
        {
          if (*field != NULL)
            {
              return false;
            }

          *field = item;
        }
    }

  return true;
}

static bool json_uint(const cJSON *object, const char *name,
                      uint32_t maximum, uint32_t *value)
{
  const cJSON *field;
  double number;

  if (!json_field(object, name, &field) || !cJSON_IsNumber(field))
    {
      return false;
    }

  number = field->valuedouble;
  if (!isfinite(number) || number < 0 || number > maximum ||
      (double)(uint32_t)number != number)
    {
      return false;
    }

  *value = (uint32_t)number;
  return true;
}

static bool json_day(const cJSON *object, study_focus_day_t *day)
{
  uint32_t stamp;

  if (!json_uint(object, "day", 99991231, &stamp) ||
      (stamp != 0 && !valid_day((int)stamp)) ||
      !json_uint(object, "rounds", UINT32_MAX, &day->rounds) ||
      !json_uint(object, "minutes", UINT32_MAX, &day->minutes))
    {
      return false;
    }

  day->day = (int)stamp;
  return valid_counts(day->rounds, day->minutes);
}

static bool parse_stats(const cJSON *root, study_focus_stats_t *stats)
{
  study_focus_day_t top;
  const cJSON *days;
  const cJSON *goal;
  const cJSON *pending_rounds;
  const cJSON *pending_minutes;
  uint32_t version;
  uint32_t preset;
  uint32_t goal_minutes = 60;

  if (!json_uint(root, "version", FOCUS_VERSION, &version) || version == 0 ||
      !json_uint(root, "preset", 2, &preset) || !json_day(root, &top) ||
      !json_field(root, "goal_minutes", &goal) ||
      (goal != NULL &&
       !json_uint(root, "goal_minutes", 90, &goal_minutes)) ||
      !json_field(root, "days", &days) ||
      !json_field(root, "pending_rounds", &pending_rounds) ||
      !json_field(root, "pending_minutes", &pending_minutes) ||
      (pending_rounds == NULL) != (pending_minutes == NULL))
    {
      return false;
    }

  study_focus_stats_init(stats, top.day);
  stats->preset = (uint8_t)preset;
  stats->goal_minutes = (uint16_t)goal_minutes;
  if (pending_rounds != NULL &&
      (!json_uint(root, "pending_rounds", UINT32_MAX, &stats->pending_rounds) ||
       !json_uint(root, "pending_minutes", UINT32_MAX, &stats->pending_minutes)))
    {
      return false;
    }

  if (version == 1 && days == NULL)
    {
      stats->days[STUDY_FOCUS_DAYS - 1] = top;
      return valid_stats(stats);
    }

  if (!cJSON_IsArray(days) ||
      cJSON_GetArraySize(days) != STUDY_FOCUS_DAYS || goal == NULL)
    {
      return false;
    }

  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      if (!json_day(cJSON_GetArrayItem(days, i), &stats->days[i]))
        {
          return false;
        }
    }

  return valid_stats(stats) &&
         stats->days[STUDY_FOCUS_DAYS - 1].day == top.day &&
         stats->days[STUDY_FOCUS_DAYS - 1].rounds == top.rounds &&
         stats->days[STUDY_FOCUS_DAYS - 1].minutes == top.minutes;
}

static bool bounded_json_depth(const char *json, size_t size)
{
  unsigned depth = 0;
  bool quoted = false;
  bool escaped = false;

  /* cJSON is recursive. Reject excessive nesting before it uses the UI stack;
   * cJSON still performs all syntax, schema, and number parsing below.
   */

  for (size_t i = 0; i < size; i++)
    {
      char ch = json[i];

      if (ch == '\0')
        {
          return false;
        }

      if (quoted)
        {
          if (escaped)
            {
              escaped = false;
            }
          else if (ch == '\\')
            {
              escaped = true;
            }
          else if (ch == '"')
            {
              quoted = false;
            }
        }
      else if (ch == '"')
        {
          quoted = true;
        }
      else if (ch == '{' || ch == '[')
        {
          if (++depth > 4)
            {
              return false;
            }
        }
      else if (ch == '}' || ch == ']')
        {
          if (depth == 0)
            {
              return false;
            }

          depth--;
        }
    }

  return depth == 0 && !quoted;
}

int study_focus_stats_load(study_focus_stats_t *stats, const char *path,
                          int day)
{
  study_focus_stats_t loaded;
  FILE *file;
  char *buffer;
  cJSON *root = NULL;
  size_t length;
  int result = 0;

  study_focus_stats_init(stats, day);
  if (stats == NULL || path == NULL || path[0] == '\0' ||
      (day != 0 && !valid_day(day)))
    {
      return -EINVAL;
    }

  file = fopen(path, "rb");
  if (file == NULL)
    {
      return errno != 0 ? -errno : -EIO;
    }

  buffer = malloc(FOCUS_JSON_LIMIT + 1);
  if (buffer == NULL)
    {
      fclose(file);
      return -ENOMEM;
    }

  length = fread(buffer, 1, FOCUS_JSON_LIMIT, file);
  if (ferror(file))
    {
      result = -EIO;
    }
  else if (length == FOCUS_JSON_LIMIT && fgetc(file) != EOF)
    {
      result = -EFBIG;
    }

  if (ferror(file) && result == 0)
    {
      result = -EIO;
    }

  if (fclose(file) != 0 && result == 0)
    {
      result = -EIO;
    }

  buffer[length] = '\0';
  if (result == 0)
    {
      if (length == 0 || !bounded_json_depth(buffer, length))
        {
          result = -EINVAL;
        }
      else
        {
          root = cJSON_ParseWithOpts(buffer, NULL, 1);
          if (root == NULL || !parse_stats(root, &loaded))
            {
              result = -EINVAL;
            }
          else
            {
              study_focus_stats_set_day(&loaded, day);
              *stats = loaded;
            }
        }
    }

  cJSON_Delete(root);
  free(buffer);
  return result;
}

static bool add_day(cJSON *object, const study_focus_day_t *day)
{
  return cJSON_AddNumberToObject(object, "day", day->day) != NULL &&
         cJSON_AddNumberToObject(object, "rounds", day->rounds) != NULL &&
         cJSON_AddNumberToObject(object, "minutes", day->minutes) != NULL;
}

static char *print_stats(const study_focus_stats_t *stats)
{
  cJSON *root = cJSON_CreateObject();
  cJSON *days;
  char *text = NULL;

  if (root == NULL ||
      cJSON_AddNumberToObject(root, "version", FOCUS_VERSION) == NULL ||
      !add_day(root, &stats->days[STUDY_FOCUS_DAYS - 1]) ||
      cJSON_AddNumberToObject(root, "preset", stats->preset) == NULL ||
      cJSON_AddNumberToObject(root, "goal_minutes", stats->goal_minutes) == NULL ||
      cJSON_AddNumberToObject(root, "pending_rounds",
                              stats->pending_rounds) == NULL ||
      cJSON_AddNumberToObject(root, "pending_minutes",
                              stats->pending_minutes) == NULL)
    {
      goto out;
    }

  days = cJSON_AddArrayToObject(root, "days");
  if (days == NULL)
    {
      goto out;
    }

  for (int i = 0; i < STUDY_FOCUS_DAYS; i++)
    {
      cJSON *entry = cJSON_CreateObject();

      if (entry == NULL || !add_day(entry, &stats->days[i]) ||
          !cJSON_AddItemToArray(days, entry))
        {
          cJSON_Delete(entry);
          goto out;
        }
    }

  text = cJSON_PrintUnformatted(root);
out:
  cJSON_Delete(root);
  return text;
}

int study_focus_stats_save(const study_focus_stats_t *stats, const char *path)
{
  char *text;
  char *temporary;
  FILE *file;
  size_t length;
  int result = 0;

  if (!valid_stats(stats) || path == NULL || path[0] == '\0')
    {
      return -EINVAL;
    }

  length = strlen(path);
  if (length > FOCUS_JSON_LIMIT)
    {
      return -ENAMETOOLONG;
    }

  temporary = malloc(length + sizeof(".tmp"));
  if (temporary == NULL)
    {
      return -ENOMEM;
    }

  memcpy(temporary, path, length);
  memcpy(temporary + length, ".tmp", sizeof(".tmp"));
  text = print_stats(stats);
  if (text == NULL)
    {
      free(temporary);
      return -ENOMEM;
    }

  file = fopen(temporary, "wb");
  if (file == NULL)
    {
      result = errno != 0 ? -errno : -EIO;
      goto out;
    }

  length = strlen(text);
  if (fwrite(text, 1, length, file) != length || fputc('\n', file) == EOF)
    {
      result = -EIO;
    }

  if (fflush(file) != 0 && result == 0)
    {
      result = -EIO;
    }

  if (fclose(file) != 0 && result == 0)
    {
      result = -EIO;
    }

  if (result == 0 && rename(temporary, path) != 0)
    {
      result = errno != 0 ? -errno : -EIO;
    }

  if (result != 0)
    {
      remove(temporary);
    }

out:
  cJSON_free(text);
  free(temporary);
  return result;
}
