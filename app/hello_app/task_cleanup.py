#!/usr/bin/env python3
"""Add a way to clear finished tasks in one go.

With the four-row cap gone the list holds up to twenty tasks, and ticked-off
ones stay in it forever -- the only way to remove them was the per-row delete
button, twenty taps for twenty tasks, each one shifting the rows under the
finger.  The completion counters also keep counting them, so a week of use
leaves the ratio pinned near 100% and useless as a signal.

The button reports what it did rather than silently doing nothing when there is
nothing to clear: a control that appears to fail is worse than one that explains
itself.
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


# --- 1. the handler, next to the preset one ------------------------------
sub(
    "static void task_preset_event_cb(lv_event_t *event)\n",
    """/* Drop every completed task in one pass.  Follows the same load, compact,
 * save shape as task_delete_event_cb so both take the same path through
 * save_tasks() and the same mtime-gated rebuild.
 */

static void task_clear_done_event_cb(lv_event_t *event)
{
  struct study_task_s tasks[MAX_TASKS];
  size_t count;
  size_t index;
  size_t write_index = 0;

  (void)event;
  note_user_activity();

  count = load_tasks(tasks, MAX_TASKS);

  for (index = 0; index < count; index++)
    {
      if (tasks[index].completed)
        {
          continue;
        }

      if (write_index != index)
        {
          tasks[write_index] = tasks[index];
        }

      write_index++;
    }

  if (write_index == count)
    {
      /* Say so instead of appearing to fail. */

      if (g_ui.task_file_status != NULL)
        {
          lv_label_set_text(g_ui.task_file_status, "没有已完成的任务");
        }

      return;
    }

  if (!save_tasks(tasks, write_index))
    {
      if (g_ui.task_file_status != NULL)
        {
          lv_label_set_text(g_ui.task_file_status, "清理失败");
        }

      return;
    }

  /* Rebuild first: update_task_widgets() rewrites this same label with the
   * task count, so setting the message afterwards is what makes it visible.
   */

  g_task_reload_pending = true;
  update_task_widgets();

  if (g_ui.task_file_status != NULL)
    {
      lv_label_set_text_fmt(g_ui.task_file_status, "已清理 %d 项",
                            (int)(count - write_index));
    }
}

static void task_preset_event_cb(lv_event_t *event)
""",
    label="clear-done handler",
)

# --- 2. the button, beside 同步 ------------------------------------------
sub(
    """  g_ui.task_sync_label = create_action_button(button_row, COLOR_BLUE,
                                              "同步",
                                              task_sync_event_cb);
  if (g_compact_layout)
    {
      lv_obj_set_height(lv_obj_get_parent(g_ui.task_sync_label), 28);
    }""",
    """  {
    /* Clearing finished tasks was twenty taps on the per-row delete button,
     * each one shifting the rows under the finger.
     */

    lv_obj_t *clear_label = create_action_button(button_row, COLOR_MUTED,
                                                g_compact_layout ? "清理" :
                                                "清理已完成",
                                                task_clear_done_event_cb);

    if (g_compact_layout)
      {
        lv_obj_set_height(lv_obj_get_parent(clear_label), 28);
      }
  }

  g_ui.task_sync_label = create_action_button(button_row, COLOR_BLUE,
                                              "同步",
                                              task_sync_event_cb);
  if (g_compact_layout)
    {
      lv_obj_set_height(lv_obj_get_parent(g_ui.task_sync_label), 28);
    }""",
    label="clear-done button",
)

# --- 3. make room for it -------------------------------------------------
# The row is SPACE_BETWEEN, so a status label claiming 360 px pushes the two
# buttons off the right edge.
sub(
    "  lv_obj_set_width(g_ui.task_file_status, g_compact_layout ? 280 : 360);\n",
    "  lv_obj_set_width(g_ui.task_file_status, g_compact_layout ? 168 : 228);\n",
    label="narrow status label",
)

with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(src)

print("OK: clear-completed handler and button added")
