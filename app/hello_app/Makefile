############################################################################
# Contest 2026 team 120 study terminal - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = study_terminal
PRIORITY  = $(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL_PRIORITY)
STACKSIZE = $(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL_STACKSIZE)
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL)

# Value-only public contracts; no dependency on private agent headers.
CFLAGS += -I$(APPDIR)/../packages/ai_agent/include

MAINSRC = study_terminal_main.c
CSRCS  = wifi_setup.c
CSRCS += task_presets.c
CSRCS += study_focus_stats.c
CSRCS += voice_ui_bridge.c
CSRCS += ai_chat_bridge.c
# Baked 16px font used by the Windows preview; keeps board text pixel-identical.
CSRCS += lv_font_study_16.c

# ui_status_bridge.c is deliberately not built: the UI reads sysinfo(),
# uname() and getifaddrs() directly in update_system_widgets(). The file is
# kept on disk as the standalone reference implementation.

include $(APPDIR)/Application.mk
