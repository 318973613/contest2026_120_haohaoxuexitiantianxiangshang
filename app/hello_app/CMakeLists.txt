# Contest 2026 team 120 study terminal
if(CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL)
  set(STUDY_TERMINAL_CJK_FONT
      ${NUTTX_DIR}/../apps/graphics/lvgl/lvgl/src/font/lv_font_simsun_16_cjk.c)
  set_source_files_properties(${STUDY_TERMINAL_CJK_FONT}
                              PROPERTIES COMPILE_DEFINITIONS
                              LV_FONT_SIMSUN_16_CJK=1)
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
    ${STUDY_TERMINAL_CJK_FONT})
endif()
