# contest2026_120_haohaoxuexitiantianxiangshang

## 最新交付：连续语音与提醒联动（2026-09-11）

当前产品为 R528S3 DShanPi、480×320 横屏、原生 LVGL C + ai_agent，不是 Quick App。以下早期项目说明和比赛模板保留为历史；当前验收以 [CURRENT_HARDWARE_STATUS](docs/CURRENT_HARDWARE_STATUS.md) 为准。

本次默认使用关闭思考的 `mimo-v2.5`，统一“你好小米”唤醒词，加入本地提示音和自动语音开页，修复等待回答期间重复抢麦的协调问题；主页显示最近提醒倒计时并支持取消，保留独立专注统计。保留已有 TLS 任务组隔离、触摸、FreeType、G2D 和开机音频修复。

38 组主机检查、15 个改动 C 文件重编、官方 make/Dragon 打包及 ELF 全量配对通过。**新候选未烧录，真实板端延迟、连续播报与触摸仍待验收。** 唤醒仍为联网批量 ASR，不宣称离线能力或固定秒数回复。

| 项目 | 值 |
|---|---|
| Windows 镜像 | `D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_voice-dialogue-prekeyed-candidate_20260911_256Mnand.img` |
| 镜像大小 / SHA-256 | 40,824,832 bytes / `af1655aee2f72607a67cb37601dfa74a5717dd5c0f458a0a7b39d3d9bfce54ce` |
| Windows 配对 ELF | `D:\openvela\archive\nuttx-20260911-voice-dialogue-candidate.elf` |
| ELF 大小 / SHA-256 | 68,584,104 bytes / `9bb192d148da9ba5cb36e5f40a3b193d0b7e3c360f5484256f9df848e8ce6a11` |
| VM 配对快照 | `/home/openvela/img_backups/voice-dialogue-candidate_20260911_155223` |

[完整改动与最小真机验收](docs/voice-dialogue-20260911.md)。镜像含本人预置凭据，仅在本地存放，不进入本仓或公开发布；配置只切换对话模型，其余字段保持不变。公共 ai_agent 改动仍在其公共仓本地，后续单独 fork/PR。本次未 commit、未 push、未自动烧录。

> 2026-09-12：用户告知 MiMo Key 已过期。镜像沿用原 Key，38 组主机检查未调用 MiMo 云端。须本地更新有效凭据后再验收联网识别、对话和播报，不能将鉴权失败当作模型速度问题。

## AI 智能学习与调试显示终端

本项目面向 2026 openvela AI 硬件开发者大赛，基于 openvela、LVGL 和
`ai_agent` 实现一个可真实运行的学习与调试终端。当前 QEMU Demo 包含首页、
系统状态、学习任务、AI 助手和设置五个页面，并已验证 MiMo 对话、工具调用、
任务记录、主动提醒、实时系统状态和完整中文显示。

主要代码位于 `app/hello_app/`，运行说明见
`app/hello_app/README.md`，设计依据见 `DESIGN.md`，真实运行截图位于
`assets/screenshots/`。作品代码只保存在本专属仓，不修改公共 `nuttx/`、
`packages/` 或 `vendor/` 仓库。

真机目标使用官方百问网 `r528s3-dshanpi` BSP。官方补丁、真机配置编译和
全志镜像打包已于 2026-07-13 跑通，镜像已生成并完成 SHA256 校验；物理板
烧录、启动、屏幕方向和触摸映射仍待验证。准确状态和安全操作顺序见
`docs/HARDWARE_BSP.md`。

### 当前实现路线（2026-07-15）

最终作品采用专属仓自有的原生 LVGL C 应用 + `ai_agent`。原生应用位于
`app/hello_app/`，包含首页、系统状态、学习任务、AI 助手和设置五个页面。
任务、最近 AI 回复和主动提醒优先通过 `/data/ai_agent/` 下的共享数据同步，
先保证真机可编译、可运行、可演示，再评估 UDP 或消息队列。

