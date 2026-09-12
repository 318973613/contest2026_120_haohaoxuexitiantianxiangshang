# 成功构建与产物记录

> 更新：2026-09-12。  
> 本表只记录已完成构建/打包的事实；“已构建”不等于“已烧录”或“真机通过”。每条当前产物必须有对应 ELF 备份。

## 最新候选：唤醒词合规统一（2026-09-12）

把 7 处「你好小米」改为官方要求的「你好，openvela / Hello，openvela」（5 处在公共仓
`packages/ai_agent`、2 处在专属仓），并加 ASCII 大小写与标点折叠、删除品牌唤醒词、
保留中性旧别名。83 组主机检查通过；**重编 4 个翻译单元**，make 退出 0。官方 Dragon
SUCCESS、pack finish 与新镜像时间戳成立；按板级 objcopy 参数从 ELF 导出的 11,855,284
字节完整内核与打包输入逐字节一致。内核内 `你好，openvela` 命中 **6** 处、
`你好小米` 命中 **0**。

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_wake-word_20260912_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,841,216 bytes / `e7ce47c3983bbe652dba431c52e1a419eafd9f5ec46c1c540618d093f42a52ea` |
| 配对 ELF | `archive/nuttx-20260912-wake-word-candidate.elf` |
| ELF 大小 / SHA-256 | 68,678,904 bytes / `4e08ff20e6f4efb690547b7c0f025314011a13164e645e1d911d172de0478881` |
| 内核 bin 大小 / SHA-256 | 11,855,284 bytes / `a0762889cf987aa3e22227bf5e75dacfbb83c2cb1e13246b0ad0a14ed73d6b0a` |
| 镜像生成时间（UTC） | 2026-09-12T16:00:57.213143+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/study-offline-candidate_20260912_160113` |
| 状态 | **已构建打包、已配对备份；未烧录、真机待验。** |

YAFFS 载荷核验通过：启动脚本、三份开机音频、字体、壁纸、图标逐文件哈希与上轮一致，
`packed_private_config_matches_seed: true`、`non_model_settings_unchanged: true`，
模型仍是 `mimo-v2.5`，`active.config` 哈希与上轮一致（无配置漂移）。userdata 镜像
18,165,760 bytes / `de1d8b2c…`，字节数与上轮相同、资产哈希全部一致，但镜像哈希与上轮
不同——userdata 由打包器重新生成，这里只记录事实，不宣称二者逐字节相同。
Windows/VM 双端 SHA 一致。外层 pack 仍返回已知的 1，不以该返回码替代产物核验。
详见 [完整记录](wake-word-20260912.md)。镜像含凭据，不可外发。

## 历史候选：学习工具页视觉统一 + 学习报告（2026-09-12）

把学习工具页拉回主页同一套视觉语言（烘焙壁纸 + 深色顶栏/tabbar + 白色半透明卡片），
新增第三个标签「学习报告」和「清除全部提醒」。83 组主机检查通过；**只重编 1 个翻译
单元**（`study_terminal_main.c`），make 退出 0。官方 Dragon SUCCESS、pack finish 与新
镜像时间戳成立；按板级 objcopy 参数从 ELF 导出的 11,855,284 字节完整内核与打包输入
逐字节一致。公共仓库 `nuttx/`、`packages/ai_agent/` 本轮零改动。

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_study-ui-consistency_20260912_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,841,216 bytes / `e2b97d1aedf564e85b044111276f0515ef52ba3997b9846686f2a167ccf2a85a` |
| 配对 ELF | `archive/nuttx-20260912-ui-consistency-candidate.elf` |
| ELF 大小 / SHA-256 | 68,678,880 bytes / `b20bba64006cd7d2ae3fbcb31f619c8f3583c55d16a40dcf1c12fd02d2422ae8` |
| 内核 bin 大小 / SHA-256 | 11,855,284 bytes / `539617c6a43d15f27388381f5877e4772b744f15d8c033fb46b8b90255a5029a` |
| 镜像生成时间（UTC） | 2026-09-12T15:35:03.046454+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/study-offline-candidate_20260912_153518` |
| 状态 | **已构建打包、已配对备份；未烧录、真机待验。** |

YAFFS 按对象路径还原启动脚本、三份开机音频、字体、壁纸和图标后逐文件比对；
完整 userdata（18,165,760 bytes / `6c384305…`）与内核均在最终镜像中，Windows/VM
SHA 一致。私有配置本轮**逐字节不变**（`5e17fa90…`），聊天模型仍是 `mimo-v2.5`。
外层 pack 仍返回已知的 1，不以该返回码替代产物核验。详见
[完整记录](ui-consistency-20260912.md)。镜像含凭据，不可外发。

