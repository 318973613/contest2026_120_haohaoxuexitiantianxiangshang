# 学习终端文件协议

本协议是原生 LVGL、`ai_agent` 和 `study-assistant` 之间的低风险数据边界。
所有文件位于 `/data/ai_agent/`。当前先实现系统状态；其余功能必须按任务、AI、
提醒、设置的顺序逐项实现和验证。

## 通用规则

- 写入方先写同目录临时文件，例如 `UI_STATUS.json.tmp`，完整 `fflush()`、
  `fsync()`、`fclose()` 后再用 `rename()` 原子替换目标文件。
- 读取方遇到文件缺失、字段缺失、内容截断或解析失败时，保留上一份有效数据；
  若没有有效缓存，则显示安全默认值。
- JSON 文件使用 UTF-8、对象顶层结构和整数 Unix 时间戳。未知字段必须忽略，
  便于后续兼容扩展。
- 文本字段去除控制字符并限制长度。运行证据不得包含 API Key、Authorization、
  WiFi 密码、账号密码或完整隐私对话。
- LVGL 默认每秒刷新系统状态，每 5 秒检查任务、AI、提醒和设置文件；文件修改
  时间未变化时不重复解析。

## `STUDY_TASKS.md`

- 生产者：`study-assistant`、原生 LVGL 任务页。
- 消费者：`study-assistant`、原生 LVGL 首页和任务页。
- 每个任务占一行：`- [ ] 文本` 表示待办，`- [x] 文本` 表示完成。
- 任务文本去除换行和控制字符，最大 80 个 UTF-8 字节。
- 新增、完成和删除都必须重写临时文件后原子替换，不能原地局部修改。

## `UI_STATUS.json`

- 生产者：`ai_agent` 或同步辅助脚本。
- 消费者：原生 LVGL 首页和 AI 页。
- 更新频率：AI 状态或工具结果变化时写入，空闲时最多每 30 秒一次心跳。

```json
{
  "version": 1,
  "updated_at": 1784160000,
  "ai_online": true,
  "state": "idle",
  "tool_name": "study_task_list",
  "tool_result": "3 个待办任务"
}
```

`state` 只使用 `offline`、`idle`、`thinking`、`tool`、`error`。工具结果必须脱敏，
最大 160 个 UTF-8 字节。

## `LAST_REPLY.txt`

- 生产者：`ai_agent`。
- 消费者：原生 LVGL AI 页。
- 只保存最近一次适合屏幕展示的脱敏回复，最大 512 个 UTF-8 字节。
- 不保存请求头、模型原始响应、历史对话或用户输入全文。

## `REMINDERS.json`

- 生产者：`study-assistant`、原生 LVGL 提醒入口。
- 消费者：`study-assistant`、原生 LVGL 首页和提醒层。

```json
{
  "version": 1,
  "updated_at": 1784160000,
  "items": [
    {
      "id": 1,
      "text": "休息一下",
      "due_at": 1784160060,
      "state": "pending"
    }
  ]
}
```

`state` 只使用 `pending`、`fired`、`read`。提醒文本最大 80 个 UTF-8 字节。
到期与已读状态都必须持久化，日志只记录提醒 ID、状态和脱敏摘要。

## `STUDY_SETTINGS.json`

- 生产者和消费者：原生 LVGL 设置页。
- 用户点击开关后立即原子写入；启动时读取一次，后续每 5 秒检查外部变化。

```json
{
  "version": 1,
  "active_reminders": true,
  "auto_refresh": true,
  "page_animation": false,
  "debug_info": false
}
```

字段缺失时使用上面示例中的默认值。解析失败不得覆盖内存中的有效设置。

## `STUDY_VOLUME.json`

- 生产者：原生 LVGL 应用的实体音量键处理。
- 消费者：原生 LVGL 应用和 `ai_agent` 的直接 TTS 回退播放器。
- 仅保存用户可见的输出音量和已完成校准的两个按键掩码；不保存 API Key、Wi-Fi
  密码或任何对话内容。
- 初始音量为 85，范围为 0 到 100；每次按键调整 10，结果必须限制在该范围。
- 首次运行时应用先请求用户依次按音量加、音量减来记录实际 LRADC 按键掩码，避免
  假定开发板的物理键接线。

```json
{
  "version": 1,
  "volume": 85,
  "volume_up_mask": 1,
  "volume_down_mask": 2
