# 语音、触摸与开机音质候选记录（2026-09-05）

本次交付一个候选镜像，包含触摸队列修补、异步录音/识别、TTS 栈防护与握手诊断，以及开机音频衰减。构建和主机故障注入通过；没有把这些结果写成真机修复通过。镜像保留用户授权预置的凭据，仅供本地真机使用，不可外发。

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


## 改动与证据

- 专属仓 `app/hello_app/voice_ui_bridge.c/.h` 和 `study_terminal_main.c`：按下、松手与关闭页面只提交请求，单一后台线程负责麦克风启停及同步 ASR。UI 每 100 ms 取带请求编号的结果；快速松手不会漏掉停止请求，关闭页后丢弃过时结果，失败后可以再试。后台不调用 LVGL。初始化期间的就绪查询也不会等待后台锁。
- 公共 `packages/ai_agent/src/voice/voice_channel.c`：`stop_with_text()` 返回真实 ASR 错误码，将完成录音的 PCM 缓冲转为局部独占所有权，避免其他入口开始新录音后误释放新缓冲。
- 公共 `nuttx/drivers/input/ft5x06.c`：非阻塞队列满时丢最旧事件、保留最新事件，避免丢最终 TOUCH_UP；允许调用方传入大于一条报告的缓冲；工作排队失败后恢复 IRQ；增加限频的队列溢出/I²C 错误日志。
- 公共 `agent_config.h` 的 outbound 栈、专属 UI TTS 线程和新 ASR 线程均为 32 KB。`vela_tls.c` 增加实际调用线程/栈大小、握手状态/长度/受边界保护的前 5 字节，以及写失败错误码日志。应用数据、HTTP 请求头和凭据不进入这些新增日志。**栈溢出仍是待验证假设**，不能把提高栈写成已解释 `-0x7080`。
- 板级音频另存 `boot_intro_soft.pcm`，从原 WAV 的 data chunk 解析后衰减 6 dB。仍是 24 kHz、mono、S16_LE、599,040 bytes、12.48 秒；峰值由 27,720（−1.45 dBFS）变为 13,893（−7.45 dBFS）。原 WAV/PCM 完整保留，采样率和 codec 寄存器未改变。衰减是降低输出过载风险的候选处理，清晰度需要真机听验。
- 新增专属仓 `demo/scripts/prepare_boot_audio.py`，按 WAV chunk 解析并生成可复现的候选 PCM，不调用收费 TTS 服务。启动入口仍是 `nxplayer < /data/audio/bootvoice.cmd &`，命令文件仍保留 `volume 100`、`!sleep 14` 和 `q`，只将播放路径换为候选 PCM。

## 本次排除或修正的判断

- FT5X06 在 I²C2；codec 是片内 MMIO。没有“两个设备共用同一 I²C 控制器”的依据。触摸 PE0/PE1、PA PD17、DMIC PD20/PD19/PB18 也未发现冲突；没有证据时不修改引脚或控制器寄存器。
- 队列原本就是 O_NONBLOCK，满队列会丢消息，并不会因此把 HPWORK 阻塞在发送上。同步 ASR 可以造成 UI 暂停，但不能单独解释对话结束后“FPS 仍变化而所有触摸持续失效”；后者仍需板端分层取证。
- chat 容器高度是 128/150；旧交接的“2745”是行号，不能当作高度证据。
- 24 kHz 在 getcaps、白名单和 codec 时钟表里都被支持。原 WAV data 与 PCM 逐字节一致，没有发现 RIFF/LIST 等头残留、奇数字节错位或接近满幅的削波样本。固定切掉 44 字节的旧生成脚本有健壮性隐患，但不能据此声称当前文件已损坏。
- `mediad` 的 graph 首个 `amovie_async` filter 不在当前 FFmpeg 注册表中，初始化在打开音频设备前失败。graph 里的 44.1 kHz 不代表实际 nxplayer 采样率，也没有 mediad 已占用 codec 的证据。本次保留 nxplayer 回退，未扩大到 graph 重写。
- TTS HTTPS 同步运行在调用者栈上。专属 UI 通过消息 tap 接收回复，再由其自己的 TTS worker 调用，并非所有 TTS 都走 outbound。实际 mbedTLS 的 `-0x7080` 可达点是跨 record 的握手分片不支持，或 ServerHello 压缩不是 NULL；待新日志区分，未改证书校验策略或伪造时钟。
- `tls_read_response()` 现有返回路径已释放 raw 缓冲，“确定漏释放导致永久挂起”不成立。ALSA 避免重复 prepare 的修复已存在于 bootvoice2 配对内核，不能再按“尚未改源码”处理；连续采集和重复 ASR 仍需复验。