## 历史候选：离线学习工具（2026-09-12）

提醒中心、离线提示音、专注记录三块不依赖 MiMo 云端的功能。81 组主机检查通过；
6 个改动翻译单元全部重编，make 退出 0。官方 Dragon SUCCESS、pack finish 与新镜像
时间戳成立；按板级 objcopy 参数从 ELF 导出的 11,855,284 字节完整内核与打包输入
逐字节一致。

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_study-offline_20260912_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,841,216 bytes / `74f98365e632b6d240a848738bab33234439b71f793cee0689760a3f9f3e1c33` |
| 配对 ELF | `archive/nuttx-20260912-study-offline-candidate.elf` |
| ELF 大小 / SHA-256 | 68,670,248 bytes / `4928276d4d7d80c999858927113544b0d36cb366143f04d5cb7ae5c472c6c5e0` |
| 内核 bin 大小 / SHA-256 | 11,855,284 bytes / `5510c0e2e9cf42501f1699fdb2125ab46c2a938198e3c0e2361c0b9aa821f645` |
| 镜像生成时间（UTC） | 2026-09-12T15:10:10.193099+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/study-offline-candidate_20260912_151027` |
| 状态 | **已构建打包、已配对备份；未烧录、真机待验。** |

YAFFS 按对象路径还原启动脚本、三份开机音频、字体、壁纸和图标后逐文件比对；
完整 userdata（18,165,760 bytes / `6c384305…`）与内核均在最终镜像中，Windows/VM
SHA 一致。私有配置本轮**逐字节不变**（`5e17fa90…`），聊天模型仍是 `mimo-v2.5`。
外层 pack 仍返回已知的 1，不以该返回码替代产物核验。详见
[完整记录](study-offline-20260912.md)。镜像含凭据，不可外发。

## 历史候选：连续语音对话与提醒联动（2026-09-11）

38 组主机检查通过；15 个改动 C 文件全部重编，make 退出 0。官方 Dragon SUCCESS、pack finish 与新镜像时间戳成立；按板级 objcopy 参数从 ELF 导出的 11,838,852 字节完整内核与打包输入一致。

9/12 用户告知 MiMo Key 已过期；本镜像仍沿用原 Key，38 组检查未调用云端服务。联网功能必须更新有效凭据后再验收，镜像本身不能解决密钥过期。

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_voice-dialogue-prekeyed-candidate_20260911_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,824,832 bytes / `af1655aee2f72607a67cb37601dfa74a5717dd5c0f458a0a7b39d3d9bfce54ce` |
| 配对 ELF | `archive/nuttx-20260911-voice-dialogue-candidate.elf` |
| ELF 大小 / SHA-256 | 68,584,104 bytes / `9bb192d148da9ba5cb36e5f40a3b193d0b7e3c360f5484256f9df848e8ce6a11` |
| 内核 bin 大小 / SHA-256 | 11,838,852 bytes / `a2b4f5583450643da5cfbfa577b9968ba0045efade98f15f98328b14f33347d3` |
| 镜像生成时间（UTC） | 2026-09-11T15:51:29.764816+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/voice-dialogue-candidate_20260911_155223` |
| 状态 | **已构建打包、已配对备份；未烧录、真机待验。** |

YAFFS 按对象路径还原启动脚本、三份开机音频和配置后逐文件比对；私有配置只切换两处模型，凭据与其余字段不变。完整 userdata 与内核均在最终镜像中，Windows/VM SHA 一致。已有第三方编译警告保留，详见 [完整记录](voice-dialogue-20260911.md)。外层 pack 仍返回已知的 1，不以该返回码替代产物核验。镜像含凭据，不可外发。

## 历史候选：AI 回复后触摸与语音修复（2026-09-06）

以下为当时交付记录，用户后来已烧录并报告基本可运行；当前继续使用 9/11 候选验证对话与提醒联动。

