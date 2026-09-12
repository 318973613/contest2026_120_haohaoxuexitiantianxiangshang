# Contest 2026 team 120 study terminal
if(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL)
  include_directories(${NUTTX_DIR}/../packages/ai_agent/include)
  set(STUDY_TERMINAL_CJK_FONT
      ${NUTTX_DIR}/../apps/graphics/lvgl/lvgl/src/font/lv_font_simsun_16_cjk.c)
  set_source_files_properties(${STUDY_TERMINAL_CJK_FONT}
                              PROPERTIES COMPILE_DEFINITIONS
                              LV_FONT_SIMSUN_16_CJK=1)
  # Keep this source list in sync with Makefile's MAINSRC/CSRCS.
  # volume_keys.c is deliberately excluded: the physical volume-button
  # feature is cancelled.
  # ui_status_bridge.c is also excluded: the UI reads sysinfo(), uname() and
  # getifaddrs() directly in update_system_widgets().
  nuttx_add_application(
    NAME
    study_terminal
    PRIORITY
    ${CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL_PRIORITY}
    STACKSIZE
    ${CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL_STACKSIZE}
    MODULE
    ${CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL}
    DEPENDS
    lvgl
    SRCS
    study_terminal_main.c
    wifi_setup.c
    task_presets.c
    study_focus_stats.c
    voice_ui_bridge.c
    ai_chat_bridge.c
    lv_font_study_16.c
    ${STUDY_TERMINAL_CJK_FONT})
endif()
