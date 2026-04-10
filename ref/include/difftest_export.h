#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>

// ── Difftest ABI（NEMU 风格）────────────────────────────────────────────────
// 这些符号由 simulator.so 导出，由仿真主程序通过 dlopen/dlsym 加载。

// 内存复制方向
#define DIFFTEST_TO_REF  true
#define DIFFTEST_TO_DUT  false

using PMemMap = std::unordered_map<uint32_t, uint32_t>;

#define __EXPORT extern "C" __attribute__((visibility("default")))

// 初始化参考模拟器，加载内存镜像
// img_path: 二进制镜像路径（可选，NULL 则不加载）
// base_addr: 镜像加载基地址
__EXPORT void difftest_init(const char* img_path, uint32_t base_addr);

// 同步内存：在 DUT pmem 和参考 pmem 之间复制
// direction = DIFFTEST_TO_REF: DUT → 参考
// direction = DIFFTEST_TO_DUT: 参考 → DUT
__EXPORT void difftest_memcpy(PMemMap& dut_pmem, bool direction);

// 同步寄存器（32个架构寄存器 + pc）
// buf 布局：uint32_t rf[32], uint32_t pc
// direction = DIFFTEST_TO_REF: DUT → 参考
// direction = DIFFTEST_TO_DUT: 参考 → DUT
__EXPORT void difftest_regcpy(uint32_t* buf, bool direction);

// 参考执行 n 条指令
__EXPORT void difftest_exec(uint64_t n);

// 参考触发中断（irq = ESTAT.IS 字段值）
__EXPORT void difftest_raise_intr(uint32_t irq);

// 读取参考当前 PC
__EXPORT uint32_t difftest_get_pc();

// 读取参考第 i 个架构寄存器
__EXPORT uint32_t difftest_get_rf(int i);

// ── TLBFILL 索引同步 ──────────────────────────────────────────────────────────
// 在 DUT 的 tlbfill 指令提交后、difftest_exec() 之前调用。
// idx 为 CPU 实际写入的 TLB 槽位号；参考模拟器在执行 tlbfill 时将使用
// 相同索引，而非内部伪随机计数器，从而保持 TLB 状态一致。
// 仅在 CONFIG_DIFFTEST_TLBFILL_SYNC 开启时有意义（其他情况调用也无害）。
__EXPORT void difftest_tlbfill_index_sync(uint32_t idx);