隔离不同 NuttX 任务组的 TLS 池 fd；协调唤醒/PTT/TTS 资源，保留既有已验证修复。六个源码文件、四个翻译单元已重新编译，主机复现与回归、官方打包、完整内核配对和 YAFFS 内容核验通过。本候选未烧录。

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_voice-replyfix-prekeyed-candidate_20260906_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,816,640 bytes / `541cec4fe763eaca0fdce9d9882c2db33ebc566f9f0b72ba28bf5c16a3a490b6` |
| 配对 ELF | `archive/nuttx-20260906-voice-replyfix-candidate.elf` |
| ELF 大小 / SHA-256 | 68,547,832 bytes / `aa91ddf65eb70cd7500ce04db25449964ba7386594f3151235428b167bce8f60` |
| 内核 bin 大小 / SHA-256 | 11,830,628 bytes / `65009226fa2f41ca30a8b6051201fd9ef798e15c6b030a0308c3e2e55f034a28` |
| 镜像生成时间（UTC） | 2026-09-06T05:37:48.007897+00:00 |
| VM 配对快照 | `/home/openvela/img_backups/voice-replyfix-candidate-20260906_20260906_053900` |
| 状态 | **已构建打包、已配对备份；未烧录、真机待验。** |

完整证据：[voice-reply-20260906.md](voice-reply-20260906.md)。9/5 候选已烧录但关键复验失败，应使用本条候选继续验证。

## 历史诊断产物

### 开机语音修复版 bootvoice2（2026-09-04，基于 popen 栈修复版）

| 项目 | 值 |
|---|---|
| 目的 | 修掉 8/28 bootvoice 版的两个症状（同一根因）：开机没声音，以及串口控制台被抢。根因是 `nxplayer` 是 stdin REPL——`apps/system/nxplayer/nxplayer_main.c:790` 的 `main()` 通篇不引用 argc/argv，所以 `nxplayer <path> &` 的路径被丢弃（永不播放），且进程挂在串口 stdin 上和 NSH 抢字符。改为 `nxplayer < /data/audio/bootvoice.cmd &`：新增命令文件喂 stdin，并新增去头 PCM `/data/audio/boot_intro.pcm`（按 WAV `data` chunk 真实偏移 44 提取，599,040 bytes，1ch/24000Hz/16bit）走 `playraw`——即 `audio_playback.c` 现场出过声的同一条命令。内核未变，仅重新 pack。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi-popenstack-bootvoice2_20260904_256Mnand.img` |
| 镜像大小 | 40,206,336 bytes |
| 镜像 SHA-256 | `3443b768dc15527e539fe1a737de9435639abb2858924857534c16734887d7fe` |
| 对应 ELF | `archive/nuttx-20260904-bootvoice2-verified-pair.elf` |
| ELF 大小 | 68,520,444 bytes |
| ELF SHA-256 | `fa0593cd6eb694426dd8dc0aa8fea7e9142d96c7f91a68bb96ef0bf6f47e9ba6` |
| 内核标识 | `nsh.fex` = `nuttx.bin`，SHA-256 `fd417f2fcaa85359ac04233e5f188062e36cbe69ae09606b073b20e471aa48a6`，与 popenstack 版同一内核。 |
| ELF 配对证据 | `cmp -n 11762444` 把该 ELF 的 `.text`（`objcopy --only-section=.text`）与 `nuttx.bin` 前 11,762,444 字节逐字节比对**通过**。此前归档的 `44bdeb56...` 文件级不同且未做此比对，解崩溃地址优先用这份。 |
| 打包证据 | `Dragon execute image.cfg SUCCESS !` + `pack finish`；yaffs 日志逐条列出 `audio/bootvoice.cmd`（1 chunk）、`audio/boot_intro.pcm`（295 chunks）、`audio/boot_intro.wav`（295 chunks）。`PACK_RC=1` 是已知的外层脚本假失败（见 `编译方法.md` §3）。 |
| 离线验证 | 在最终 `.img` 内双向核对，不只查新行在不在：新行 `nxplayer < /data/audio/bootvoice.cmd &` 命中 1 次；旧坏行 `nxplayer /data/audio/boot_intro.wav &` 命中 **0** 次；`playraw /data/audio/boot_intro.pcm 1 16 24000 0` 与 `!sleep 14` 各命中 1 次。启动脚本 43 行全部 <64 字符，无 CR 字节。 |
| 烧录状态 | **未烧录。** 烧录后验两件事：开机约 12 秒中文自我介绍有声；`vela>` 下打字不再被 `nxplayer>` 插断。 |

`bootvoice.cmd` 四行都是必需的：

```text
volume 100
playraw /data/audio/boot_intro.pcm 1 16 24000 0
!sleep 14
q
```

- `volume 100` → `nxplayer_cmd_volume` 做 `atof(parg)*10.0`，内部 1000（满音量）。
- `!sleep 14` 必需：`nxplayer_playraw` 注释写明 `OK = File is being played`，是异步的，紧跟 `q` 会立刻 `nxplayer_stop` 掐断播放。音频 12.48s。
- `q` 必需：主循环在 `len <= 0`（EOF）时没有 break，会无限打印 `nxplayer> ` 刷屏。

### 开机语音介绍版（2026-08-28，基于 popen 栈修复版）——**已被 bootvoice2 取代，不要再烧**

| 项目 | 值 |
|---|---|
| 目的 | 按用户要求用 MiMo `mimo-v2.5-tts` 预生成开机中文介绍语音：因接口单次只返回约 1.5 秒内音频，把介绍词拆成 6 个短句逐句合成后拼接为 12.48s / 24kHz / 单声道 / 16bit WAV，打入镜像 `/data/audio/boot_intro.wav`；`study-terminal.sh` 在 `mediad` 就绪后执行 `nxplayer /data/audio/boot_intro.wav &`，开机本地播放、不依赖联网。内核与 popen 栈修复版相同，仅重新 pack。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi-popenstack-bootvoice_20260828_256Mnand.img` |
| 镜像大小 | 39,596,032 bytes |
| 镜像 SHA-256 | `0b7352d9a1711bfe9d6802578d72e4eab672431bd9eaaa84fed0228e55348952` |
| 对应 ELF | `archive/nuttx-20260828-ui-dailyplan-coach-prekeyed-wifi-popenstack.elf`（与 popen 栈修复版同一内核） |
| ELF SHA-256 | `44bdeb56a28057c585197e5d7cd1b35f14ce6001bd5c58f82b8051c6c48f434a` |
| 打包证据 | 内核未重编，仅重新 `pack`；`Dragon execute image.cfg SUCCESS`；usrdata.fex 检出 `boot_intro.wav` 与 `nxplayer /data/audio/boot_intro.wav &`；无 `wifi_manager &`。 |
| 实际结果 | **已烧录，功能为零且造成回归。** 开机无声（`nxplayer` 忽略 argv，路径被丢弃），并且串口控制台不可用（nxplayer 抢 stdin）。当时的“离线验证”只 grep 了那行字符串在不在——字符串在，功能无。这是本项目最典型的一次无效验证。 |


