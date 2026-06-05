# STM32C5 UF2 Bootloader 烧录指南

## 硬件环境

- 开发板: NUCLEO-C5A3ZG (STM32C5A3ZGT6)
- 调试器: 板载 ST-LINK V3 (固件 V3J17M10+)
- 连接方式: ST-LINK USB 或 SWD 排针

## 软件要求

- **STM32CubeProgrammer >= 2.18**（2.2.0 已验证可用）
- 旧版本无法识别 STM32C5 系列，报 "Cannot identify the device"

## 已知问题

STM32C5A3 支持 TrustZone，默认有两个调试访问端口：
- AP 0（非安全）：默认不可用
- AP 1（安全）：可用但需新版 Programmer 才能识别

使用 **Normal** 模式连接时会报以下错误：
```
Cannot connect to access port 0
Warning: Connection to AP 0 requested and Failed, Connection established with AP 1
Error: Unable to read device id from ROM table
Error: Cannot identify the device
```

解决方法：更新 STM32CubeProgrammer 到 >= 2.18，使用 **HotPlug** 模式连接。

## 烧录步骤

### 编译固件

```bash
cd tinyuf2
mkdir -p _build_stm32c5 && cd _build_stm32c5
cmake -DBOARD=xiao_c5 -DTOOLCHAIN=gcc -DCMAKE_BUILD_TYPE=Release ../ports/stm32c5
cmake --build .
```

输出文件：
- `_build_stm32c5/tinyuf2.hex` — 烧录用
- `_build_stm32c5/tinyuf2.bin` — 备用
- `_build_stm32c5/tinyuf2.elf` — 调试用

### 方法一：STM32CubeProgrammer GUI

1. 连接板子 ST-LINK USB 到电脑
2. 打开 STM32CubeProgrammer (>= 2.18)
3. 右侧选择 **ST-LINK**、**SWD**
4. **Mode** 选择 **HotPlug**
5. **Frequency** 设为 **4000 KHz**（降低频率更稳定）
6. 点击 **Connect**
7. 确认能识别到 STM32C5A3ZGT6
8. 左侧 **Erasing & Programming** → **Full erase**
9. 点击 **Start** 烧录 `tinyuf2.hex`，起始地址 `0x08000000`
10. 烧录完成后 **Disconnect**，按 **Reset** 复位

### 方法二：STM32CubeProgrammer CLI

```bash
# HotPlug 连接
STM32_Programmer_CLI -c port=swd mode=hotplug

# 全片擦除
STM32_Programmer_CLI -c port=swd mode=hotplug -e all

# 烧录 hex
STM32_Programmer_CLI -c port=swd mode=hotplug \
  -w _build_stm32c5/tinyuf2.hex 0x08000000 -v -rst

# 或烧录 bin
STM32_Programmer_CLI -c port=swd mode=hotplug \
  -w _build_stm32c5/tinyuf2.bin 0x08000000 -v -rst
```

### 方法三：OpenOCD

```bash
openocd -f interface/stlink.cfg -f target/stm32c5x.cfg \
  -c "program _build_stm32c5/tinyuf2.elf verify reset exit"
```

## 验证

烧录完成后，USB 连接电脑，应出现一个 UF2 可移动磁盘，说明 bootloader 运行正常。

## 变砖恢复

STM32 内置 Boot ROM 永远可用，不可能真正变砖。

### SWD 恢复（推荐）

```bash
# Connect Under Reset：按住 Reset → Connect → 松开 Reset
STM32_Programmer_CLI -c port=swd mode=ur -e all
STM32_Programmer_CLI -c port=swd mode=hotplug \
  -w tinyuf2.hex 0x08000000 -v -rst
```

### BOOT0 DFU 恢复

1. 按住 **BOOT0** 按钮
2. 连接 USB 或按一下 Reset
3. 芯片从系统存储器启动，进入 DFU 模式
4. STM32CubeProgrammer 选择 **USB** 端口连接并重新烧录
5. 松开 BOOT0，Reset 后从 Flash 正常启动

### Option Bytes 异常恢复

如果 SWD 完全无法连接（调试端口被 Option Bytes 关闭）：

```bash
# Under Reset 强制连接
STM32_Programmer_CLI -c port=swd mode=ur
# 恢复 Option Bytes 默认值后在 GUI 中操作
```

## 固件信息

