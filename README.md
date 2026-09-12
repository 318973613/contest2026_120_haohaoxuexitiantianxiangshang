# openvela 学习终端

> 2026 首届 openvela AI 硬件开发者大赛参赛作品
> 队伍编号 `120` · 仓库 `contest2026_120_haohaoxuexitiantianxiangshang`
> 硬件：百问网 DShanPi（全志 R528S3，256M NAND，480×320 横屏）
> 唤醒词：**「你好，openvela」/「Hello，openvela」**

---

## 一、作品简介

一台跑在 openvela 上的**学习终端**。核心主张是**断网也能用**：把学习过程中真正高频的动作——列任务、计时专注、按时提醒、看这周学了多久——全部做成本地能力，不依赖云端；联网时再叠加 AI 问答与语音唤醒。

**断网之后，它仍然是一台完整可用的学习工具**，而不是一块只会转圈的黑屏。

主要能力：

- **五个主页面**：首页、系统状态、学习任务、AI 助手、设置。
- **离线学习工具**（全部本地实现，不调用云端）：
  - 提醒中心——集中管理待办提醒，主页显示最近一条倒计时并支持取消；
  - 离线提示音——无网络时也能给出声音反馈；
  - 五分钟稍后提醒——一键把当前提醒顺延；
  - 专注目标——设定目标时长并跟踪完成度；
  - 七日学习记录与学习报告——按天汇总专注时长与任务完成情况。
- **语音交互**：唤醒词「你好，openvela」；唤醒后可直接下达指令（如「介绍 hello world」）；采用云端 ASR 回退识别。
- **5 个自定义 Skill**：学习助手、每日复习、引导式学习、设备自检、学习计划。
- **双分辨率自适应**：480×320（真机）与 1280×800（模拟器）共用同一套代码，靠紧凑布局开关自动切换。

技术路线是**原生 LVGL C 应用**（`app/hello_app/`）配合 openvela 自带的 `ai_agent` 组件，**不使用快应用**。

---

## 二、选题方向

**AI 硬件产品创新。**

理由：本项目把「AI 助手」当作学习终端的一部分，而不是全部。先用本地能力保证设备在无网络、无有效凭据时依然是一台可用的学习工具，再用 AI 增强它。这恰好是 AI 硬件在真实使用场景里必须回答的问题——**断网了，还剩下什么**。

---

## 三、目录结构

```text
contest2026_120_haohaoxuexitiantianxiangshang/
├── app/hello_app/              # 作品主体：原生 LVGL C 应用
│   ├── study_terminal_main.c   # 主界面与全部离线学习功能（约 6900 行）
│   ├── voice_ui_bridge.c/.h    # 语音 UI 桥接、唤醒词匹配与指令提取
│   ├── ai_chat_bridge.c/.h     # AI 问答桥接
│   ├── ui_status_bridge.c/.h   # 系统状态读取（WiFi / 存储 / 内存）
│   ├── study_focus_stats.c/.h  # 专注统计与七日记录
│   ├── task_presets.c/.h       # 学习任务预设
│   ├── wifi_setup.c/.h         # 配网
│   ├── volume_keys.c/.h        # 音量键处理
│   └── lv_font_study_16.c      # 中文字体子集（CJK）
├── src/skills/                 # 5 个自定义 Skill（大赛要求 ≥ 1 个）
├── src/config/                 # 运行时配置模板
├── assets/                     # 字体与界面参考图
├── docs/                       # 工程记录与验收状态（详见 docs/CURRENT_HARDWARE_STATUS.md）
├── demo/                       # 模拟器启动脚本、技能/字体安装脚本、运行日志
├── board/contest_board/        # 板级适配骨架
├── quickapp/                   # 历史快应用实验（非主路线，保留备查）
├── logs/                       # AI Coding 日志
├── openvela.xml                # openvela 基础工程清单
└── contest2026_120_...xml      # 本仓 manifest，把上述子目录软链进 openvela 编译树
```