### popen 栈溢出修复版（2026-08-28）

| 项目 | 值 |
|---|---|
| 目的 | 用户烧录 agentfix2 版后仍整机 popen 断言 panic。符号化三段 backtrace（popen 子进程、study_terminal 的 mallinfo、realtek_sdio_thread 的 calloc）确认崩溃点都是堆损坏受害者：真根因是 `CONFIG_SYSTEM_POPEN_STACKSIZE=2048`（2KB）。ai_agent 的 `popen("wapi status wlan0")` 会 spawn 一个 2KB 栈的完整 NSH 会话（`apps/system/popen/popen.c` → `task_spawn("popen", nsh_system)`），NSH 退出路径栈溢出写坏栈底相邻堆对象、污染堆链表，导致全机随机断言；日志 `nsh: wap▒▒AD▒B: command not found` 是溢出后解析缓冲区损坏的表现。修复：DShanPi `defconfig` 新增 `CONFIG_SYSTEM_POPEN_STACKSIZE=20480`（备份 `.bak-20260828-popenstack`），与 `CONFIG_SYSTEM_SYSTEM_STACKSIZE=20480` 对齐；`study-terminal.sh` 注释同步为真实根因。推翻阶段 15 的 fs_files 并发假设。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi-popenstack_20260828_256Mnand.img` |
| 镜像大小 | 38,985,728 bytes |
| 镜像 SHA-256 | `4c26bce01f85d14152345cb9723fd4e2e3a573263a2db1626213f1dc5322ec2d` |
| 对应 ELF | `archive/nuttx-20260828-ui-dailyplan-coach-prekeyed-wifi-popenstack.elf` |
| ELF 大小 | 68,516,936 bytes |
| ELF SHA-256 | `44bdeb56a28057c585197e5d7cd1b35f14ce6001bd5c58f82b8051c6c48f434a` |
| 构建证据 | 内核全量重建 `BUILD_RC=0`、无 `error:`、无 undefined reference、`Dragon execute image.cfg SUCCESS`；ELF 含 `study_terminal_main`，无 `vapp_main`/`xiaozhi_gui_main`；`.config` 确认 `CONFIG_SYSTEM_POPEN_STACKSIZE=20480`、`CONFIG_LV_COLOR_DEPTH=16`。 |
| 烧录状态 | **已烧录初验（2026-08-28）：整机不再 popen panic，WiFi 自动连 iQOO 并联网；麦克风“持续采集”仍失败，见 CURRENT_HARDWARE_STATUS。** |

### 今日计划/智能建议/演示就绪度 UI 版（2026-08-26）

| 项目 | 值 |
|---|---|
| 目的 | 合入 2026-08-18 在 Windows 完成但从未同步/构建的 UI 增强：任务页“今日计划”一键生成三条真实任务（任务满时提示清理）、AI 页“智能建议”把真实待办/完成/专注/下一项发给 MiMo 并回显最近回复、状态页“演示就绪度”与真实 `ai_agent` 状态；任务时间戳改用 `clock_is_synced()` 判断，避免未同步时钟把新任务打成 1970。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach_20260826_256Mnand.img` |
| 镜像大小 | 38,977,536 bytes |
| 镜像 SHA-256 | `20390d384294c29c7e6e722b666ac6ff3e4416845d030964f42b81e3027745db` |
| 对应 ELF | `archive/nuttx-20260826-ui-dailyplan-coach.elf` |
| ELF 大小 | 68,531,144 bytes |
| ELF SHA-256 | `5b57f69f8c2683a7c7a42caff677678d92290cdc7017f040e2293d747940f112` |
| 构建证据 | `BUILD_RC=0`、无 `error:`、无 undefined reference、`Dragon execute image.cfg SUCCESS`；ELF 含 `study_terminal_main`、`task_daily_plan_event_cb`、`ai_coach_event_cb`，无 `vapp_main`/`xiaozhi_gui_main`；固件离线检出“今日计划/智能建议/演示就绪度”字符串。 |
| 烧录状态 | **未烧录、未真机验证。** 新按钮与状态行在 480x320 布局需真机复核。 |

