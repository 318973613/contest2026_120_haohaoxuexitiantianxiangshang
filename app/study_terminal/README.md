# 学习终端完整UI集成版本

## 概述

这是Contest 2026 Team 120的完整UI集成实现，将模拟器版本的5页UI适配到DShanPi板端，并对接ai_agent的语音和状态功能。

## 文件清单

- `study_terminal_main.c` - 主UI实现（1300+ 行）
- `task_presets.c/h` - 预设任务短语库
- `ui_status_bridge.c/h` - 系统状态桥接（读取UI_STATUS.json）
- `voice_ui_bridge.c/h` - 语音交互桥接（调用voice_channel和voice_wake API）
- `wifi_setup.c/h` - WiFi配置模块（复用自board-current）
- `Makefile` - 构建配置

## 主要特性

### 1. 五页UI布局

- **主页（Home）**: 显示任务统计和系统状态摘要
- **状态（Status）**: 实时监控CPU/内存/网络/运行时间
- **任务（Tasks）**: 预设按钮快速添加任务 + 任务列表
- **AI对话（AI）**: PTT按钮录音 + 聊天历史显示
- **设置（Settings）**: 音量调节 + 唤醒词开关 + WiFi配置

### 2. 输入方式适配

**遵循用户要求：预设短语 + 语音，无键盘**

- 任务页：6个预设任务按钮（完成作业、复习笔记、预习课程、练习题目、阅读书籍、背诵单词）
- AI页：仅PTT按钮（按住说话、松开识别）
- 完全移除了lv_keyboard和lv_textarea

### 3. 数据桥接

**状态显示**（2秒刷新）：
- ai_agent heartbeat → `/data/ai_agent/UI_STATUS.json`
- LVGL定时器读取 → 更新标签控件

**任务管理**（5秒刷新）：
- 预设按钮 → 写入 `/data/ai_agent/STUDY_TASKS.md`
- Markdown格式：`- [ ] 任务文本` / `- [x] 已完成任务`
- 原子写入：temp文件 + rename

**AI对话**（500ms刷新）：
- PTT按钮 → `voice_channel_start()` / `voice_channel_stop_with_text()`
- 等待ai_agent处理 → `/data/ai_agent/LAST_REPLY.json`
- LVGL定时器检测时间戳更新 → 追加到聊天记录

**音量控制**：
- 滑块调节 → 写入 `/data/ai_agent/STUDY_VOLUME.json`
- ai_agent的audio_playback.c自动读取

**唤醒词控制**：
- 设置页开关 → 调用 `voice_wake_start()` / `voice_wake_stop()`
- 默认关闭（遵循handoff要求，ASR验证通过后才启用）

### 4. 关键函数映射

| UI功能 | 调用的ai_agent API |
|--------|-------------------|
| PTT按下 | `voice_channel_start()` |
| PTT释放 | `voice_channel_stop_with_text()` |
| 唤醒词开 | `voice_wake_start()` |
| 唤醒词关 | `voice_wake_stop()` |
| 状态读取 | 读取 `UI_STATUS.json` (cJSON解析) |
| AI回复 | 读取 `LAST_REPLY.json` (cJSON解析) |

## 构建步骤

### 方式1：在VM中编译（推荐）

```bash
# 1. 将ai_agent_hdr目录复制到VM的比赛仓库
scp -r D:\openvela\_staging\integrate-20260806\ai_agent_hdr user@192.168.x.x:~/openvela/vendor/contest2026_120_haohaoxuexitiantianxiangshang/apps/study_terminal/

# 2. SSH进入VM
ssh user@192.168.x.x

# 3. 配置defconfig
cd ~/openvela
./tools/vela.sh export allwinnertech dshanpi mimo25-asr

# 4. 确保CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL=y
# 在 vendor/allwinnertech/boards/r528/r528s3-dshanpi/configs/nsh/defconfig

# 5. 编译
./tools/vela.sh build

# 6. 打包固件
cd out/allwinnertech_dshanpi/images/boot_package
dragon image.cfg sys_partition.fex

# 输出: rtos_nuttx_r528s3-dshanpi_mimo25-asr_<日期>.img
```

### 方式2：本地Windows编译（交叉工具链）

暂不支持，需要WSL或虚拟机环境。

## 依赖项

### NuttX配置要求

```
CONFIG_LVX_USE_DEMO_CONTEST2026_120_STUDY_TERMINAL=y
CONFIG_EXAMPLES_AI_AGENT_VELA=y
CONFIG_SYSTEM_CJSON=y
LV_USE_TABVIEW=y
LV_USE_SWITCH=y
LV_USE_SLIDER=y
LV_USE_CHECKBOX=y
LV_FONT_SIMSUN_16_CJK=y
```