其中 `app/` `quickapp/` `board/` 三个子目录通过 manifest 的 `<linkfile>` 映射到 openvela 编译树的对应位置，**不需要手动拷贝**，也不改动任何公共仓库源码。

---

## 四、运行方式

### 1. 拉取完整工程

```bash
repo init -u https://github.com/open-vela/contest2026_120_haohaoxuexitiantianxiangshang \
  -b dev-ai-contest-2026 -m contest2026_120_haohaoxuexitiantianxiangshang.xml
repo sync -c -j8
```

同步后，本仓位于工作区的 `contest2026_120_haohaoxuexitiantianxiangshang/`，openvela 全量源码在外层（`nuttx/`、`packages/`、`vendor/` 等）。

### 2. 编译

构建在 **openvela 工作区根目录**（本仓的上一级）进行，用官方 `build.sh`：

```bash
cd ..
./build.sh vendor/allwinnertech/boards/r528/r528s3-dshanpi/configs/nsh/ -j8
```

### 3. 打包与烧录

编译产物经全志 Dragon 工具打包为 256M NAND 镜像后烧录至 DShanPi。烧录步骤与安全操作顺序见 `docs/HARDWARE_BSP.md`。

### 4. 模拟器（可选）

`demo/scripts/` 下提供了模拟器启动脚本与技能、字体安装脚本，可在无开发板时先验证界面与交互。

---

## 五、当前验收状态

**已完成并有证据的部分：**

| 项目 | 状态 |
|---|---|
| 主机回归测试 | 83/83 测试组通过，0 失败，ASan/UBSan 无告警 |
| 官方 make 全量构建 | 通过，0 error |
| Dragon 打包 | `execute image.cfg SUCCESS` + `pack finish` |
| 镜像与 ELF 配对 | ELF → bin 字节一致，11,855,284 bytes |
| 载荷校验 | `packed_private_config_matches_seed: true`、`non_model_settings_unchanged: true` |
| 唤醒词合规 | 二进制中「你好，openvela」命中 6 处；旧的小米品牌唤醒词命中 0 处 |

**尚未完成的部分（如实说明）：**

- **镜像未烧录到真机验证。** 真实板端的语音唤醒识别率、连续播报延迟、触摸映射仍待验收。
- 唤醒依赖**联网批量 ASR**，因此不宣称离线语音能力。
- 模型凭据需在本地配置有效值后方可验收联网问答。

准确、逐项的功能验收状态以 `docs/CURRENT_HARDWARE_STATUS.md` 为唯一权威。

---

## 六、合规与边界

- **唤醒词合规**：按大赛规则统一使用「你好，openvela / Hello，openvela」。匹配时会折叠大小写与分隔符，因此「Hello，OpenVela」「你好 openvela」均可唤醒；旧的品牌唤醒词已移除。
- **公共仓改动分开提交**：涉及 `packages/ai_agent` 的改动属于公共仓，不在本仓内直接修改，而是单独 fork 公共仓、向其 `dev-ai-contest-2026` 分支提 PR。
- **凭据不入仓**：固件镜像中包含本地预置的模型凭据，该镜像**仅在本地保存，不进入本仓、不公开发布**。仓库内不含任何密钥。
- **AI Coding 日志**：位于 `logs/<github_login>/`，由大赛官方 `contest-log-collector` 工具导出，未做任何内容修改。

---

## 七、文档索引

| 文档 | 内容 |
|---|---|
| `docs/CURRENT_HARDWARE_STATUS.md` | 功能验收状态（唯一权威） |
| `docs/successful_builds.md` | 历次成功构建的镜像与 ELF 配对记录 |
| `docs/HARDWARE_BSP.md` | 板级适配与烧录说明 |
| `DESIGN.md` | 设计依据 |
| `logs/README.md` | AI Coding 日志目录结构说明 |