### 预置天气/MiMo/WiFi 凭据版（2026-08-26）

| 项目 | 值 |
|---|---|
| 目的 | 按用户明确要求，把用户提供的天气 Key、MiMo 主机/Key 和 WiFi（SSID/密码）预置进板级 `usrdata`（`/data/ai_agent/config/config.json` 与 `/data/etc/wifi/wapi.conf`），烧录后无需手动配置即可测试。**注意：此镜像含明文密钥，严禁外发；测试后建议在服务商控制台轮换密钥。** 本项目默认规则“密钥不进镜像”被本次显式用户指令覆盖一次。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed_20260826_256Mnand.img` |
| 镜像大小 | 38,985,728 bytes |
| 镜像 SHA-256 | `71b0ca08b2acff5b48f64e3e8cc443936acea3b8fafb861c83430c399c8b57d3` |
| 对应 ELF | `archive/nuttx-20260826-ui-dailyplan-coach.elf`（与 UI 版同一内核，未改动） |
| ELF SHA-256 | `5b57f69f8c2683a7c7a42caff677678d92290cdc7017f040e2293d747940f112` |
| 打包证据 | 内核未重编，仅重新 `pack`；`Dragon execute image.cfg SUCCESS`；镜像离线检出 `llm_backend_0`、`token-plan-cn.xiaomimimo.com`、`mimo-v2.5-pro`、`weather.key`、`iQOO`、`wapi.conf`，且三个凭据均存在（掩码核验，未打印明文）。 |
| 烧录状态 | **未烧录。** 烧录后应能直接测天气、MiMo 对话/TTS/ASR 和 WiFi 自动连接。 |

来源：种子目录 `vendor/allwinnertech/lichee/board/r528s3/dshanpi_nand/data/usrdata/`
下写入 `ai_agent/config/config.json` 与 `etc/wifi/wapi.conf`（原 wapi.conf 备份为
`wapi.conf.bak-<日期>`），VM 快照在
`~/img_backups/ui-dailyplan-coach-prekeyed-20260826_*/`。内核 ELF 与 UI 版相同。



### 启动脚本修复版 2（2026-08-27 第三次：ai_agent 自启 + 并发崩溃）

| 项目 | 值 |
|---|---|
| 目的 | 用户烧录 agentfix 版后回报：`ps` 无 `ai_agent`（手动 `ai_agent --no-cli` 可正常启动并自动连 iQOO），随后整机 popen 断言 panic。根因一：脚本 `ntpc start` 命令不存在（本构建只注册 `ntpcstart/ntpcstop/ntpcstatus`），NSH `sh` 遇命令不存在即中止脚本，`ai_agent` 永不启动；根因二：`wifi_manager` 循环 `system()`，与 ai_agent `network_wifi_connect()` 的 `system("ifup wlan0")`/`popen("wapi status")` 并发触发 `fs_files.c` popen 断言。修复：`study-terminal.sh` 移除 `wifi_manager &`（WiFi 改由 ai_agent netmgr 从预置 `config.json` 的 `wifi_ssid/wifi_pass` 自动连接），`ntpc start` 改 `ntpcstart`；内核与配置未改，仅重新 `pack`。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi-agentfix2_20260827_256Mnand.img` |
| 镜像大小 | 38,989,824 bytes |
| 镜像 SHA-256 | `97ffc08b935f6bc2b61c39f127bb51644bf16e30899763ff848c5fa1ded52b93` |
| 对应 ELF | `archive/nuttx-20260826-ui-dailyplan-coach.elf`（与 8/26 相同，未改动） |
| ELF SHA-256 | `5b57f69f8c2683a7c7a42caff677678d92290cdc7017f040e2293d747940f112` |
| 打包证据 | 内核未重编，仅重新 `pack`（先移走旧 `usrdata.fex`/`img` 避免 mkyaffs 旧数据残留）；`Dragon execute image.cfg SUCCESS`；离线检出新脚本含 `ntpcstart`、`ai_agent --no-cli`、`study_terminal &`、`mediad &`，无 `wifi_manager &`；`iQOO`、`token-plan-cn.xiaomimimo.com` 均在 usrdata 内。 |
| 烧录状态 | **未烧录。** 烧录后应看到：开机自动连 iQOO（ai_agent netmgr）、`ps` 含 `ai_agent`、UI 首页不再“AI 服务未运行”、无 popen panic。 |
### 预置凭据 + WiFi + 启动错峰修复版（2026-08-27 第二次）

