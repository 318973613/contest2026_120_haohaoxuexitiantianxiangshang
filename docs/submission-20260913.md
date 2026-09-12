# 比赛提交完成确认（2026-09-13）

> 本文记录 2026 首届 openvela AI 硬件开发者大赛参赛作品**首次正式提交**的落地结果。
> 功能验收状态仍以 [`CURRENT_HARDWARE_STATUS.md`](CURRENT_HARDWARE_STATUS.md) 为唯一权威。

---

## 一、结论

**本次提交已完成。** 代码、作品说明 README、AI Coding 日志均已合入专属仓
`open-vela/contest2026_120_haohaoxuexitiantianxiangshang` 的 `dev-ai-contest-2026` 分支。

| 项目 | 值 |
|---|---|
| PR | [#1](https://github.com/open-vela/contest2026_120_haohaoxuexitiantianxiangshang/pull/1) |
| 合入时间 | 2026-09-12T16:32:08Z |
| 提交数 | 6 |
| 改动规模 | 147 files, +39,997 / −146 |
| CLA 检查 | `cla/signature` : success |
| 合入后分支头 | `ead4ecc` |

---

## 二、合入方式说明（重要）

合入后**提交哈希被改写**：

| 本地分支 | 专属仓 |
|---|---|
| `2915fb6` feat: complete native LVGL study terminal… | `ad717b4` |
| `a6b1de5` docs: replace organiser template… | `c4fbe0c` |
| `652c9c6` logs: backfill AI Coding sessions… | `ead4ecc` |

这说明使用的是 **Rebase and merge**。哈希变了但**内容一字未少**——核验方式：

```bash
git diff --stat feat/high-fidelity-study-ui openvela/dev-ai-contest-2026
# 只列出上游自己新增的 .github/ISSUE_TEMPLATE/*（4 个）与 openvela.xml（1 行）
```

**对后续开发的影响**：本地旧分支已与远程分叉，**下次开发要基于远程
`dev-ai-contest-2026` 重新拉分支**，不要接着用 `feat/high-fidelity-study-ui`。

---

## 三、本次提交包含的内容

1. **作品代码**：原生 LVGL C 学习终端（`app/hello_app/`）、5 个自定义 Skill、
   板级适配骨架、`demo/` 脚本等，共 151 个文件。
2. **作品说明 README**：按大赛要求替换掉组委会模板，含作品简介 / 选题方向 /
   目录结构 / 运行方式 / 当前验收状态 / 合规与边界。
3. **AI Coding 日志**：`logs/318973613/`，9 份 Claude Code 会话、4181 个事件，
   由官方 `contest-log-collector` 导出，**内容未做任何修改**。
4. **`.gitignore`**：基于官方 `.gitignore.example` 扩展，**刻意不忽略 `logs/`**。

---

## 四、提交前合规核查（均通过）

| 检查 | 结果 |
|---|---|
| 仓库内是否存在凭据类文件（`config.json`/`*.key`/`*.pem`/`.env*`） | 无 |
| git 历史是否添加过凭据类文件 | 从未 |
| 含模型密钥的板级配置位置 | 在仓库**之外**（`vendor/.../usrdata/ai_agent/config/config.json`） |
| 暂存内容密钥扫描 | `sk-` 2 处（CSS 类名假阳性）、`Bearer`/赋值类/`ssid-psk` 全 0 |
| 日志中真实模型密钥 | **0 命中**（用板级配置里的真实 key 直接比对） |
| 官方 `validate-log.py` 防作弊校验 | ✅ ALL OK（9 文件 / 4181 事件） |
| 是否混入编译产物 | 无 |

### 日志导出中的一次隐私规避

官方导出脚本的 `--backfill` 会扫描 `~/.claude/projects/` 下的**全部项目**。
开发机上还存有 K230、diansai、codex-mobile、gesture-ai、petalinux 等无关项目的
对话，**直接在开发机执行会把它们一并传到公开仓**。

因此改为**在 VM 上导出**：VM 原本没有该目录，只把 `D--openvela` 一个目录传过去，
导出工具只能看到本项目会话，天然隔离。

---

## 五、剩余事项

| 事项 | 状态 |
|---|---|
| 公共仓 `packages_ai_agent` 的 PR | **未做**。该仓为独立 git 仓（`open-vela/packages_ai_agent`），有 29 个已跟踪文件被改 + 22 个未跟踪新文件（含新增 `voice_wake.c`），需先剔除 `.bak-*`/`.orig`/一次性脚本，再 fork 提 PR。密钥扫描已做且干净。 |
| 烧录真机验证 | **部分完成（2026-09-13）**。用户已烧录 `wake-word_20260912` 镜像；`voice_wake_test` **六条用例全部符合预期**（3 正向命中、`你好小米` / `小米同学` / `你好` 均 `no`）。详见 [`wake-word-20260912.md`](wake-word-20260912.md) 第七之二节。**未完成**：端到端唤醒与 AI 问答需要有效 ASR Key。 |
| 唤醒词公共仓改动 | 同上，随 `packages_ai_agent` PR 一并提交。 |
| 作品介绍文档 / ≤5 分钟演示视频 | **未做**，截止前需补。 |

**截止时间：2026-09-20。**

---

## 六、后续一轮：文档同步（2026-09-13）

合入后发现一个**文档一致性问题**：本项目约定「功能验收状态以仓库根
`docs/CURRENT_HARDWARE_STATUS.md` 为唯一权威」，但此前三轮的记录文档只写在了
交付目录 `D:\openvela\docs\`，**没有同步进专属仓**，导致仓库里的权威文档
落后两轮（仍写着唤醒词为「你好小米」）。

本轮已把以下文档同步进仓库 `docs/`，使仓库副本重新成为可信的唯一权威：

| 文档 | 说明 |
|---|---|
| `CURRENT_HARDWARE_STATUS.md` | 补入 9/12 三轮候选（唤醒词合规、工具页视觉统一、离线学习工具）与真机结果 |
| `successful_builds.md` | 补入最近三轮候选的构建/打包/配对记录 |
| `wake-word-20260912.md` | 唤醒词合规统一的完整记录 |
| `ui-consistency-20260912.md` | 工具页视觉统一 + 学习报告 |
| `study-offline-20260912.md` | 离线学习工具候选 |
| `flash-checklist-wake-word-20260913.md` | 烧录测试清单（文件名改为 ASCII，与仓库既有命名一致） |
| `submission-20260913.md` | 本文 |

顺带修正：仓库既有文档全部使用 ASCII 文件名，本轮新增文档沿用该约定
（原 `烧录测试清单_唤醒词版_20260913.md` 已改名为 `flash-checklist-wake-word-20260913.md`）。