### 外部模块

- **ai_agent包**: `packages/ai_agent/`（已在VM中存在）
  - voice_channel.c - 语音录音和ASR
  - voice_wake.c - 唤醒词检测
  - heartbeat.c - 系统状态心跳
  - message_bus.c - 消息总线

- **WiFi管理**: `wifi_setup.c`（已包含）

## 数据文件协议

所有JSON文件位于 `/data/ai_agent/`：

### UI_STATUS.json（ai_agent写入，UI读取）
```json
{
  "version": 1,
  "cpu_percent": 25.3,
  "mem_used_mb": 45,
  "mem_total_mb": 128,
  "network_ip": "192.168.1.100",
  "uptime_sec": 3600
}
```

### STUDY_TASKS.md（UI写入和读取）
```markdown
- [ ] 完成今天的作业
- [x] 复习今天的课堂笔记
- [ ] 预习明天的课程内容
```

### LAST_REPLY.json（ai_agent写入，UI读取）
```json
{
  "version": 1,
  "updated_at": 1722960000,
  "text": "好的，我帮你记录了这个任务。",
  "model": "mimo-v2.5-pro"
}
```

### STUDY_VOLUME.json（UI写入，ai_agent读取）
```json
{
  "version": 1,
  "volume": 70
}
```

### WIFI_CONFIG.json（wifi_setup写入和读取）
```json
{
  "version": 1,
  "ssid": "MyNetwork",
  "password": "********",
  "auto_connect": true
}
```

## 验证步骤

### 阶段1：ASR基础验证（当前优先级）

```bash
vela> voice_capture_test 5
# 说话5秒

vela> voice_test_asr /data/ai_agent/mic-capture.pcm
# 期望看到：ASR result: <你说的话>
```

**通过标准**：识别准确率 > 80%

### 阶段2：UI功能测试

1. **主页** - 显示统计数字正常
2. **状态页** - CPU/内存/网络实时更新（2秒刷新）
3. **任务页** - 点击预设按钮 → 任务出现 → 勾选完成 → 重启后仍保留
4. **AI页** - 按住PTT说话 → 松开显示识别文本 → 5-10秒后AI回复出现
5. **设置页** - 音量滑块调节 → TTS音量改变

### 阶段3：唤醒词测试（ASR通过后）

1. 设置页打开"唤醒词"开关
2. 说"小维同学" → 听到提示音
3. 6秒内说问题 → AI回复 → TTS播放

## 约束和注意事项

### 严格遵循的规则（来自HANDOFF_2026-08-06.md）

1. **不要在ASR验证前运行voice_wake_start**
2. **只使用允许的MiMo模型**：
   - mimo-v2.5-pro
   - mimo-v2.5
   - mimo-v2.5-asr
   - mimo-v2.5-tts-voiceclone
   - mimo-v2.5-tts-voicedesign
   - mimo-v2.5-tts
3. **不要索取、显示、记录API Key或密码**
4. **不要未经确认就commit/push/烧录**
5. **实体音量键功能已取消，不要扩展**

### 内存优化

- 聊天历史最多保留20条
- 任务列表最多显示5条
- 单缓冲LVGL模式
- 及时释放未使用的LVGL对象

## 已知限制

1. **聊天历史不持久化** - 重启后清空（可扩展保存到JSON文件）
2. **任务列表仅显示前5条** - 超出部分需要滚动或分页
3. **无任务编辑功能** - 只能添加预设任务和勾选完成
4. **AI对话无重试机制** - 识别失败需要重新按PTT

## 下一步计划

根据INTEGRATION_PLAN.md：

- [x] P0: UI适配（移除键盘、预设按钮、PTT）
- [x] P0: 任务桥接（Markdown CRUD）
- [x] P0: 语音桥接（PTT + API调用）
- [x] P1: 状态桥接（系统监控）
- [x] P1: WiFi集成（Settings页）
- [ ] P1: QEMU构建测试
- [ ] P2: DShanPi固件打包
- [ ] P2: 板端刷写和功能测试
- [ ] P3: 唤醒词验证（ASR通过后）

## 技术栈

- **UI框架**: LVGL 9.x
- **操作系统**: NuttX RTOS (openvela)
- **硬件**: Allwinner R528 DShanPi开发板（480x320 RGB565）
- **字体**: SimSun 16px CJK（板端编译）
- **数据格式**: JSON（cJSON库）、Markdown（纯文本解析）
- **语音引擎**: MiMo云端ASR/TTS

## 许可和作者

Contest 2026 Team 120 - 好好学习天天向上队  
此代码仅供比赛使用，未经授权不得用于商业用途。