| 项目 | 值 |
|------|-----|
| 芯片 | STM32C5A3ZGT6 |
| FLASH 基地址 | 0x08000000 |
| Bootloader 大小 | 32KB (0x08000000 - 0x08007FFF) |
| App 起始地址 | 0x08008000 |
| RAM 基地址 | 0x20000000 |
| RAM 大小 | 256KB - 4 (双击检测预留) |
| 固件大小 | text 28112 + data 808 + bss 9056 |
| Flash 占用 | 28920 / 31744 (91%) |

---

## 将 STM32CubeC5 示例编译为 UF2 固件

以 FDCAN loopback 示例为例，说明如何将任意 STM32CubeC5 示例编译为可通过 UF2 bootloader 拖拽烧录的固件。

### 原理

```
0x08000000 +-------------------+
           | UF2 Bootloader    |  32KB, 通过 ST-LINK 烧录一次
0x08008000 +-------------------+
           | Application       |  通过 UF2 拖拽烧录
           | ...               |
0x08100000 +-------------------+  (1MB Flash end)
```

App 需要将链接脚本中的 Flash 起始地址从 `0x08000000` 改为 `0x08008000`，这样：
- 向量表放在 0x08008000
- SystemInit() 中的 `SCB_SetVTOR()` 自动正确指向新向量表
- Bootloader 检测到有效 App 后会跳转执行

### 步骤一：准备 App 链接脚本

已准备好 `stm32c5a3xg_flash_app.ld`（ROM 从 0x08008000 开始）。

在 PowerShell 中执行：

```powershell
$exampleDir = "D:\workspace\xiao_c5\STM32CubeC5\examples\hal\fdcan\loopback\NUCLEO-C5A3ZG"
$ldDir = "$exampleDir\user_modifiable\Device\STM32C5A3ZGT6"

# 备份原始链接脚本
Copy-Item "$ldDir\stm32c5a3xg_flash.ld" "$ldDir\stm32c5a3xg_flash.ld.orig"

# 用 App 版本替换（编译完成后可恢复）
Copy-Item "$ldDir\stm32c5a3xg_flash_app.ld" "$ldDir\stm32c5a3xg_flash.ld" -Force
```

### 步骤二：编译 App

```powershell
# 设置工具链 PATH
$cmakeDir = "D:\software\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cmake.win32_1.1.101.202603101401\tools\bin"
$gccDir = "D:\software\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
$ninjaDir = "D:\software\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.ninja.win32_1.1.100.202601091506\tools\bin"
$env:PATH = "$cmakeDir;$gccDir;$ninjaDir;$env:PATH"

cd "$exampleDir\cmake"

# 清除旧缓存（必须，因为链接脚本改了）
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# 配置 + 编译
cmake --preset debug_GCC_NUCLEO-C5A3ZG
cmake --build --preset debug_GCC_NUCLEO-C5A3ZG
```

### 步骤三：提取 .bin

```powershell
# 从 ELF 提取 bin
arm-none-eabi-objcopy -O binary `
  "build\debug_GCC_NUCLEO-C5A3ZG\hal_fdcan_loopback.elf" `
  "build\debug_GCC_NUCLEO-C5A3ZG\hal_fdcan_loopback.bin"
```

### 步骤四：转换为 UF2

```powershell
# 使用 bin_to_uf2.py 转换
python D:\workspace\xiao_c5\bootloader\tinyuf2\tools\bin_to_uf2.py `
  "build\debug_GCC_NUCLEO-C5A3ZG\hal_fdcan_loopback.bin" `
  "build\debug_GCC_NUCLEO-C5A3ZG\hal_fdcan_loopback.uf2" `
  0x08008000 `
  0x00c5c5c5
```

### 步骤五：拖拽烧录

1. 用 USB 线连接板子的**用户 USB 口**到电脑
2. 电脑出现 `XIAOC5BOOT` U 盘
3. 将 `hal_fdcan_loopback.uf2` 拖拽到 U 盘
4. 烧录完成后板子自动重启，运行 FDCAN loopback 示例
5. 观察 LED（PA5）：常亮 = 测试通过

### 步骤六：恢复原始链接脚本

```powershell
Copy-Item "$ldDir\stm32c5a3xg_flash.ld.orig" "$ldDir\stm32c5a3xg_flash.ld" -Force
```

### 返回 Bootloader 模式

如果 App 已经烧录并运行，想重新进入 Bootloader：
- **双击 Reset 按钮**（快速按两次），bootloader 检测到 double-tap 会停留在 DFU 模式
- 或按住用户按钮（PC13）的同时按 Reset
