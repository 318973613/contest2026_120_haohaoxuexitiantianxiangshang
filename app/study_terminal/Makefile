# Makefile for Study Terminal UI
# Contest 2026 Team 120

include $(APPDIR)/Make.defs

PROGNAME = study_terminal
PRIORITY = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_DEFAULT_TASK_STACKSIZE)
MODULE = $(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL)

CSRCS = study_terminal_main.c
CSRCS += task_presets.c
CSRCS += ui_status_bridge.c
CSRCS += voice_ui_bridge.c
CSRCS += wifi_setup.c

MAINSRC = study_terminal_main.c

CFLAGS += -I$(APPDIR)/graphics/lvgl
CFLAGS += -I$(APPDIR)/system/cjson

include $(APPDIR)/Application.mk