| 项目 | 值 |
|---|---|
| 目的 | 用户烧录 wifi-fix 版后反馈“AI 服务未运行、麦克风启动失败”。UI 的“AI 服务未运行”= 在 /proc 找不到 ai_agent 进程。本次把启动脚本改为错峰：wifi_manager 先起，UI 先亮屏，mediad 后 sleep 5 再起 ntpc 和 ai_agent，降低启动期多服务同时 system()/popen() 的 fd 冲突（历史根因 fs_files.c:616）。未改内核与配置。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi-agentfix_20260827_256Mnand.img` |
| 镜像大小 | 38,989,824 bytes |
| 镜像 SHA-256 | `d592dd647cf770270a4a83c0582f4e63df6d9543ed449fe01a0a3a0d95c48d04` |
| 对应 ELF | `archive/nuttx-20260826-ui-dailyplan-coach.elf`（内核未改） |
| ELF SHA-256 | `5b57f69f8c2683a7c7a42caff677678d92290cdc7017f040e2293d747940f112` |
| 打包证据 | 内核未重编，仅重新 `pack`；`Dragon execute image.cfg SUCCESS`；镜像离线检出 `ai_agent --no-cli`、`wifi_manager &`、`SSID="iQOO"`。 |
| 烧录状态 | **未烧录。** 若仍“AI 服务未运行”，需抓串口日志（ai_agent 的 P0-P5 启动标记或崩溃寄存器）。 |

### 预置凭据 + WiFi 自动连接修复版（2026-08-27）

| 项目 | 值 |
|---|---|
| 目的 | 修复 2026-08-26 预置版“烧录后不自动连 WiFi”的问题。根因：上游 `xiaozhi.sh` 靠 `wifi_manager` 自动连接，而 `wifi_manager` 只读 `/data/wifi.cfg`（`SSID=`/`PASSWORD=` 格式），不读 `wapi.conf`；原生 LVGL 启动脚本丢掉了 `wifi_manager &`，镜像里也只有 `wapi.conf`。本版补上 `/data/wifi.cfg`，并在 `study-terminal.sh` 开头加回 `wifi_manager &`（与小智示例一致），同时保留 `wapi.conf` 供 UI 配网页使用。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_ui-dailyplan-coach-prekeyed-wifi_20260827_256Mnand.img` |
| 镜像大小 | 38,989,824 bytes |
| 镜像 SHA-256 | `d5fdf37ff66d90e056ff19df403e2b9b08fae2a2aa90e6c64b204f13d041dbb9` |
| 对应 ELF | `archive/nuttx-20260826-ui-dailyplan-coach.elf`（内核未改，与 8/26 两版同一 ELF） |
| ELF SHA-256 | `5b57f69f8c2683a7c7a42caff677678d92290cdc7017f040e2293d747940f112` |
| 打包证据 | 内核未重编，仅重新 `pack`；`Dragon execute image.cfg SUCCESS`；镜像离线检出 `wifi_manager &`、`SSID="iQOO"`、`PASSWORD="`、`token-plan-cn.xiaomimimo.com`。 |
| 烧录状态 | **未烧录。** 烧录后应开机自动连 iQOO，再测天气 / MiMo 对话 / TTS。 |