Quick App 不再作为最终作品主路线。已有 `quickapp/hello_quickapp/`、RPK、
截图和真机诊断结果会保留为历史实验材料，不删除，也不继续优先修复其
UIKit/预编译库兼容问题。

2026-07-15 已完成第一版 DShanPi 原生 LVGL 构建。最终 `nuttx.bin` 为
9,961,348 字节，ELF 包含 `study_terminal_main`，不包含 `vapp_main` 或
`xiaozhi_gui_main`。新镜像已完成官方 `dragon` 封装和离线内容检查：

```text
D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl_256Mnand.img
size: 36,743,168 bytes
sha256: ad652bfcbb1d5f139a401ffba636dc3298eae44e02bc081fb4e4ff6973fe6303
```

镜像包含 `study_terminal` 自启动和 NotoSansSC 字体，不包含 Quick App 包名或
`vapp` 启动命令。当前仍未烧录、未 commit、未 push。

### Quick App 历史实验状态

在保留 `app/hello_app/` LVGL 五页实现作为稳定回退的同时，项目已在
`quickapp/hello_quickapp/` 建立官方 openvela Quick App 功能界面。它包含
五个真实可交互页面，并接入专注计时、主动提醒、任务持久化、系统状态 API、
`@system.velaclaw` AI 问答和设置持久化。

2026-07-12 已完成 Windows 工具链构建，并在 QEMU 中验证中文字体、真实触摸
导航、系统状态页和任务完成动作。最终修复版 RPK 已构建但尚未做最后一次
QEMU 部署验证；当前 MiMo backend 也需要用户在本地重新配置。准确交接状态和
下次操作顺序见 `docs/PROGRESS.md`。

---

👋 欢迎参加 **2026 首届 openvela AI 硬件开发者大赛**！

这是组委会为你的队伍创建的**专属参赛仓库**（本仓为样例/模板，队伍编号 `120`；你看到的将是你自己的 `contest2026_<编号>_<队伍名>`）。比赛期间，你的全部参赛代码、打包产物与 AI Coding 日志都提交到这里。

> 本仓既是「代码仓」，又内置了一键拉取整套 openvela 工程的 `repo` 清单（manifest）。你只需跟它打交道，**自始至终只动一个文件夹**。

---

## 一、先读这些官方文档

**通用（所有赛道必读）：**

| 文档                                                                                                                                     | 用途                                           |
| ---------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------- |
| [《大赛总览》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/contest_overview.md)                        | 赛道、流程、评分、资源，建议先通读             |
| [《参赛代码提交指南》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/code_submission_guide.md)           | 仓库获取、提交流程、时间与权限（**以此为准**） |
| [《AI Coding 日志归集与提交手册》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md) | 如何导出 AI 对话日志并提交到 `logs/`           |

**按你的赛道选读（三选一）：**

| 赛道                  | 教程导航                                                                                                                                                 |
| --------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 快应用 / 手表应用创新 | [快应用教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/quickapp/quickapp_guide_index.md)                         |
| AI 硬件产品创新       | [AI 硬件赛道教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_hardware/ai_hardware_guide_index.md)              |
| 新硬件适配            | [新硬件适配赛道教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/hardware_porting/hardware_porting_guide_index.md) |

---

## 二、第一步：拉取完整工程

用组委会提供的命令一键拉取「openvela 全量源码 + 你的专属仓」：

```bash
repo init -u https://github.com/open-vela/contest2026_120_haohaoxuexitiantianxiangshang \
  -b dev-ai-contest-2026 -m contest2026_120_haohaoxuexitiantianxiangshang.xml
repo sync -c -j8
```

同步后，你的整个仓库位于工作区的 `contest2026_120_haohaoxuexitiantianxiangshang/`，openvela 全量源码在外层（`nuttx/`、`apps/`、`packages/`、`vendor/` 等）。

---

## 三、第二步：在哪里写代码

