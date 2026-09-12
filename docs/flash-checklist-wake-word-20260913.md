# 烧录测试清单：唤醒词合规版（2026-09-13）

> 适用镜像：`wake-word_20260912`（本轮已烧录）。
> 目的：**在没有有效模型 Key 的前提下**，把能验的都验掉，并明确哪些确实验不了。
> 安全：本镜像含明文预置 Key 与 WiFi 密码，**只在本机测试，不要截图外发、不要上传**。

---

## 1. 先核对烧的是不是这一版

```text
镜像：D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_wake-word_20260912_256Mnand.img
大小：40,841,216 bytes
SHA-256：e7ce47c3983bbe652dba431c52e1a419eafd9f5ec46c1c540618d093f42a52ea

配对 ELF：D:\openvela\archive\nuttx-20260912-wake-word-candidate.elf
大小：68,678,904 bytes
SHA-256：4e08ff20e6f4efb690547b7c0f025314011a13164e645e1d911d172de0478881
```

真机崩溃时用这个 ELF 做 `addr2line`，**不要和别的版本混用**。

---

## 2. 结论先说：哪些能测、哪些不能

| 能力 | 需要模型吗 | 现在能测吗 |
|---|---|---|
| 五个页面 / 触摸 / 中文字体 | 不需要 | ✅ 能 |
| 离线学习工具（提醒、提示音、稍后提醒、专注目标、七日记录、学习报告） | 不需要 | ✅ 能 |
| 系统状态（WiFi / 存储 / 内存） | 不需要 | ✅ 能 |
| **唤醒词匹配逻辑**（本轮改动的核心） | **不需要** | ✅ **能**（见第 4 节） |
| 语音唤醒**端到端**（对着麦克风喊） | **需要** | ❌ 不能 |
| AI 问答 | 需要 | ❌ 不能 |

**你的判断是对的**：唤醒词走的是在线 ASR，Key 过期就喊不醒。但**"喊不醒"不等于"改错了"**——
本轮改的是**短语匹配规则**，那个规则有离线命令可以直接验。

---

## 3. 立刻能做的验证（不需要模型）

1. **开机 + 屏幕**：五页 UI 正常显示，中文无方框，触摸可用。
2. **系统状态页**：能读到 WiFi、存储、内存等真实数值（不显示"处理中"卡住）。
3. **离线学习工具**（这一轮的重点功能，全部本地）：
   - 提醒中心：设一个 10 秒后的提醒，到点屏幕/日志出现提醒；
   - 离线提示音：断网状态下仍有声音反馈；
   - 五分钟稍后提醒：一键顺延生效；
   - 专注目标：设定后计时与完成度正确；
   - 七日学习记录 / 学习报告：有按天汇总的数据。
4. **断网测试**：把 WiFi 关掉，确认上面这些功能**仍然可用**——这正是本作品的核心主张。

---

## 4. 唤醒词逻辑的离线验证（关键，不需要模型）

串口进到 `vela>` 提示符（`nsh>` 下执行 `ai_agent`；若已在 `vela>` 则直接用），
逐条执行下面命令。命令会直接打印 `Wake match: yes` 或 `Wake match: no`。

```text
voice_wake_test 你好，openvela
voice_wake_test 你好 openvela
voice_wake_test Hello，OpenVela
voice_wake_test 你好小米
voice_wake_test 小米同学
voice_wake_test 你好
```

**期望结果：**

| 命令 | 期望输出 | 说明 | 实测 |
|---|---|---|---|
| `voice_wake_test 你好，openvela` | `Wake match: yes` | 官方规定唤醒词 | ✅ **已验，yes** |
| `voice_wake_test 你好 openvela` | `Wake match: yes` | 分隔符被折叠 | 待测 |
| `voice_wake_test Hello，OpenVela` | `Wake match: yes` | ASCII 大小写被折叠 | 待测 |
| `voice_wake_test 你好小米` | **`Wake match: no`** | 旧的品牌唤醒词已移除 | 待测 |
| `voice_wake_test 小米同学` | **`Wake match: no`** | 同上 | 待测 |
| `voice_wake_test 你好` | `Wake match: no` | 不误触发 | 待测 |

> 2026-09-13 串口实测：`voice_wake_test 你好，openvela` → **`Wake match: yes`**。
> 剩下五条（尤其三条 **no**）仍需补测——**负例才是合规的关键证据**。

**测试时请在干净的 `vela>` 提示符下逐条执行。** 本次日志是在 `recording...`
期间输入的，命令与提示符输出交错了（`Voice: recording... (usvoice_wake_test ...`），
结果虽然有效，但下次避免这样，免得误判。

**注意**：串口单行长度上限是 64 字符（`CONFIG_NSH_LINELEN=64`），上面的命令都在范围内，
但不要再往里加别的内容。

**已知行为**：历史中性别名 `小维同学` / `小维` / `学习助手` 仍然被接受
（`voice_wake_test 小维同学` 会返回 yes）。这是刻意保留的兼容行为，
不是品牌唤醒词。如果你希望一并去掉，告诉我，需要重新编译一版。

---

## 4.5 关于串口日志里的「System clock not set」——不是故障

你可能会看到这行：

```text
[vela_tls] System clock not set (UNIX=1822); NTP has not synced
```

**它只是提示，不是 TLS 失败的原因。** `vela_tls.c` 里明确注释：

> Nothing here needs a valid clock: authmode is VERIFY_OPTIONAL, so
> certificate validity windows do not gate this handshake.

也就是说证书有效期**不参与**这次握手。历史上"开机把时间硬设成 2026-02-28"的写法
已经被**故意删除**（它会掩盖 `ntpc` 失败、让界面顶栏显示假时间、还让所有调试会话
时间戳雷同）。**排查联网问题时不要走时钟这条线**，根因在凭据。

---

## 5. 想测完整语音？不用重新烧录就能换 Key

镜像里的 Key 已过期，但**Key 是运行时可改的**，不需要重烧：

```text
vela> router_status                    # 先看当前后端状态
vela> router_set mimo <新的api_key>    # 写入新 Key（在本机手动输入，不要发给任何人）
vela> router_status                    # 确认后端已生效
vela> ask 你好，请介绍一下你自己        # 验证 AI 问答
```

`router_set` 也支持其他后端（`deepseek` / `kimi` / `qwen` / `openai` 等），
所以**不是非得用 MiMo**，你手上有任意一家可用的 Key 就能把对话和唤醒跑通。

跑通后再验端到端语音：

```text
vela> voice_wake_status                # 看唤醒监听状态
vela> voice_wake_start                 # 启动唤醒监听
# 对着麦克风说：你好，openvela
vela> voice_wake_stop
```

> **安全**：Key 只在本机手动输入，不要贴到聊天里、不要提交到仓库、不要截图。
> 仓库与日志都做过密钥扫描，目前是干净的，别从这里破坏它。

---

## 6. 测试后回报什么

按第 3、4 节逐项写「通过 / 失败」，失败项附上串口关键日志（**去掉 IP、MAC、Key**）。
崩溃时给 PC 地址，我用配对 ELF 解析。

---

## 7. 顺带说明：本轮代码已提交

代码 + 作品说明 README + AI Coding 日志已合入专属仓（PR #1，`dev-ai-contest-2026`）。
本次烧录的镜像与该提交同源。完整记录见 [`submission-20260913.md`](submission-20260913.md)。
