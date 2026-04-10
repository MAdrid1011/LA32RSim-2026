# Difftest 差分测试使用指南

## 概述

LA32RSim-2026 采用 **NEMU 风格的 dlopen difftest**：

- 参考模拟器（LA32R 解释器）编译为共享库 `build/simulator.so`
- 仿真主程序在运行时通过 `dlopen` 动态加载参考库
- 每条指令提交时，将 DUT（被测 CPU）的架构状态与参考对比

**架构优势：**

- 参考库和仿真主程序独立编译，互不依赖
- 参考库可同时编译为独立可执行文件（`build/ref-sim`），便于在没有 CPU RTL 的情况下单独验证测试程序
- 无需在仿真主程序中维护 rename 表，支持任意微架构的 CPU

---

## 工作流程

```
             ┌──────────────────────────────────────────┐
             │          la32r-sim（仿真主程序）           │
             │                                            │
             │  ┌──────────┐    ┌───────────────────────┐│
test.bin ──► │  │ Verilator │    │    DifftestHost        ││
             │  │  (VCPU)  │───►│  (dlopen ABI)          │◄── simulator.so
             │  └──────────┘    └───────────────────────┘│
             │       │                      │             │
             │  每周期读取提交信息         每次提交       │
             │  (io_cmt_0/1_*)             步进参考 1次   │
             │  (io_cmt_rf_0..31)          比较32寄存器   │
             └──────────────────────────────────────────┘
```

---

## Difftest ABI

参考库 `simulator.so` 导出以下函数：

```c
// 初始化参考，加载内存镜像（img_path=NULL 则不加载）
void difftest_init(const char* img_path, uint32_t base_addr);

// 同步内存（TO_REF=true 表示 DUT→参考，false 表示 参考→DUT）
void difftest_memcpy(PMemMap& dut_pmem, bool direction);

// 同步寄存器（buf 布局：uint32_t rf[32], uint32_t pc）
void difftest_regcpy(uint32_t* buf, bool direction);

// 参考前进 n 条指令
void difftest_exec(uint64_t n);

// 注入中断（irq = ESTAT.IS 字段值）
void difftest_raise_intr(uint32_t irq);

// 查询参考当前 PC
uint32_t difftest_get_pc();

// 查询参考第 i 个架构寄存器
uint32_t difftest_get_rf(int i);
```

---

## 每次提交的 Difftest 步骤

对于每个有效的提交槽位（`io_cmt_i_valid=1`）：

1. **PC 检查**：比较参考 PC（`difftest_get_pc()`）与 DUT 提交 PC（`io_cmt_i_pc`）
   - 不一致 → 立即报告 `PC MISMATCH`

2. **参考前进**：调用 `difftest_exec(1)` 让参考执行一条指令

3. **寄存器检查**（`CONFIG_DIFFTEST_FULL_RF=y`）：
   - 比较 DUT 的 `io_cmt_rf_0..31` 与参考的 `difftest_get_rf(0..31)`
   - 发现不一致 → 打印完整的寄存器对比表并停止

---

## 不一致时的错误输出示例

```
[difftest] PC 不一致！
  DUT  PC = 0x1c000018
  REF  PC = 0x1c000014

── DUT vs REF 寄存器对比 (提交PC=0x1c000014) ──
  寄存器        DUT 值         REF 值         状态
  ────────────────────────────────────────────────
  r0/zero     0x00000000     0x00000000     OK
  r1/ra       0x1c000010     0x1c000010     OK
  r4/a0       0x00000037     0x00000036     <<< MISMATCH   ← 红色
  ...

── 最近 32 条指令追踪（ITRACE）──
  周期      PC          指令                       写入值         目标
  ─────────────────────────────────────────────────────────────────
  12345     0x1c000004  addi.w   r4, r4, 1         0x00000001     r4
  12346     0x1c000008  add.w    r4, r4, r5         0x00000037     r4
  ...
```

---

## 使用参考模拟器独立验证测试程序

当 `CONFIG_REF_STANDALONE=y` 时，还会构建 `build/ref-sim`：

```bash
# 编译所有测试程序
make software-all

# 单独运行（不需要 CPU RTL）
./build/ref-sim software/functest/build/add-la32r.bin
./build/ref-sim software/coremark/build/coremark-la32r.bin
./build/ref-sim software/picotest/build/pico-la32r.bin

# 或通过 make 目标
make test-all        # 运行全部4套
make test-functest   # functest（35个用例）
make test-coremark   # CoreMark
make test-dhrystone  # Dhrystone
make test-picotest   # picotest（TLB/MMU/异常）
```

输出示例：

```
[ref-sim] 加载镜像: software/coremark/build/coremark-la32r.bin
[ref-sim] 基地址:   0x1c000000
[ref-sim] 开始执行...
Running CoreMark for 1 iterations
CoreMark Size    : 666
CoreMark PASS       2921 Marks
[ref-sim] 执行完成，共 XXXXXX 步
[ref-sim] HIT GOOD TRAP  (a0 = 0)   ← 绿色
```

**用途：**

1. 验证测试程序本身是否正确（在没有 CPU 的情况下）
2. 获取"黄金参考"的寄存器最终状态
3. 调试测试程序的逻辑

```bash
# 带选项运行
./build/ref-sim test.bin --max 10000000 --quiet
```

---

## 关于 picotest 与 difftest

picotest 涵盖 TLB/MMU/CSR/例外操作（n59–n72），参考模拟器已完整实现：

- **DA/PG 模式切换**（CRMD 寄存器）
- **DMW 直接映射窗口**（DMW0/DMW1）
- **16 项 TLB**（TLBWR/TLBRD/TLBFILL/TLBSRCH/INVTLB）
- **TLB 相关例外**（TLBR/PIL/PIS/PIF/PME/PPI）及正确的 ERTN 恢复
- **CSR 操作**（CSRRD/CSRWR/CSRXCHG，含 PRMD/ESTAT/ERA/BADV/TLBEHI/EENTRY/TLBRENTRY）

**全仿真 difftest 前提**：DUT CPU 同样需要实现上述 TLB/CSR/例外功能，否则
picotest 的 TLB 指令执行时 DUT 与参考的状态将产生差异。

---

## 常见问题

### Q: PC 在复位后立刻不匹配

检查：
- CPU 复位向量是否为 `0x1c000000`（`CONFIG_RESET_VECTOR`）
- 镜像是否加载到正确基地址（`CONFIG_MEM_BASE`）

### Q: 寄存器在第一条指令后就不匹配

检查：
- CPU 的 `io_cmt_rf_*` 是否在 valid 信号有效的**同一周期**更新
- 是否实现了 r0 恒为 0 的约束

### Q: Difftest 通过但程序输出错误

说明 CPU 功能正确，但可能是 UART 输出未接通。
确认 `CONFIG_UART_STDOUT=y`，且测试程序写 UART 地址 `0xa00003f8`（base-port 约定）。

### Q: picotest 在 invtlb 处报 PC 不一致

DUT 可能未实现 TLB 指令。picotest 从 n59 开始进入 TLB 测试区域。
可以先单独测 functest/coremark/dhrystone 验证基础 ALU/访存功能。