**只在自己的仓目录 `contest2026_120_haohaoxuexitiantianxiangshang/` 里开发。** 不同作品形态放在对应子目录，manifest 会通过 `<linkfile>` 把它们**软链**到 openvela 编译树该在的位置——你不用手动 copy：

| 作品形态 | 你的代码放这里             | 系统自动映射到                                 |
| -------- | -------------------------- | ---------------------------------------------- |
| 应用     | `app/hello_app/`           | `packages/demos/contest2026_120_hello_app`     |
| 快应用   | `quickapp/hello_quickapp/` | `packages/apps/contest2026_120_hello_quickapp` |
| 板级适配 | `board/contest_board/`     | `vendor/openvela/boards/contest2026_120_board` |

> 用不到的形态目录可以删掉；新增作品时按同样规则加子目录，并在 `contest2026_120_haohaoxuexitiantianxiangshang.xml` 里补一条 `<linkfile>` 映射即可。**生产仓库（packages/nuttx/vendor 等）零改动。**

建议仓库目录约定（便于评委定位）：

```text
app/ | quickapp/ | board/   # 你的作品代码
logs/                       # AI Coding 日志（主动导出后提交，格式见 logs/README.md）
README.md                   # 作品说明（提交前请改成你自己的，见第六节）
```

> 仓内附带了一个 `.gitignore.example`，给出了**编译产物**等不需要进仓的文件示例。如需启用，`cp .gitignore.example .gitignore` 后按需增删即可。**注意 `logs/` 下最终导出的 AI Coding 日志必须提交，不要忽略。**
>
> `logs/` 的目录结构与提交格式见 [logs/README.md](logs/README.md)。

---

## 四、第三步：编译与运行

编译/运行步骤随作品形态不同而不同，请参考你所在赛道的教程导航：

- 快应用 / 手表应用：[快应用教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/quickapp/quickapp_guide_index.md)（含模拟器与开发板部署）。
- AI 硬件产品创新：[AI 硬件赛道教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_hardware/ai_hardware_guide_index.md)（环境搭建、编译烧录、Skill 开发）。
- 新硬件适配：[新硬件适配赛道教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/hardware_porting/hardware_porting_guide_index.md)（BSP 移植、最小 NSH 基线）。

子目录已通过 manifest 中的 `<linkfile>` 软链进 openvela 编译树，因此构建在 openvela 工作区**根目录**（即你这个仓的上一级）进行。openvela 使用 `build.sh` 作为统一入口，接收一个 **board config 路径**作为参数：

```bash
# 进入 openvela 工作区根目录（你的仓的上一级）
cd ..

# 通用语法：第一个参数是 board config 路径，第二个参数可以是 menuconfig / distclean 等
./build.sh <board-config-path> [menuconfig|distclean] [-j8]
```

> 具体的 board config 路径、目标产物、模拟器/真机部署方式请以你所在赛道的教程导航为准。本仓 `app/` `quickapp/` `board/` 三个示例骨架对应的 Kconfig 选项可通过 `menuconfig` 启用。

---

## 五、第四步：提交作品

1. **fork** 你的专属仓 → 开发 → `git commit` 并推送 → 向专属仓发起 **Pull Request**，可**自行 review 并合入**（无需等组委会）。
2. **AI Coding 日志**：与 AI 工具的对话会自动记录到本机 staging（不会自动上传），需你**主动导出/打包**选定会话到仓内 `logs/` 目录后一并提交。详见[《AI Coding 日志归集与提交手册》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md)。
3. 若需改动 **nuttx 等公共仓库**，不在本仓改，而是 fork 对应公共仓、以 PR 提交到 `dev-ai-contest-2026` 分支，由组委会 review 后合入。

> ⏰ **提交作品截止：9 月 20 日**。截止后统一收回 push 权限，仍可查看 / clone。
>
> 获奖后再按要求将作品 PR 至 openvela 上游对应仓库（走标准 PR + CI 流程）。

### 关于 PR 与 CLA

