# AI 回复后触摸失效、无播报和唤醒故障（2026-09-06）

用户已烧录 `voice-touch-prekeyed-candidate_20260905`（镜像 SHA-256 `f73a1e8210d663d1108cd122000116e55e06bb8ae0b9647aa8a3864450da1df2`），但 AI 回复后仍不能点击或继续对话，回复无声音，唤醒词不可用。上一版的“未烧录”是当时交付状态，现在由这次真机反馈更新为**已烧录、关键复验失败**。

本次源码修复、主机回归、DShanPi make 构建、官方打包与 Windows/VM 配对校验已完成。**尚未烧录，未声称真机通过。**

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


## 新日志的直接证据

输入来自用户 9/6 粘贴的 346 行串口日志。仅摘录不含凭据的诊断字段：

- 00:02:51，LLM 返回 44 bytes 并分发到 `lvgl_ui:ui`；UI 线程同时出现 `FT_Load_Glyph error(0x55)`、多次中文字形查找失败。
- 同时 TTS 线程明确输出 `speak: skipped, recording active`、`[ui_voice] TTS failed`。这次播报是在调用 TTS 网络前被拒绝。
- 唤醒线程反复录音 80000 bytes，再执行 batch ASR；有一次返回 `1.`，之后连续 `wake listen failed: -5`。监听确实已经启动。
- `TLS caller: thread=150 stack=32744` 证明增栈已进入板端；仍有 `-0x3b62`、`-0x7080`、`-0x7200`。其中 `msglen=84` 却有 `hslen=9383953`、前缀 `f38f300d06` 等异常数据；不能仅据错误码就断言服务端不支持 TLS。
- FreeType 的实际错误表将 `0x55` 定义为 `Invalid_Stream_Operation`。日志共 41 次同类报错；这不是“字体不包含这些汉字”的错误码。

## 根因与证据边界

`study_terminal` 和 `ai_agent` 是 NuttX 的不同任务组。flat 构建下它们共享 C 全局变量，但各自使用 `tcb->group->tg_fdlist`。pthread 则加入父任务组，因此用 `getpid()` 标识 fd 所有者，不能用 `pthread_self()`。

`packages/ai_agent/src/infra/vela_tls.c` 的静态连接池原先只按 host/port 匹配。UI 的 ASR/TTS 与 agent 的 LLM 因而可能拿到另一任务创建的 TLS context。context 内的整数 fd 在当前任务中可能对应字体、触摸设备或另一个网络连接。

当前 mbedTLS `net_sockets.c` 的接收函数使用 `read(fd, ...)`；清理函数执行 `shutdown(fd, 2)` 后直接 `close(fd)`。于是错误不止是 HTTPS 失败，还会误关当前任务的文件。FreeType 后续按保留的 `FILE *` 读字体会失败，触摸 fd 被关闭后也不会因一次 UI 刷新而自行恢复。

主机测试使用 Linux `clone(CLONE_VM | SIGCHLD)`，刻意不设 `CLONE_FILES`，重现“共享静态池、独立 fd 表”的关键条件，执行实际连接池 C 代码和真实 `close()`。旧版在**复用、淘汰、清理三条路径**均误关 UI 描述符，新版均保留，同组另一 pthread 仍可复用连接。测试不执行真实 TLS 握手。该代码缺陷已证实；现场具体被误关的 fd 编号尚未直接采到，最终仍需真机复验。

第二个缺陷是后台云端唤醒没有让出麦克风，TTS 见录音状态直接返回 `-EBUSY`。此外，重复 `voice_channel_init()` 会清空正在使用的全局录音状态，ASR 尚未结束时又提前发布 IDLE。这些路径在 UI 和 agent 共用全局语音通道时会互相干扰。

## 修改范围

仅修改 `packages/ai_agent` 公共组件的六个源码文件；必须保留为该公共仓的独立 diff，不能混入比赛专属仓提交：

