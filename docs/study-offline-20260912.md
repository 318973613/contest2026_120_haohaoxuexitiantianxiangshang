# 离线学习工具候选：提醒中心、离线提示音、专注记录（2026-09-12）

> 本轮不依赖 MiMo 云端：提醒、提示音、专注统计全部在本地完成。MiMo Key 已过期，
> 本轮没有请求云端，也没有把鉴权失败当作模型慢。
> 真机验收仍以 [CURRENT_HARDWARE_STATUS.md](CURRENT_HARDWARE_STATUS.md) 为准。

## 本轮做了什么

在 9/11 连续语音对话候选之上，新增三个不依赖云端的模块，并修掉上一轮遗留的
snooze/通知/边界/内存分配问题。

### 提醒中心

主界面顶栏铃铛进入“学习工具”，含“提醒中心”和“专注记录”两个页签。

- 四种提醒内容：休息 / 喝水 / 远眺 / 学习；时长 10 秒至 1 小时。
- 三项快捷提醒；最多 16 条本地提醒；逐条取消；到期弹窗；5 分钟后再提醒。
- 主页沿用调度器的同一份快照显示最近提醒，**不建立第二套倒计时**。
- 公共接口在 `packages/ai_agent/include/infra/cron_snapshot.h`：
  - `cron_create_reminder()` 只异步受理，结果用 `cron_get_reminder_create_result()` 查询。
  - `cron_list_reminders()`、`cron_get_next_reminder()` 用 trylock，不等工具/文件 I/O。
  - `cron_set_ui_notifications(true)` 让本地纯提醒走独立通知队列，绕开聊天回复和 TTS；
    未启用时保留 headless outbound 行为。
  - `cron_poll_reminder_event()` 单消费者取通知，不误结束当前语音请求。
  - `CRON_KIND_AFTER` 新倒计时用 `CLOCK_MONOTONIC`，运行中不受 NTP 前后跳影响。
- 断电恢复的**明确限制**：有可信保存日期的未来提醒在校时后恢复；未校时期间创建、
  又没有可信日期的倒计时无法推断断电时间，重启时清除，不假装准确恢复。
- 失效的一次性提醒做容量回收，避免列表空白却占满 16 个槽位。

### 离线提示音

- `voice_ui_bridge_chime()` 非阻塞入队；**返回 0 只表示受理，不代表已经出声**。
- `voice_channel_play_notification_prompt()` 播放本地 360 ms 双音（600/800 Hz），
  有间隔与渐入渐出，**不访问 MiMo**；既有 nxplayer 收尾还会等约 250 ms。
- 原 150 ms 唤醒音不变；任务组 fd 和前台 PTT/ASR 所有权保护保留。
- 到期通知尊重主音量 0；活动语音页期间延后显示和响铃，不抢当前对话。
- 音量滑块试听也改成本地提示音。

### 专注记录

新增独立模块 `app/hello_app/study_focus_stats.c/.h`。

- 每日 30 / 60 / 90 分钟目标；七日统计图；窗口内连续学习天数。
- JSON v2，迁移旧 v1；临时文件写完关闭后 rename，失败保留旧文件。
- 未校时或时间回拨的轮次进入 pending，恢复有效日期后**只合并一次**，不丢历史。
- 主 UI 用 LVGL 单调 tick 的**实际经过时间**计专注，不再每次回调简单加一秒。
- 专注完成只记一次，随后 5 分钟休息；休息结束停下，由用户决定是否开始下一轮。
- 主页底部“专注记录”可进入记录页。

## 本轮修掉的遗留问题

交接文档列出 7 项“写完但未复测”的改动，本轮全部复测并修正：

1. “稍后提醒”保留原弹窗，等异步保存成功才关闭；失败恢复原弹窗重试入口。
2. 专注/休息通知在语音或其他弹窗期间进入 pending，较新的阶段覆盖旧阶段。
3. 休息到时边界点击先记住操作意图，避免意外开启下一轮。
4. 主页播放按钮与时长选择改成独立横排，修复遮挡；列表预留滚动条空间。
5. `cron_write_jobs()` 检查所有 cJSON 构造/字段/挂载分配失败。
6. AFTER 创建成功文本改成相对秒数（`tool_cron.c:264`）；文件大小上限统一为
   `AGENT_CRON_FILE_MAX_SIZE = (64 * 1024)`，定义在 `agent_config.h`，测试直接
   include 生产头，因此测试与生产**共用同一个上限**，不是两份魔数。
7. 构建、同步、配对和归档脚本适配本轮目录；私有配置改成逐字节保持不变。

### 本轮额外修正的一处

`tests/focus_stats_test.c` 末尾的汇总行原本写成 `puts("PASS focus stats: 8 scenario
groups")`。它以 `PASS ` 开头，会被 `archive_candidate.py` 当成第 9 个测试组，
使 focus 组的统计从 8 变成 9，归档断言必然失败。

改成 `puts("focus stats: 8 scenario groups passed")` —— **没有改动任何测试数**，
只是让汇总行不再伪装成测试结果。修完 focus.log 正好 8 行 `PASS `，与期望一致。

## 验证结果（真实执行）

全部为**主机检查**，不是真机验收。

| 模块 | 组数 | 结果 |
|---|---|---|
| voice（语音实际模块，ASan/UBSan） | 23 | 全过 |
| chat（UI 桥接） | 3 | 全过 |
| cron（提醒调度，ASan/UBSan） | 23 | 全过 |
| focus（专注统计，ASan/UBSan） | 8 | 全过 |
| api（LLM/ASR 请求构造） | 4 | 全过 |
| UI 480×320（真实 LVGL 渲染） | 10 | 全过 |
| UI 1280×800（真实 LVGL 渲染） | 10 | 全过 |
| **合计** | **81** | **0 失败、0 ASan/UBSan 命中** |