- 本仓所有改动通过 **Pull Request** 合入（分支保护强制，可自行合入自己的 PR）。
- 首次贡献需在[**官网签署 CLA**](https://openvela.com/#/community/cla)；PR 上会自动跑 `cla/signature` 检查，在官网签署成功后，在 PR 评论 `/check-cla` 复检即可通过。

---

## 六、提交前：把本 README 改成你的作品说明

本文件目前是组委会给的**使用说明书**。**作品提交前，请把它替换成你自己作品的说明**，方便评委快速了解你做了什么、怎么跑起来。建议至少包含以下内容：

```markdown
# <你的作品名>

## 一、作品简介
<一句话/一段话说明这个作品是什么、解决什么问题、亮点在哪>

## 二、选题方向
<快应用 / 手表应用创新 ｜ AI 硬件产品创新 ｜ 新硬件适配 ｜ 自定方向，并简述理由>

## 三、目录结构
<列出你这个仓里各目录/文件的作用，例如：>
- `app/xxx/`        — <说明>
- `board/xxx/`      — <说明>
- `quickapp/xxx/`   — <说明>
- `logs/`           — AI Coding 日志
- `docs/` 或其他    — <说明>

## 四、运行方式
<拉取工程后，如何编译、烧录/部署、运行的完整步骤；最好能让评委照着一步步复现>

## 五、AI Coding 使用说明
<说明本作品如何借助 AI 辅助开发：
- 在需求拆解 / 方案设计 / 编码 / 调试 / 文档等环节如何与 AI 协作；
- AI 对开发效率或质量带来的实际帮助。
完整对话日志见 logs/ 目录>
```

> 提示：将会根据「作品本身 + 你的 README 说明 + `logs/` 里的 AI Coding 日志」来理解和评估你的作品，README 写清楚很重要。

---

## 附：仓库命名规范

`contest2026_<编号>_<队伍名>` — 编号三位零填充；队名 slug（全小写、英文/拼音、连字符）。例：`contest2026_120_haohaoxuexitiantianxiangshang`。
（仓库由组委会统一创建，**每队仅一个仓**，无需自行命名。）

---

## 当前开发进度（2026-07-13）

作品已于 2026-07-15 将原生 LVGL 恢复为正式交付路线。提交 `63a55a8` 是
已验证的五页 LVGL 基线；Quick App 源码位于 `quickapp/hello_quickapp/`，
仅作为历史实验保留。

本轮已完成：

- 以五张高保真参考图为视觉基准，重排真实 Quick App 组件；未使用静态整图
  作为最终页面。
- 修复 Windows `vela-watch-5.0` 的 1.5 倍布局缩放问题，使组件按
  `853x533` 逻辑尺寸映射到 1280x800 framebuffer。
- 首页和系统状态页已完成真实组件铺满验证。
- 状态读取增加缺失 API 防御。Windows 模拟器没有 battery 和 velaclaw 时，
  页面会显示“不可用”或友好提示，不会中断后续 device/storage 查询，也不会
  永久停留在“处理中”。
- 状态页实测读取到 WiFi、Emulator-Vela、NuttX 12.3.0、1280x800、
  总存储 255.9 MB 和可用存储 149.9 MB。
- Windows 执行 `npm run build` 成功生成调试 RPK。

当前仍在进行：

- 校准任务页和 AI 页的 Quick App 原生输入层位置与尺寸。
- 完成任务新增/完成/移除、AI 输入、语音按钮和设置持久化的逐页验证。
- 在 `packages/ai_agent` 公共仓单独实现 ASR start/stop 消息分发，再在 QEMU
  验证真实语音识别；该公共仓改动不会混入比赛专属仓提交。
- 对照参考图保存五张最终真实组件截图，并完成真实 QEMU 和 DShanPi 验证。

本轮未执行 commit 或 push。

## 2026-07-22 真机触摸、刷新与 WiFi 配网

- FT5X06 横屏触摸采用交换 X/Y 后反转横屏 X 坐标，用户已真机确认触摸与
  480x320 显示一致。
- SPI LCD framebuffer 已从硬编码 8 MHz 改为可配置，DShanPi 当前设置为
  24 MHz。开机 Logo 和进度条已恢复，同时取消小屏连续全屏淡入。
- 新增原生 LVGL WiFi 配网模块，包含扫描、热点选择、密码键盘、连接保存、
  启动重连和掉线弹窗。ARM LTO、官方 pack 和 dragon 均已成功。
- 最终镜像为
  `rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-touch-perf_20260722_256Mnand.img`，
  大小 36,965,376 字节，SHA256 为
  `45d4e1fe210ab73348ae2ac04112a8afea3c1ed2d4d8d93ecca56937935c604c`。

真机结果：用户已自行烧录，触摸正常；但 WiFi 没有完成扫描或连接，配网页面
没有按预期停留，应用直接进入主页。因此 WiFi 当前状态是“代码和构建完成，
真机验证失败”，不能标记为功能完成。用户要求暂停，后续先只读检查启动日志、
运行条件、配网层层级和扫描线程，再进行最小修复。本轮未 commit 或 push。

## 2026-07-22 串口控制台争用修复

- 真机串口可以输出 `nsh>`，但输入命令丢失，并交替出现 `vela>` 和 `nsh>`。
  根因是后台 `ai_agent` 的 CLI 线程与 NSH 同时读取 `/dev/console`，不是已经
  证明的 LVGL 资源耗尽。
- 公共 `packages/ai_agent` 增加 `--no-cli` 参数。该模式只跳过 stdin CLI，
  AI、网络、工具、语音和心跳服务继续运行。
- 板级启动脚本改为 `ai_agent --no-cli &`，使开机后的 NSH 独占串口。
- ARM LTO、官方分区生成和 `dragon` 封装成功；离线检查确认新参数、禁用日志
  和启动命令均进入镜像。
- 新镜像为
  `rtos_nuttx_r528s3-dshanpi_native-lvgl-wifi-console-fix_20260722_256Mnand.img`，
  大小 36,965,376 字节，SHA256 为
  `b870c19008752484a0a7638b5dfc2f95b70c130d2daacac6931753976b13aea4`。

新镜像尚未烧录，串口修复仍待真机验证。WiFi 配网根因也仍未确认；下一步先
验证 `nsh>`、`ps` 和 `ifconfig wlan0`，再读取脱敏的 WiFi 临时日志。完成首次
配网、重启重连和掉线弹窗闭环前不开始 OTA。本轮未 commit 或 push，助手未
执行烧录。

## 2026-07-23 当前路线与验证边界

- 最终交付 UI 采用原生 LVGL C 五页应用和 `ai_agent`。
- Windows Quick App 高保真原型仅作为历史实验保留，不进入最终固件。
- `_staging/lvgl-ui-redesign-win` 是另一套在 Windows 下调试的原生 LVGL
  重设计，不是 Quick App；它仍是最终原生 UI 的视觉候选。
- 当前 `full-lvgl-delay20` 镜像为了排查启动问题，临时使用较早的原生 LVGL
  五页基线，没有包含上述 Windows 原生 LVGL 重设计的壁纸和新版布局。
- min-ui 只用于显示路径故障隔离。已确认无 UI、官方 framebuffer、min-ui
  和 20 秒延迟启动均可稳定运行。
- 启动故障根因是 NuttX NSH 的 `CONFIG_NSH_LINELEN=64` 截断了过长脚本注释；
  启动脚本已改为每行不超过限制。
- 当前完整 UI 延迟启动镜像只启动 `study_terminal`，用于验证原生界面，
  不代表 WiFi、`ai_agent` 或语音唤醒已经在真机通过。
- ASR/语音代码已进入构建，但麦克风、唤醒词、ASR/TTS 和主动语音提醒仍需
  单独语音镜像和用户批准的真机验证。
- 后续 UI 工作应在保留短启动脚本和已验证触摸映射的前提下，重新合入 Windows
  原生 LVGL 重设计；不恢复 Quick App 运行时。

本轮不 commit、不 push、不自动烧录。
