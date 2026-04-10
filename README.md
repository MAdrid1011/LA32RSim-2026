# LA32RSim-2026

基于 Verilator 的**龙芯架构32位精简版（LA32R）**仿真测试框架。

## 核心特性

- **Difftest 差分测试**：NEMU 风格 dlopen 差分测试，内置完整 LA32R 参考解释器
- **零 rename 依赖**：CPU 直接输出全部32个架构寄存器，支持任意发射宽度、任意微架构（顺序/乱序/超标量）
- **双内存接口**：AXI4 和简单单周期总线可选，Kconfig 一键切换
- **完整 MMU/TLB 支持**：参考解释器实现 DA/PG 模式、DMW、16 项 TLB，覆盖全部 TLB 相关例外
- **参考器可独立执行**：`build/ref-sim` 无需 CPU RTL 即可运行所有测试程序
- **Kconfig 配置**：`make menuconfig` 图形界面控制所有功能开关
- **完整调试支持**：ITRACE 环形缓冲区、MTRACE 内存追踪、MEM_WTRACE 字节级写入时间戳、VCD 波形
- **CPU 完全解耦**：用户将 Verilog 代码放到 `rtl/` 目录即可

## 快速开始

### 1. 环境依赖

```bash
# Ubuntu/Debian
sudo apt-get install verilator python3-pip

# Kconfig 配置工具
pip3 install kconfiglib
```

龙芯交叉编译工具链（编译测试软件用）：

```
/opt/loongson-gnu-toolchain-8.3-x86_64-loongarch32r-linux-gnusf-v2.0/bin/
```

### 2. 配置

```bash
cd LA32RSim-2026
make menuconfig   # 图形化配置（推荐）
```

### 3. 使用参考模拟器验证测试程序（无需 CPU RTL）

```bash
# 编译并运行全部4套测试
make test-all

# 或单独运行某套
make test-functest    # 35 个功能单元测试
make test-coremark    # CoreMark 基准测试
make test-dhrystone   # Dhrystone 基准测试
make test-picotest    # TLB/MMU/异常测试（14 个用例）
```

### 4. 全仿真（需要 CPU RTL）

将被测 CPU 的 Verilog 代码放入 `rtl/` 目录，顶层模块名为 `CPU`。

**使用 Zircon（本框架已验证的参考 CPU）：**

```bash
# Zircon 是框架配套的 LA32R 四发射乱序处理器（Chisel 实现）
git clone -b la32rsim-2026 https://github.com/MAdrid1011/Zircon.git
cd Zircon
make verilog           # 使用 sbt 生成 Verilog（需要 Java + sbt）
cp verilog/*.sv ../rtl/
cp verilog/*.f  ../rtl/
cd ..
```

**使用其他 CPU：**

```bash
cp path/to/your/CPU.sv rtl/          # 顶层模块必须命名为 CPU
cp path/to/other_modules.sv rtl/     # 所有依赖文件一起放入

# 构建仿真器（Verilator 编译 + 链接参考库）
make all

# 运行单个测试（含 difftest）
make run IMG=software/functest/build/add-la32r.bin

# 运行全套测试（含 difftest）
make sim-all

# 生成波形
make wave IMG=software/picotest/build/pico-la32r.bin
```

接口规范见 [`docs/接口规范.md`](docs/接口规范.md)。

## 项目结构

```
LA32RSim-2026/
├── Kconfig                    # 配置定义
├── Makefile                   # 构建入口（make help 查看所有目标）
├── rtl/                       # ← 在此放置 CPU Verilog 代码
├── sim/                       # Verilator 仿真主程序
│   ├── include/               # 头文件
│   └── src/                   # C++ 源文件
├── ref/                       # 参考 LA32R 解释器（含完整 TLB/MMU）
│   ├── include/
│   └── src/
├── software/                  # 测试程序（4套）
│   ├── base/                  # 链接脚本 + 通用启动代码
│   ├── base-port/             # 基础运行时（CRT、UART、链接脚本）
│   ├── functest/              # 35 个功能单元测试（算术/分支/访存）
│   ├── coremark/              # CoreMark 嵌入式基准测试
│   ├── dhrystone/             # Dhrystone 2.2 基准测试
│   └── picotest/              # TLB/MMU/CSR/异常测试（14 个用例）
├── docs/                      # 中文文档
│   ├── 接口规范.md            # CPU 接口（Verilog + Chisel 两版本）
│   ├── Difftest使用指南.md
│   ├── Kconfig配置说明.md
│   └── 调试功能说明.md
└── scripts/                   # 构建辅助脚本
```

## 测试套件说明

| 测试套件 | 用例数 | 覆盖范围 |
|---------|-------|---------|
| functest | 35 | 基础整数指令、分支、访存 |
| coremark | 1 | 嵌入式基准（链表、矩阵、状态机） |
| dhrystone | 1 | Dhrystone 2.2 综合基准 |
| picotest | 14 | TLBWR/TLBRD/TLBFILL/TLBSRCH/INVTLB（n59–n70）、TLB 例外（n71）、DMW 测试（n72） |

参考模拟器当前通过状态：

```
functest : 35/35 通过
coremark : GOOD TRAP
dhrystone: GOOD TRAP
picotest : GOOD TRAP（s3=14，全部14项）
```

## CPU 接口概述

被测 CPU 需要提供以下接口（详见 [`docs/接口规范.md`](docs/接口规范.md)）：

| 信号组 | 说明 |
|--------|------|
| `clock`, `reset` | 时钟与同步高电平复位 |
| `io_axi_*` | AXI4 内存接口（或简单总线 `io_mem_*`） |
| `io_cmt_N_valid/pc/inst/rd_valid/rd/exception` | NCOMMIT 个提交端口 |
| `io_cmt_rf_0..31` | 全部32个架构寄存器当前值（CPU 自维护映射后输出） |

## 测试程序约定

程序退出时通过写寄存器 `a0` 并执行魔法字 `.word 0x80000000` 通知仿真器：

```asm
# a0 = 0  → GOOD TRAP（测试通过）
# a0 ≠ 0  → BAD TRAP（测试失败，a0 为错误码）
move   $r4, <exit_code>
.word  0x80000000
```

> **注意**：这与 LA32R 的 `break` 指令（`0x002a0000`）不同，
> `0x80000000` 是框架专用的仿真退出魔法字，不对应任何 LA32R 标准指令。

## 文档

| 文档 | 内容 |
|------|------|
| [接口规范](docs/接口规范.md) | CPU 顶层接口定义（Verilog + Chisel 两版本）、时序图、实现指南 |
| [Difftest 使用指南](docs/Difftest使用指南.md) | Difftest 工作原理、ABI 说明、错误排查 |
| [Kconfig 配置说明](docs/Kconfig配置说明.md) | 所有配置项详解、常用组合 |
| [调试功能说明](docs/调试功能说明.md) | ITRACE、MTRACE、MEM_WTRACE、波形调试流程 |
