# DShanPi 编译、镜像与烧录链路

更新时间：2026-07-13

本文记录百问网 `r528s3-dshanpi` 的可复现流程，并明确区分“已经验证”和
“仍需真机验证”。不要使用 `r528s3-velaevb1`、`r528s3-evb4` 或其他 R528
开发板的镜像替代 DShanPi 镜像。

## 当前结论

- 官方 BSP：`vendor/allwinnertech/boards/r528/r528s3-dshanpi/`
- BSP 仓库分支：`dev-ai-contest-2026`
- BSP 同步提交：`14bc07bc6ca1dc086fc1fcccba337e7a037e96b6`
- 官方补丁已经应用，共修改 `external/opus/opus` 2 个文件、`nuttx` 8 个文件。
- DShanPi 的 `nsh` 配置已经编译成功，`nuttx.bin` 大小为 9,703,316 字节。
- 官方打包工具 `dragon` 已输出 `Dragon execute image.cfg SUCCESS`。
- 已生成 DShanPi NAND 镜像，尚未烧录到真机。

镜像信息：

```text
路径：vendor/allwinnertech/lichee/out/r528s3/dshanpi_nand/
      rtos_nuttx_r528s3-dshanpi_uart0_256Mnand.img
大小：25,835,520 字节
SHA256：20a302f19380069623fc3563d45805630b231f770495814f156dc8791f1cbdf5
```

`file` 只把全志打包镜像识别为 `data`，这是正常现象，不能据此判断镜像失败。

## 1. 下载和同步源码

首次拉取必须使用组委会提供的 manifest，不能只 clone 专属仓：

```bash
mkdir -p ~/openvela
cd ~/openvela

~/.local/bin/repo init \
  -u https://github.com/open-vela/contest2026_120_haohaoxuexitiantianxiangshang \
  -b dev-ai-contest-2026 \
  -m contest2026_120_haohaoxuexitiantianxiangshang.xml

~/.local/bin/repo sync -c -j8
```

成功标志：

- `vendor/allwinnertech/boards/r528/r528s3-dshanpi/` 存在。
- `contest2026_120_haohaoxuexitiantianxiangshang/` 存在。
- `repo status` 没有同步错误。

## 2. 应用官方 DShanPi 补丁

补丁只应用一次：

```bash
cd ~/openvela
patch -p1 < vendor/allwinnertech/boards/r528/r528s3-dshanpi/dshanpi_for_trunk5.5.patch
```

当前工作区已经应用过该补丁，不要重复执行。补丁会让 `nuttx` 和
`external/opus/opus` 显示修改，这是预期现象。

本次构建还发现 Make 构建漏掉蓝牙连接管理实现，已在
`frameworks/connectivity/bluetooth/Makefile` 做最小修复，使其与同目录 CMake
逻辑一致并加入 `sal_connection_manager.c`。没有该修复时，最终链接会缺少
`bt_sal_cm_*` 符号。

## 3. 编译 DShanPi

在工程根目录执行：

```bash
cd ~/openvela

# 只有切换板级配置或配置严重混乱时才执行 distclean。
./build.sh vendor/allwinnertech/boards/r528/r528s3-dshanpi/configs/nsh/ -j4 distclean

# 正式编译。当前虚拟机已经用 4 线程验证成功。
./build.sh vendor/allwinnertech/boards/r528/r528s3-dshanpi/configs/nsh/ -j4
```

如果只是修改少量代码，直接再次执行第二条命令进行增量编译，不必每次 clean。

成功标志：

```text
nuttx/nuttx
nuttx/nuttx.bin
nuttx/vela_nsh.elf
vendor/allwinnertech/lichee/board/r528s3/dshanpi_nand/configs/nsh.fex
```

其中 `nuttx.bin` 与 `nsh.fex` 应为同一份待打包系统二进制。

## 4. 生成全志烧录镜像

官方 README 要求 Ubuntu 安装 `gcc-multilib` 和 `g++-multilib`。打包工具
`tools/tool/dragon` 是 32 位程序；若出现下面的错误：

```text
tools/tool/dragon: No such file or directory
```