## 验证范围

1. Ubuntu 主机编译实际 bridge C 文件，ASan/UBSan 下 13 个故障注入场景通过：慢初始化、线程创建失败、快速/重复松手、慢 ASR 时取消、旧请求、失败及空结果后的重试、兼容入口串行化、TTS 独立线程。主机请求/快照最大耗时 0.607 ms，仅代表该测试环境。bridge 对象没有 LVGL 引用；UI 调用点静态检查通过，未把它算作真实触摸测试。
2. 从实际源码提取 FT5X06、TLS 日志和 ASR 收尾函数，ASan/UBSan 下验证队列溢出后最终松手、大缓冲读取、读失败/中断、IRQ 恢复、固定/可变 TLS 缓冲边界及 PCM 所有权。WAV 带奇数长度 LIST chunk 的解析与有符号衰减检查通过。
3. 273 个 VM 源码基线 SHA 校验通过后备份并同步 10 个变更文件。DShanPi make 构建/链接退出 0，没有编译错误或 undefined reference；保留原 BSP/蓝牙/NAND 等既有警告，未宣称无警告。
4. 发现仅清 `.built` 不足以更新 `agent_main.c` 的缓存入口对象；已备份并移走两个对应对象、重新编译入口后再链接。最终 build.log 确认入口重新编译。首次未更新入口的中间产物不交付。同步脚本已补充这项处理，未手删 `libapps.a`。
5. 按板级 Make.defs 的 `objcopy -R .note.gnu.build-id -O binary` 从本次 ELF 导出，全部 11,830,596 bytes 与 nuttx.bin、nsh.fex 相同。默认 objcopy 会额外导出远地址 build-id 段，这解释了最初的长度差异。
6. 官方 pack 有 `Dragon execute image.cfg SUCCESS` 与 `pack finish`，新镜像时间戳成立。外层返回 1 的已知假失败不作为唯一结论。新 stdin 启动行/候选 playraw 各出现一次，旧坏 argv 启动行和旧 playraw 均为零。
7. 从 YAFFS 的 2048-byte 页/16-byte in-band tag 中重组启动脚本、命令文件、原始 WAV/PCM 和候选 PCM，均与源文件逐字节一致。完整 18,165,760-byte userdata 和配对内核都在最终镜像中。启动脚本最长 59 字符；命令文件最长 52 字符，均小于 64。Windows 与 VM 的镜像/ELF SHA 再次一致。

日志与可复现检查位于 `D:/openvela/_work/voice-touch-20260905/`：`candidate.patch`、`candidate-manifest.json`、`tests/`、`ptt-test-results.log`、`build.log`、`pack.log`、`pair-verification.json`、`payload-verification.json` 和 `audio-report.json`。

## 真机待验

1. 冷启动检查自我介绍听感与串口输入；和保留的原 PCM 比较音量与清晰度。
2. 连续完成至少三轮按住说话/回复，随后切换其他页面；再验证快速松手、识别时返回和失败后重试。
3. 若触摸再失效，用新 FT5X06 日志及原始输入事件区分驱动和 UI；记录 TTS 的 `TLS caller` 与 `hs fail` 行判断 `-0x7080`。采集日志前须脱敏。

本次未 commit、未 push、未烧录。天气、时钟、唤醒词及 mediad 图配置保持各自未验收状态。
