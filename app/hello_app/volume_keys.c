#include <nuttx/config.h>
#include <nuttx/input/buttons.h>

#include "volume_keys.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define VOLUME_KEYS_DEVICE "/dev/input/event1"
#define VOLUME_STATE_PATH "/data/ai_agent/STUDY_VOLUME.json"
#define VOLUME_STATE_TEMP_PATH "/data/ai_agent/STUDY_VOLUME.json.tmp"
#define VOLUME_DEFAULT 85
#define VOLUME_STEP 10
#define VOLUME_MAX 100

struct volume_keys_state_s
{
  int fd;
  btn_buttonset_t previous;
  btn_buttonset_t up_mask;
  btn_buttonset_t down_mask;
  unsigned int volume;
  lv_obj_t *label;
};

static struct volume_keys_state_s g_volume_keys =
{
  .fd = -1,
  .volume = VOLUME_DEFAULT,
};

static bool volume_state_number(const char *text, const char *key,
                                unsigned int *value)
{
  const char *cursor = strstr(text, key);
  char *end;
  unsigned long parsed;

  if (cursor == NULL)
    {
      return false;
    }

  cursor = strchr(cursor, ':');
  if (cursor == NULL)
    {
      return false;
    }

  errno = 0;
  parsed = strtoul(cursor + 1, &end, 10);
  if (errno != 0 || end == cursor + 1 || parsed > UINT32_MAX)
    {
      return false;
    }

  *value = (unsigned int)parsed;
  return true;
}

static void volume_state_load(void)
{
  FILE *stream;
  char buffer[160];
  size_t count;
  unsigned int value;

  stream = fopen(VOLUME_STATE_PATH, "r");
  if (stream == NULL)
    {
      return;
    }

  count = fread(buffer, 1, sizeof(buffer) - 1, stream);
  fclose(stream);
  buffer[count] = '\0';

  if (volume_state_number(buffer, "\"volume\"", &value) &&
      value <= VOLUME_MAX)
    {
      g_volume_keys.volume = value;
    }

  if (volume_state_number(buffer, "\"volume_up_mask\"", &value) &&
      value != 0 && value <= UINT8_MAX)
    {
      g_volume_keys.up_mask = (btn_buttonset_t)value;
    }

  if (volume_state_number(buffer, "\"volume_down_mask\"", &value) &&
      value != 0 && value <= UINT8_MAX)
    {
      g_volume_keys.down_mask = (btn_buttonset_t)value;
    }

  if (g_volume_keys.up_mask == g_volume_keys.down_mask)
    {
      g_volume_keys.up_mask = 0;
      g_volume_keys.down_mask = 0;
    }
}

static void volume_state_save(void)
{
  char buffer[160];
  int fd;
  int length;
  ssize_t written;

  (void)mkdir("/data/ai_agent", 0700);
  length = snprintf(buffer, sizeof(buffer),
                    "{\"version\":1,\"volume\":%u,\"volume_up_mask\":%u,"
                    "\"volume_down_mask\":%u}\n",
                    g_volume_keys.volume,
                    (unsigned int)g_volume_keys.up_mask,
                    (unsigned int)g_volume_keys.down_mask);
  if (length < 0 || (size_t)length >= sizeof(buffer))
    {
      return;
    }

  fd = open(VOLUME_STATE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      printf("study_terminal: cannot save volume: %d\n", errno);
      return;
    }

  written = write(fd, buffer, (size_t)length);
  if (written == length && fsync(fd) == 0 && close(fd) == 0)
    {
      fd = -1;
      if (rename(VOLUME_STATE_TEMP_PATH, VOLUME_STATE_PATH) == 0)
        {
          return;
        }
    }

  if (written != length)
    {
      printf("study_terminal: cannot write volume: %d\n", errno);
    }

  if (fd >= 0)
    {
      close(fd);
    }
  unlink(VOLUME_STATE_TEMP_PATH);
}

static void volume_label_set(const char *text)
{
  if (g_volume_keys.label != NULL)
    {
      lv_label_set_text(g_volume_keys.label, text);
    }
}

static void volume_label_update(void)
{
  char text[32];

  if (g_volume_keys.up_mask == 0)
    {
      volume_label_set("Press volume up");
      return;
    }

  if (g_volume_keys.down_mask == 0)
    {
      volume_label_set("Press volume down");
      return;
    }

  snprintf(text, sizeof(text), "Volume %u%%", g_volume_keys.volume);
  volume_label_set(text);
}

static void volume_keys_handle_press(btn_buttonset_t mask)
{
  if (g_volume_keys.up_mask == 0)
    {
      g_volume_keys.up_mask = mask;
      printf("study_terminal: volume-up key learned mask=0x%lx\n",
             (unsigned long)mask);
      volume_label_update();
      return;
    }

  if (g_volume_keys.down_mask == 0 && mask != g_volume_keys.up_mask)
    {
      g_volume_keys.down_mask = mask;
      volume_state_save();
      printf("study_terminal: volume-down key learned mask=0x%lx\n",
             (unsigned long)mask);
      volume_label_update();
      return;
    }

  if (mask == g_volume_keys.up_mask && g_volume_keys.volume < VOLUME_MAX)
    {
      g_volume_keys.volume += VOLUME_STEP;
      if (g_volume_keys.volume > VOLUME_MAX)
        {
          g_volume_keys.volume = VOLUME_MAX;
        }
    }
  else if (mask == g_volume_keys.down_mask && g_volume_keys.volume >= VOLUME_STEP)
    {
      g_volume_keys.volume -= VOLUME_STEP;
    }
  else if (mask == g_volume_keys.down_mask)
    {
      g_volume_keys.volume = 0;
    }
  else
    {
      return;
    }

  volume_state_save();
  printf("study_terminal: volume=%u mask=0x%lx\n", g_volume_keys.volume,
         (unsigned long)mask);
  volume_label_update();
}

static void volume_keys_timer(lv_timer_t *timer)
{
  btn_buttonset_t current;
  btn_buttonset_t pressed;
  ssize_t bytes;

  (void)timer;
  bytes = read(g_volume_keys.fd, &current, sizeof(current));
  if (bytes != sizeof(current))
    {
      return;
    }

  pressed = current & ~g_volume_keys.previous;
  g_volume_keys.previous = current;

  while (pressed != 0)
    {
      btn_buttonset_t mask = pressed & (btn_buttonset_t)(-pressed);
      volume_keys_handle_press(mask);
      pressed &= ~mask;
    }
}

int volume_keys_init(lv_obj_t *screen, const lv_font_t *font)
{
  if (screen == NULL)
    {
      return -1;
    }

  volume_state_load();
  g_volume_keys.fd = open(VOLUME_KEYS_DEVICE, O_RDONLY);
  if (g_volume_keys.fd < 0)
    {
      printf("study_terminal: cannot open volume keys: %d\n", errno);
      return -1;
    }

  g_volume_keys.label = lv_label_create(screen);
  lv_obj_set_style_text_font(g_volume_keys.label, font, 0);
  lv_obj_set_style_text_color(g_volume_keys.label, lv_color_hex(0x71809e), 0);
  lv_obj_align(g_volume_keys.label, LV_ALIGN_TOP_RIGHT, -12, 10);
  volume_label_update();
  lv_timer_create(volume_keys_timer, 80, NULL);
  printf("study_terminal: volume keys ready on %s\n", VOLUME_KEYS_DEVICE);
  return 0;
}