通常不是文件真的丢失，而是 Ubuntu 缺少 `/lib/ld-linux.so.2` 和 i386 运行库。
可由用户在虚拟机中手动输入自己的 sudo 密码安装：

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libc6:i386 libstdc++6:i386 libgcc-s1:i386
```

然后执行官方打包流程：

```bash
cd ~/openvela/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx r528s3-dshanpi
pack
```

成功标志不是只看最后一个 shell 返回码，而是同时确认：

```text
Dragon execute image.cfg SUCCESS
pack finish
out/r528s3/dshanpi_nand/rtos_nuttx_r528s3-dshanpi_uart0_256Mnand.img
```

本次 `pack` 虽然最后返回了 `1`，但 `dragon` 明确报告成功，25 MB 镜像也已
生成并完成 SHA256 校验。日志中还出现 `boot0 checksum fail` 和使用默认
`res` 数据目录的提示，因此该镜像目前定义为“已生成、待真机启动验证”，不能
定义为“已完成硬件验收”。

## 5. 把镜像复制到 Windows

### 原生 LVGL 镜像（2026-07-15）

停止 Quick App 主路线后，DShanPi 已重新启用专属仓原生 `study_terminal`，
关闭 Quick App/UIKit/QuickJS，并完成 ARM LTO 构建、官方 pack 和 dragon 封装。

最终镜像：

```text
D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_native-lvgl_256Mnand.img
```

- 大小：36,743,168 字节
- SHA256：`ad652bfcbb1d5f139a401ffba636dc3298eae44e02bc081fb4e4ff6973fe6303`
- ELF 包含：`study_terminal_main`
- ELF 不包含：`vapp_main`、`xiaozhi_gui_main`
- 镜像包含：`study-terminal.sh`、`study_terminal &`、
  `NotoSansSC-Regular.ttf`
- 镜像未检出：`vapp hap://`、Quick App 包名、`manifest.json`、MiSans 字体
- 状态：已离线核验，尚未烧录和真机验收

烧录前必须重新执行 `Get-FileHash`，确认哈希与上面完全一致。

在 Windows PowerShell 中执行：

```powershell
New-Item -ItemType Directory -Force D:\openvela\firmware

scp openvela@10.28.239.150:/home/openvela/openvela/vendor/allwinnertech/lichee/out/r528s3/dshanpi_nand/rtos_nuttx_r528s3-dshanpi_uart0_256Mnand.img D:\openvela\firmware\
```

虚拟机 IP 由 DHCP 分配，若连接失败，先在 Ubuntu 执行 `hostname -I` 获取新
地址。复制后在 Windows 校验：

```powershell
Get-FileHash D:\openvela\firmware\rtos_nuttx_r528s3-dshanpi_uart0_256Mnand.img -Algorithm SHA256
```

输出应与本文记录的 SHA256 一致。

## 6. Windows 烧录工具

百问网官方资料包中已经确认有：

- `AllwinnertechPhoeniSuit.zip`：USB 线刷工具。
- `AllwinnerUSBFlashDeviceDriver.zip`：全志 USB 烧录驱动。
- `PhoenixCard-V2.8.zip`：TF 卡制卡工具。
- `openvela快速入门与工程实践_v1.2.pdf`：DShanPi 烧录教程。

资料地址：<https://pan.baidu.com/s/1KbpXYZube7jeKfqT9xvLow?pwd=na93>

当前生成的是 `dshanpi_nand` 全志镜像，计划优先按官方 PDF 使用 PhoenixSuit
USB 线刷。不要把网盘里的 `r528s3-velaevb1` 示例镜像烧入 DShanPi。

## 7. 真机烧录检查表

以下流程框架已经明确，但 DShanPi 的具体按键顺序必须先从官方 PDF 核对，
未核对前不执行物理烧录：

1. Windows 安装全志 USB 驱动并重启或重新插拔设备。
2. 以管理员身份启动 PhoenixSuit。
3. 在固件页面选择本文生成的 DShanPi `.img`，再次核对文件名含
   `r528s3-dshanpi` 和 `256Mnand`。
4. 板子断电，按官方 PDF 的按键/USB 顺序进入全志下载模式。
5. 在 Windows 设备管理器确认全志 USB 下载设备被识别。
6. PhoenixSuit 识别设备后开始烧录；不要拔线、关机或让电脑休眠。
7. 等进度达到 100% 且工具明确提示成功，再断开 USB 并重新上电。
8. 连接串口，确认出现启动日志和 `vela>`；再检查屏幕、触摸和
   `xiaozhi_gui`。

烧录前还必须确认两件事：

- PDF 中 DShanPi 进入下载模式的准确按键名称和操作顺序。
- PhoenixSuit 弹出的“格式化/保留数据”选项应该选择哪一个。首次验证通常
  需要完整升级，但必须以百问网教程为准。

## 验证边界

已经验证：源码已同步、官方补丁已应用、DShanPi 编译成功、镜像已生成并校验。

尚未验证：镜像在物理板上的启动、屏幕方向、触摸坐标、USB 下载模式按键顺序、
PhoenixSuit 实际烧录以及烧录后的串口和 GUI。到取得官方 PDF 并核对步骤前，
停止在“镜像已生成”阶段。