截图（主机 LVGL，测试数据，**不是真机截图**）在
`_work/study-offline-20260912/tests/renders/`，两种尺寸各 11 个场景。
逐项视觉复查结论：

- 主页播放按钮与“25 分钟”已改同一横排，**不重叠**。
- 提醒列表滚动条在最右侧，**不压取消按钮**（16 条满列表也成立）。
- 到期弹窗“5 分钟后再提醒 / 知道了”完整。
- 专注记录页：今日进度、三档目标、七日柱状图、连续天数四者数值自洽
  （09/11 = 25 分 + 09/12 = 15 分 = 近 7 天 40 分钟）。
- 语音页四种状态（聆听 / 等待 / 播报 / 长回复）完整；长回复被裁剪在对话区内，
  **不遮挡底部按钮**。

## 配对产物

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_study-offline_20260912_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,841,216 bytes / `74f98365e632b6d240a848738bab33234439b71f793cee0689760a3f9f3e1c33` |
| 配对 ELF | `archive/nuttx-20260912-study-offline-candidate.elf` |
| ELF 大小 / SHA-256 | 68,670,248 bytes / `4928276d4d7d80c999858927113544b0d36cb366143f04d5cb7ae5c472c6c5e0` |
| 内核 bin 大小 / SHA-256 | 11,855,284 bytes / `5510c0e2e9cf42501f1699fdb2125ab46c2a938198e3c0e2361c0b9aa821f645` |
| 镜像生成时间（UTC） | 2026-09-12T15:10:10.193099+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/study-offline-candidate_20260912_151027` |
| VM 同步前备份 | `/home/openvela/openvela/_local_backups/study-offline-20260912_150331` |
| 状态 | **已构建、已打包、已配对备份；未烧录、真机待验。** |

## 证据链

1. **两端源码一致**：候选 `source/` 204 个文件在 Windows 与 VM 上路径与内容全同；
   根目录脚本与 `tests/` 的 24 个共有文件内容全同。
2. **基线未被污染**：`capture_baseline.py` 核对继承的 202 个基线文件与正式工程
   逐字节一致，确认**正式工程没有被其他对话改动**。
3. **候选审计**：14 个改动（12 修改 + 2 新增），`unexpected_changes` 为空。
4. **同步**：14 个文件落盘后逐一比对候选与正式工程哈希，0 不一致；
   122 个对象/标记备份后移走，未删除 `libapps.a`。
5. **构建**：官方 `build.sh` make 路线（未传 `--cmake`），退出码 0，无 `error:`、
   无 undefined reference；6 个改动翻译单元全部重编。
6. **配对**：按板级 objcopy 参数（`-R .note.gnu.build-id -O binary`）从 ELF 导出的
   11,855,284 字节与 `nuttx.bin` **逐字节相等**；204 个源文件哈希核验通过；
   符号含 `study_terminal_main`、`ai_agent_main`，不含 `vapp_main` / `xiaozhi_gui_main`。
7. **打包**：Dragon `execute image.cfg SUCCESS` + `pack finish`。
   `pack_exit=1` 是已知的外层包装假失败，不以该返回码替代产物核验。
8. **载荷**：按对象路径从 `usrdata.fex` 还原启动脚本、三份开机音频、字体、壁纸、
   图标并逐文件比对通过；`study-terminal.sh` = `ce90db7b…`，全部行 < 64 字符。
   完整 userdata（18,165,760 bytes / `6c384305…`）与内核都在最终镜像内。
9. **凭据**：私有 `config.json` 本轮**逐字节不变**（`5e17fa90…`），
   `model_fields_changed` 为空，聊天模型仍是 `mimo-v2.5`。
10. **两端哈希一致**：Windows 与 VM 的镜像、ELF SHA256 相同。

## 文件边界（后续分仓 PR）

公共 `packages/ai_agent`：

- `include/agent_config.h`、`include/infra/cron_snapshot.h`
- `src/infra/cron_service.c/.h`、`src/tools/tool_cron.c`
- `src/voice/voice_channel.c/.h`

比赛专属仓 `contest2026_120_haohaoxuexitiantianxiangshang`：

- `app/hello_app/study_focus_stats.c/.h`（新增）
- `app/hello_app/study_terminal_main.c`、`voice_ui_bridge.c/.h`
- `app/hello_app/CMakeLists.txt`、`Makefile`

本轮**未改动** `nuttx/`、板级 `defconfig`、`study-terminal.sh`、启动脚本和私有配置。

## 保留的既有修复

TLS 任务组 fd 隔离、FT5X06 触摸队列、G2D fill 格式、FreeType CJK 回退、
`CONFIG_SYSTEM_POPEN_STACKSIZE=20480`、`nxplayer` stdin 入口 —— 全部保留。

## 未完成 / 待办

- **未烧录、未真机验证。** 真机需验：提醒到期弹窗与响铃、5 分钟后再提醒、
  专注计时与七日统计落盘、断电重启后提醒恢复行为。
- **联网功能未验**：MiMo Key 已过期，ASR / 聊天 / TTS 需用户在板端本地更新
  有效凭据后再验收。不要把鉴权失败当成模型响应慢。
- 未 commit、未 push。公共仓与专属仓改动要分开走各自的 PR 流程。
- 镜像含预置凭据（含已过期的 MiMo Key），**严禁外发**。
