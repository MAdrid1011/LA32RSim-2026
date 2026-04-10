#pragma once
#include <cstdint>
#include <unordered_map>
#include "common.h"
#include "Memory.h"

#ifdef CONFIG_DIFFTEST

// difftest ABI 函数指针类型
using PMemMap = std::unordered_map<uint32_t, uint32_t>;

typedef void     (*fn_difftest_init)              (const char*, uint32_t);
typedef void     (*fn_difftest_memcpy)             (PMemMap&, bool);
typedef void     (*fn_difftest_regcpy)             (uint32_t*, bool);
typedef void     (*fn_difftest_exec)               (uint64_t);
typedef void     (*fn_difftest_raise_intr)         (uint32_t);
typedef uint32_t (*fn_difftest_get_pc)             ();
typedef uint32_t (*fn_difftest_get_rf)             (int);
typedef void     (*fn_difftest_tlbfill_index_sync) (uint32_t);

// 寄存器快照缓冲区大小（rf[0..31] + pc = 33个 uint32_t）
#define DIFFTEST_REG_BUF_SIZE 33

// LA32R 寄存器名称
static const char* const LA32R_REG_NAMES[32] = {
    "r0/zero","r1/ra  ","r2/tp  ","r3/sp  ","r4/a0  ","r5/a1  ",
    "r6/a2  ","r7/a3  ","r8/a4  ","r9/a5  ","r10/a6 ","r11/a7 ",
    "r12/t0 ","r13/t1 ","r14/t2 ","r15/t3 ","r16/t4 ","r17/t5 ",
    "r18/t6 ","r19/t7 ","r20/t8 ","r21/rsv","r22/fp ","r23/s0 ",
    "r24/s1 ","r25/s2 ","r26/s3 ","r27/s4 ","r28/s5 ","r29/s6 ",
    "r30/s7 ","r31/s8 "
};

class DifftestHost {
public:
    DifftestHost() = default;
    ~DifftestHost();

    // 加载参考库并初始化
    void init(const char* so_path, const char* img_path,
              uint32_t base_addr, MemoryBase* mem);

    // 每次 commit 后调用：
    // dut_rf[0..31] = DUT 架构寄存器当前值，dut_pc = 提交指令 PC
    // 执行 n 步参考后比较
    // 返回 true 表示一致，false 表示不一致（已打印差异）
    bool step(const uint32_t* dut_rf, uint32_t dut_pc, uint32_t n,
              MemoryBase* mem);

    // 注入中断（DUT 发生中断时调用）
    void raiseIntr(uint32_t irq);

    // 在 DUT tlbfill 提交后、step() 之前调用，通知参考模拟器使用相同 TLB 槽位
    // 若参考库不支持此接口（旧版 simulator.so），调用无效果
    void tlbfillSync(uint32_t idx);

    bool isLoaded() const { return handle != nullptr; }

private:
    void*  handle = nullptr;

    fn_difftest_init              f_init       = nullptr;
    fn_difftest_memcpy            f_memcpy     = nullptr;
    fn_difftest_regcpy            f_regcpy     = nullptr;
    fn_difftest_exec              f_exec       = nullptr;
    fn_difftest_raise_intr        f_intr       = nullptr;
    fn_difftest_get_pc            f_get_pc     = nullptr;
    fn_difftest_get_rf            f_get_rf     = nullptr;
    fn_difftest_tlbfill_index_sync f_tlbfill_sync = nullptr; // 可选，旧 .so 可能无此符号

    // 打印寄存器差异
    void printMismatch(const uint32_t* dut_rf, uint32_t dut_pc) const;
};

#endif // CONFIG_DIFFTEST
