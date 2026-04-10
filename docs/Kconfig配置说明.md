# Kconfig 配置说明

## 使用方式

```bash
make menuconfig   # 打开图形化配置界面
make all          # 按当前配置构建
```

配置存储在项目根目录的 `.config` 文件中，构建时自动生成 `include/generated/config.h`。

---

## 配置项详解

### 目标 CPU 设置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_NCOMMIT` | int | 2 | 每周期最多提交指令数。支持 1/2/4，必须与 CPU 顶层接口 `io_cmt_N_*` 的数量一致 |
| `CONFIG_RESET_VECTOR` | hex | `0x1c000000` | CPU 上电 PC 初始值，LA32R 标准为 `0x1c000000` |

### 内存接口

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_MEM_AXI` | bool | y | 使用 AXI4 内存接口 |
| `CONFIG_MEM_SIMPLE` | bool | n | 使用简单单周期总线 |
| `CONFIG_MEM_AXI_BACKPRESSURE` | bool | n | AXI 随机背压，压力测试握手逻辑 |
| `CONFIG_MEM_SIZE_MB` | int | 128 | 仿真物理内存大小（MB） |
| `CONFIG_MEM_BASE` | hex | `0x1c000000` | 物理内存基地址 |

### Difftest 差分测试

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_DIFFTEST` | bool | y | 启用差分测试 |
| `CONFIG_DIFFTEST_FULL_RF` | bool | y | 每次提交比较全部32个架构寄存器（推荐开启） |
| `CONFIG_REF_STANDALONE` | bool | y | 同时构建独立参考可执行文件 `build/ref-sim` |

### 调试与追踪

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_ITRACE` | bool | y | 指令执行追踪（最近 N 条环形缓冲区） |
| `CONFIG_ITRACE_SIZE` | int | 32 | ITRACE 缓冲区大小（条数） |
| `CONFIG_MTRACE` | bool | n | 内存访问追踪（每次读写打印日志） |
| `CONFIG_MEM_WTRACE` | bool | y | **内存写入时间戳追踪**（字节粒度） |
| `CONFIG_DUMP_WAVE` | bool | n | 生成 VCD 波形文件 |
| `CONFIG_WAVE_BEGIN` | int | 0 | 开始录制波形的周期号（0 = 从头开始） |
| `CONFIG_WAVE_END` | int | 0 | 停止录制波形的周期号（0 = 不限制） |
| `CONFIG_DEBUG_COLORS` | bool | y | 彩色终端输出（ANSI 转义码） |
| `CONFIG_UART_STDOUT` | bool | y | MMIO UART 写操作输出到 stdout |

### 仿真控制

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_MAX_STEPS` | int | 0 | 最大仿真周期数（0 = 无限制） |
| `CONFIG_STALL_TIMEOUT` | int | 5000 | 连续无提交超时周期数（0 = 不检测） |

---

## 常用配置组合

### 快速验证（无 difftest，最快速度）

```
CONFIG_DIFFTEST=n
CONFIG_DUMP_WAVE=n
CONFIG_ITRACE=y
CONFIG_MEM_WTRACE=n
CONFIG_MTRACE=n
CONFIG_MEM_AXI_BACKPRESSURE=n
```

### 完整验证（推荐）

```
CONFIG_DIFFTEST=y
CONFIG_DIFFTEST_FULL_RF=y
CONFIG_ITRACE=y
CONFIG_ITRACE_SIZE=64
CONFIG_MEM_WTRACE=y
CONFIG_DUMP_WAVE=n
```

### AXI 压力测试

```
CONFIG_DIFFTEST=y
CONFIG_MEM_AXI=y
CONFIG_MEM_AXI_BACKPRESSURE=y
CONFIG_STALL_TIMEOUT=20000
```

### 波形调试（定位某段周期）

```
CONFIG_DUMP_WAVE=y
CONFIG_WAVE_BEGIN=10000
CONFIG_WAVE_END=11000
CONFIG_ITRACE=y
CONFIG_MTRACE=y
```

### 参考模拟器独立运行

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `CONFIG_REF_STANDALONE` | bool | y | 同时构建独立参考可执行文件 `build/ref-sim` |

`build/ref-sim` 无需 CPU RTL 即可运行测试程序，用于：

- 快速验证测试程序本身的正确性
- 获取参考寄存器状态用于比对
- 在准备 RTL 之前就能验证4套测试套件全部通过

```bash
make ref             # 仅构建 ref-sim（无需 Verilator）
make test-all        # 运行全部4套测试（ref-sim）
make test-picotest   # 单独运行 picotest（TLB/MMU）
```

---

## 注意事项

- `CONFIG_MEM_AXI` 和 `CONFIG_MEM_SIMPLE` 互斥，只能选一个
- 修改 `CONFIG_NCOMMIT` 后必须重新运行 Verilator 编译（`make clean && make all`）
- `CONFIG_MEM_WTRACE` 会增加内存占用（每个字节一个 `uint64_t` 时间戳），大程序建议关闭
- 波形文件 `build/waveform.vcd` 可能很大，建议用 `WAVE_BEGIN/WAVE_END` 限制范围