1. `src/infra/vela_tls.c`：池槽记录创建任务组，匹配、淘汰和清理都检查所有者。池被其他任务占满时走现有临时连接路径。诊断增加任务组与 fd 编号，不输出密钥或应用正文。
2. `src/voice/voice_channel.c/.h`：初始化幂等；录音资源保留所有者并持续到 ASR 完成；非所属任务禁止关闭录音。增加丢弃录音而不调用 ASR 的取消入口。前台录音/TTS 暂停唤醒，TTS 串行化直到设备关闭；失败路径释放占用。
3. `src/voice/voice_wake.c/.h`：唤醒窗口由监听线程自行停止和收尾，前台等待发生在已有 worker 中。暂停计数防止多个前台操作提前恢复监听；晚到的旧识别结果丢弃。等待有 45 秒上限，失败后可重试。
4. `src/agent_main.c`：播报失败冷却期间丢弃的回复也结束唤醒会话的回复等待，避免多等 60 秒。

既有异步 PTT UI、FT5X06 队列、G2D、popen 栈、nxplayer stdin 启动和原始/衰减开机音频均保留。

## 已完成的验证

- 连接池：旧源码三种误关路径均复现；新源码三种路径均保护文件；同组 pthread 保持复用。
- 原始语音代码：唤醒正在录音时，TTS 稳定返回 `-16`（`-EBUSY`）。
- 实际 `voice_channel.c`、`voice_wake.c`、`voice_ui_bridge.c` 一起编译，在 ASan/UBSan 下用有门控的音频、ASR、TTS 后端联测：唤醒与播报切换，三轮 PTT/ASR/TTS，慢 ASR 时前台接管及晚到结果丢弃，识别中拒绝第二次录音，网络/播放关闭期间禁止抢麦，录音 open/start/分配/线程/读取失败，非所属任务停止被拒绝，TTS 失败后重试，以及唤醒词、提示音、结束对话与重新开启监听。所有场景通过。
- 三轮联测中 UI bridge 的开始/停止提交均小于 50 ms。这是主机测试断言，不是板端触摸延迟测量。主机录音线程为 sanitizer 留了额外栈空间，板端仍使用工程配置。
- 223 个当前源码/配置 SHA 校验后，备份旧镜像、ELF、bin、map 和配置，同步六个文件；已备份移走相关八个缓存对象，保留 `libapps.a`。

- DShanPi make 与 ARM LTO 链接退出 0，四个改动的 C 文件都有实际 `CC` 记录。最终 ELF 含新的 `voice_wake_suspend/resume`、`recording_take`、`voice_channel_start_locked` 符号及任务组诊断；不是只修改了源码副本。
- 按板级参数 `objcopy -R .note.gnu.build-id -O binary` 导出的全部 11,830,628 bytes 与 `nuttx.bin` 相同，`nsh.fex` 同步相同内核。配置保持 16 位 LVGL、popen 20480 栈和原生 study_terminal，ELF 无 vapp/xiaozhi_gui 入口。
- 官方 pack 同时有 `Dragon execute image.cfg SUCCESS`、`pack finish` 和新产物时间戳。外层返回 1 是既有包装脚本问题。完整 userdata 与配对内核都在最终 `.img` 中；从 YAFFS 还原启动脚本、bootvoice.cmd、原 WAV/PCM 和衰减 PCM，全文件一致。
- Windows 的镜像和 ELF 大小、SHA-256 与 VM 快照完全一致。构建日志仍有既有 BSP/NAND 警告，以及本次两处局部 `ret` 遮蔽警告；没有编译错误或 undefined reference，未声称零警告。

证据与重现脚本：`D:/openvela/_work/voice-reply-20260906/`。构建前快照：`/home/openvela/openvela/_local_backups/voice-reply-20260906_050949/`。

## 真机验收

1. 冷启动后先不开唤醒，完成三轮“按住说话 → 松手识别 → AI 文字及声音回复”，每轮后切换其他页面确认触摸持续可用。
2. 再开启唤醒，重复按键对话，确认回答播报不再被后台录音跳过；播报结束后再说“小维同学”。当前唤醒依赖云端识别，必须联网。
3. 测试识别时返回、快速松手与失败后重试；观察有无新增 `FT_Load_Glyph 0x55`、`TTS failed`、TLS 异常。若仍失败，保留从开机到首次失败的完整脱敏日志，用本版配对 ELF 分析。

当前未自行烧录、未 commit、未 push。没有实体板 USB 串口连接到本机或 VM，因此尚无本版板端验收结果。