**注意**：2026-08-26 预置版（`71b0ca08...`）只写了 `wapi.conf` 且启动脚本没有
`wifi_manager`，**不会自动连 WiFi**，已被本版取代；两版都含明文密钥，严禁外发。

### TLS SPKI 诊断版（2026-08-15）

| 项目 | 值 |
|---|---|
| 目的 | 在 MiMo TLS `-0x3b62` 的 `mbedtls_pk_parse_subpubkey()` 失败点输出 SPKI 字节，以区别内存破坏和解析问题。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_tls-spki-diag_20260815_256Mnand.img` |
| 镜像大小 | 38,977,536 bytes |
| 镜像 SHA-256 | `df8a9055233e5bbc726d91ec7888a8e800336f2a2e0b1b949a5264e40697395e` |
| 对应 ELF | `archive/nuttx-20260815-tls-spki-diag.elf` |
| ELF 大小 | 68,525,304 bytes |
| ELF SHA-256 | `56155a4c212e7d3854e4c3a9b4e510c996250dc111b632c3aab6135090434537` |
| 构建结论 | 新 `nuttx.bin` 与 `vela_nsh.elf` 生成；构建日志无 `error:`、无 undefined reference；Dragon 输出 `SUCCESS`。 |
| 烧录状态 | **未作为 TLS 诊断结果验收。** 必须先配置/确认 MiMo 再抓 `[x509diag]`。 |

诊断预期的正常 RSA SPKI 起始：

```text
30 82 .. .. 30 0d 06 09 2a 86 48 86 f7 0d 01 01 01
```

不要把本镜像称为“TLS 已修复”。

### 城市点选、时区、音量写回修订版（2026-08-14）

| 项目 | 值 |
|---|---|
| 目的 | 加入天气城市点选、`CST-8` 时区初始化，并将音量持久化与松手试听分离。 |
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_citypicker-tz-volfix_20260814_256Mnand.img` |
| 镜像大小 | 38,973,440 bytes |
| 镜像 SHA-256 | `5b10562bdd70515069fa3975738bfd78ee7775ba7ba4ef2fecd699bb4af448f5` |
| 对应 ELF | `archive/nuttx-20260814-citypicker-tz-volfix.elf` |
| ELF 大小 | 68,522,124 bytes |
| ELF SHA-256 | `8cdffbab9185e14ac8bd63c5573068c61bd477f1bd097beb7b117d2fd58e9a3f` |
| 验收状态 | 构建/打包/备份完成；城市点选、时区、音量不回弹仍需逐项真机验证。 |

