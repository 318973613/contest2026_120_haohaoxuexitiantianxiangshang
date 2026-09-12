/****************************************************************************
 * study_focus_stats.h - Persistent, calendar-based focus history
 ****************************************************************************/

#ifndef __STUDY_FOCUS_STATS_H
#define __STUDY_FOCUS_STATS_H

#include <stdbool.h>
#include <stdint.h>

#define STUDY_FOCUS_DAYS 7

typedef struct
{
  int day;
  uint32_t rounds;
  uint32_t minutes;
} study_focus_day_t;

typedef struct
{
  uint8_t preset;
  uint16_t goal_minutes;
  uint32_t pending_rounds;
  uint32_t pending_minutes;
  study_focus_day_t days[STUDY_FOCUS_DAYS];
} study_focus_stats_t;

/* All calls belong to the UI thread. Dates are YYYYMMDD, or zero before the
 * clock is synchronized. days[6] is the latest accepted day; older clock
 * readings never move the window backwards. An undated initial tally is
 * credited once when the first valid date arrives. With dated history,
 * unsynchronized/rolled-back rounds stay in pending until a date at least as
 * new as days[6] arrives. set_day may therefore change a same-day tally.
 */

void study_focus_stats_init(study_focus_stats_t *stats, int day);
bool study_focus_stats_set_day(study_focus_stats_t *stats, int day);

/* Call once for each completed focus round, not for refreshes or breaks.
 * Returns false without changing stats on invalid dates, zero minutes, or
 * counter overflow. There is no session ID deduplication.
 */

bool study_focus_stats_record_round(study_focus_stats_t *stats, int day,
                                    uint32_t minutes);

/* Consecutive active days within the retained window, starting yesterday
 * when today is empty. Undated and pending rounds do not create a streak.
 */

unsigned study_focus_stats_streak(const study_focus_stats_t *stats);

/* Return zero on success or a negative errno value. A failed load leaves the
 * initialized defaults for day. Saving accepts presets 0/1/2 and goals
 * 30/60/90 only, and replaces path only after the temporary file is closed.
 * The caller creates the parent directory, if necessary.
 */

int study_focus_stats_load(study_focus_stats_t *stats, const char *path,
                          int day);
int study_focus_stats_save(const study_focus_stats_t *stats, const char *path);

#endif /* __STUDY_FOCUS_STATS_H */
