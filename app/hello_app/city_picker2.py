#!/usr/bin/env python3
"""The city picker itself: table, config writer, and the settings row.

Read-modify-write with cJSON rather than printing a fresh JSON object, because
that file also holds the LLM endpoint, the WiFi credentials and the API keys --
overwriting it with one key would wipe the device's whole configuration.

Cross-process writing is safe here: claw_config_get/set re-read the file on
every call and keep nothing in memory, so the agent picks up a change on its
next fetch without a restart. The mutex inside config_store only orders calls
within the agent's own threads.
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


# --- 1. cJSON include ----------------------------------------------------
sub(
    '#  include "wifi_setup.h"\n',
    '#  include "wifi_setup.h"\n'
    '#  include <cJSON.h>\n',
    label="cJSON include",
)

# --- 2. city table + config read/write -----------------------------------
sub(
    """static bool clock_is_synced(time_t wall_time)
{""",
    """#ifndef _WIN32

/* Cities offered by the on-screen picker.  A short list of the obvious ones
 * beats a keyboard: AMap takes a plain name, and anyone needing somewhere else
 * can still set weather.city from the console.
 */

static const char * const g_city_choices[] =
{
  "深圳", "广州", "北京", "上海", "杭州", "成都", "武汉", "西安"
};

#define CITY_CHOICE_COUNT \\
  (sizeof(g_city_choices) / sizeof(g_city_choices[0]))

/* Read one key out of the agent's config store.  Returns false when the file
 * or the key is missing, which is the normal state before first setup.
 */

static bool agent_config_read(const char *key, char *out, size_t out_size)
{
  FILE *file;
  long size;
  char *text;
  cJSON *root;
  cJSON *item;
  bool ok = false;

  file = fopen(AGENT_CONFIG_PATH, "r");
  if (file == NULL)
    {
      return false;
    }

  if (fseek(file, 0, SEEK_END) != 0)
    {
      fclose(file);
      return false;
    }

  size = ftell(file);

  /* 64 KiB ceiling: this file is a handful of short strings, and a bogus size
   * should not turn into a large allocation on a 128 MiB device.
   */

  if (size <= 0 || size > 65536 || fseek(file, 0, SEEK_SET) != 0)
    {
      fclose(file);
      return false;
    }

  text = malloc((size_t)size + 1);
  if (text == NULL)
    {
      fclose(file);
      return false;
    }

  size = (long)fread(text, 1, (size_t)size, file);
  fclose(file);
  text[size > 0 ? size : 0] = '\\0';

  root = cJSON_Parse(text);
  free(text);

  if (root == NULL)
    {
      return false;
    }

  item = cJSON_GetObjectItem(root, key);
  if (item != NULL && cJSON_IsString(item) && item->valuestring[0] != '\\0')
    {
      snprintf(out, out_size, "%s", item->valuestring);
      ok = true;
    }

  cJSON_Delete(root);
  return ok;
}

/* Write one key, preserving everything else.  The file also holds the LLM
 * endpoint, WiFi credentials and API keys, so this reads, edits and writes
 * back rather than emitting a fresh object -- the latter would erase the
 * device's configuration to save one string.
 */

static bool agent_config_write(const char *key, const char *value)
{
  FILE *file;
  long size;
  char *text = NULL;
  cJSON *root = NULL;
  char *rendered;
  char temp_path[96];
  bool ok = false;

  if (mkdir(AGENT_DATA_DIR, 0777) != 0 && errno != EEXIST)
    {
      return false;
    }

  if (mkdir(AGENT_CONFIG_DIR, 0777) != 0 && errno != EEXIST)
    {
      return false;
    }

  file = fopen(AGENT_CONFIG_PATH, "r");
  if (file != NULL)
    {
      if (fseek(file, 0, SEEK_END) == 0)
        {
          size = ftell(file);
          if (size > 0 && size <= 65536 && fseek(file, 0, SEEK_SET) == 0)
            {
              text = malloc((size_t)size + 1);
              if (text != NULL)
                {
                  size = (long)fread(text, 1, (size_t)size, file);
                  text[size > 0 ? size : 0] = '\\0';
                  root = cJSON_Parse(text);
                  free(text);
                }
            }
        }

      fclose(file);
    }

  if (root == NULL)
    {
      /* No file yet, or unparseable.  Starting fresh is correct for the first
       * case; for the second the alternative is refusing to save at all.
       */

      root = cJSON_CreateObject();
      if (root == NULL)
        {
          return false;
        }
    }

  cJSON_DeleteItemFromObject(root, key);
  if (cJSON_AddStringToObject(root, key, value) == NULL)
    {
      cJSON_Delete(root);
      return false;
    }

  rendered = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  if (rendered == NULL)
    {
      return false;
    }

  snprintf(temp_path, sizeof(temp_path), "%s.tmp", AGENT_CONFIG_PATH);

  file = fopen(temp_path, "w");
  if (file != NULL)
    {
      if (fputs(rendered, file) >= 0 && fflush(file) == 0)
        {
          ok = true;
        }

      fclose(file);

      /* tmp + rename so the agent never reads a half-written config. */

      if (ok && rename(temp_path, AGENT_CONFIG_PATH) != 0)
        {
          unlink(temp_path);
          ok = false;
        }
      else if (!ok)
        {
          unlink(temp_path);
        }
    }

  free(rendered);
  return ok;
}
#endif

static bool clock_is_synced(time_t wall_time)
{""",
    label="city table and config io",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: city table and config read/write added")
