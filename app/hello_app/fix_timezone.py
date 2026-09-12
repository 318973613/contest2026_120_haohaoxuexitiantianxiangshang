#!/usr/bin/env python3
"""Put the clock in the right timezone.

ntpc sets the system clock to correct UTC, but nothing told libc which zone to
render it in: CONFIG_LIBC_TZDIR points at /etc/zoneinfo and no zoneinfo files
ship in the image, so localtime_r() falls back to UTC and every displayed time
would read 8 hours behind Beijing. A clock that is confidently wrong is worse
than one showing --:--.

The POSIX "CST-8" form is used rather than "Asia/Shanghai" precisely because it
needs no tzdata file -- the offset is encoded in the string. Sign is inverted in
that syntax: -8 means 8 hours *east* of UTC. No DST rule follows it because
China has not observed DST since 1991.

Set in main() before any timer starts, so the first tick already renders local
time, and via setenv+tzset so it does not depend on the launcher's environment.
"""
import io
import sys

PATH = "study_terminal_main.c"

with io.open(PATH, encoding="utf-8") as f:
    src = f.read()


def sub(old, new, want=1, label=""):
    global src
    n = src.count(old)
    if n != want:
        sys.exit("ABORT: %d occurrences (want %d) of %s" % (n, want, label or old[:60]))
    src = src.replace(old, new)


# --- 1. the timezone helper ---------------------------------------------
sub(
    """static bool clock_is_synced(time_t wall_time)
{""",
    """#ifndef _WIN32

/* Render times in China Standard Time.
 *
 * ntpc gives us correct UTC, but libc needs to be told the zone or it renders
 * UTC and the clock reads 8 hours slow.  The POSIX offset form is used because
 * "Asia/Shanghai" would need a tzdata file under CONFIG_LIBC_TZDIR
 * (/etc/zoneinfo), and none ship in this image.
 *
 * The sign is inverted by that syntax: -8 means 8 hours east of UTC.  No DST
 * rule follows because China dropped DST in 1991.
 */

static void apply_local_timezone(void)
{
  setenv("TZ", "CST-8", 1);
  tzset();
}
#endif

static bool clock_is_synced(time_t wall_time)
{""",
    label="timezone helper",
)

# --- 2. apply it before anything reads the clock -------------------------
# The libuv block above it is unique to the board's main(); the Win32 entry
# (study_terminal_windows_start) has its own g_initial_tab = 0 and must not get
# this call, since apply_local_timezone() is board-only.
sub(
    """  uv_loop_t ui_loop;
  memset(&ui_loop, 0, sizeof(ui_loop));
#endif

  g_initial_tab = 0;""",
    """  uv_loop_t ui_loop;
  memset(&ui_loop, 0, sizeof(ui_loop));
#endif

  /* Before the first timer tick, so the header clock and standby face never
   * render a UTC time.
   */

  apply_local_timezone();

  g_initial_tab = 0;""",
    label="apply timezone in main",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: timezone set to CST-8 at startup")