### 功能聚合版（2026-08-13）

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_features_20260813_256Mnand.img` |
| 镜像大小 | 38,973,440 bytes |
| 作用 | 任务真实数据、专注统计、天气、AI 状态与近期回复等功能聚合的历史构建。 |
| 验收状态 | 历史构建，不替代后续城市/时区/TLS 诊断产物。 |

## 已有真机/构建历史（摘要）

### 直接 MiMo TTS 等待播放版（2026-08-05）

```text
firmware/rtos_nuttx_r528s3-dshanpi_minui-wifi-direct-mimo-tts-audible-wait_20260805_256Mnand.img
SHA-256: 2ca067173513974b5000a78cdeeac6632e2f6e32f1b554ef343cfcae1e9334d8
```

- 当时构建、官方 pack 与 Dragon 都成功。
- 历史板端验证曾听到 `voice_test_speak 你好` 的声音，说明网络请求、解码和 `nxplayer` fallback 当时曾贯通。
- `mediad` 当时仍因 `amovie_async` 图配置失败；不要把这条镜像写成修复了 media daemon。
- 该历史通过不能覆盖后来的 TLS `-0x3b62`，当前应以 SPKI 诊断版取证。

### DMIC/ASR 候选（2026-08-05）

```text
firmware/rtos_nuttx_r528s3-dshanpi_mimo25-asr_20260805_256Mnand.img
SHA-256: 0da8f31a419af1cc950fbc3c6b9231c75a3b9766b40fafc79518a7f5e894b542
```

- 构建和打包成功，包含 MiMo ASR 批处理实现。
- 后续现场证明麦克风能采到非零 PCM，但 streaming callbacks 未实现，且持续 ALSA capture 有 prepare/read 问题；所以 ASR、唤醒词**不通过**。

### KASAN 调试路线（已完成其历史使命）

KASAN 不是当前显示堆破坏的唯一解法。相关构建最终可用，关键是 `asan_stubs.c` 必须用 `-fno-lto`；但随后已找出更直接的 G2D fill RGB565/ARGB8888 格式硬编码越界根因。需要新查无关堆越界时仍可再启用 KASAN，不能把旧 KASAN 记录误写成“仍在链接中”。

## 每次新构建的记录模板

```text
日期/标签：
改动目的：
镜像绝对路径：
镜像大小：
镜像 SHA-256：
ELF 绝对路径：
ELF 大小：
ELF SHA-256：
构建证据：BUILD_RC、编译错误/undefined reference 检查、Dragon SUCCESS：
烧录状态：未烧录 / 已烧录
真机验收：通过项、失败项、日志位置（均脱敏）：
```

## 构建检查清单

- [ ] R528 使用 make：`~/do_make_build.sh`，不带 `--cmake`。
- [ ] 只读 `~/build_make.log`；先确认 make 进程结束。
- [ ] 不手删 `libapps.a`；应用归档异常时处理模块 `.built`。
- [ ] `distclean` 后重新 configure，不能把旧 `.config` 拷回。
- [ ] `nuttx.bin` 已复制到 `nsh.fex`。
- [ ] Dragon 有 `SUCCESS`，且镜像时间戳/大小已更新。
- [ ] 镜像与匹配 `nuttx/nuttx` ELF 均已备份到 Windows。
- [ ] 已记录 SHA-256，且未在任何记录中写入秘密。


## 2026-09-05：语音/触摸/开机音质候选（未烧录）

| 项目 | 值 |
|---|---|
| 镜像 | `firmware/rtos_nuttx_r528s3-dshanpi_voice-touch-prekeyed-candidate_20260905_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,816,640 bytes / `f73a1e8210d663d1108cd122000116e55e06bb8ae0b9647aa8a3864450da1df2` |
| 配对 ELF | `archive/nuttx-20260905-voice-touch-candidate.elf` |
| ELF 大小 / SHA-256 | 68,539,352 bytes / `cb2dc6fccf54677aca49e40337abcb3721c498c534006b55cfbb0d060612d503` |
| 内核 bin 大小 / SHA-256 | 11,830,596 bytes / `f93b7956372b8dd0b71afc4eb43122dade2968451ee4cd0496e5fe55fbc33844` |
| 镜像生成时间 | 2026-09-05T15:23:27.111405+00:00（UTC；北京时间 2026-09-05 23:23） |
| VM 成对快照 | `/home/openvela/img_backups/voice-touch-candidate-20260905_20260905_155533` |
| 状态 | **已构建打包、已成对备份；未烧录，真机复验待完成。** |

- make 构建、链接退出 0；确认 `src/agent_main.c` 重新编译；无编译错误或 undefined reference。13 个 PTT 场景及核心故障注入通过。
- Dragon `SUCCESS` + `pack finish` + 新镜像时间戳。外层 `PACK_RC=1` 为已知包装脚本假失败；不以该返回值或单一字符串作为功能通过证据。
- 配对 ELF 按实际板级 objcopy 参数完整重建 bin，与打包输入一致；YAFFS 还原音频/脚本一致；最终镜像包含完整 userdata 和内核；Windows/VM SHA 一致。
- 首次中间打包因旧 MAINSRC 缓存被判为不交付；备份失效对象、重编后才生成上表最终候选。未 commit、未 push、未烧录。
- 保留已验证的 nxplayer stdin 入口；原音保留，候选音频衰减 6 dB；触摸、音质、TTS 仍待真机复验。镜像含预置凭据，不可外发。
- [完整改动与验证](voice-touch-20260905.md)。
