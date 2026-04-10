#pragma once
#include <cstdint>
#include "common.h"
#include "Memory.h"
#include "Difftest.h"
#include "Debug.h"

class VCPU;

// 仿真退出码
enum SimResult {
    SIM_GOOD_TRAP  =  0,   // 命中 GOOD TRAP (a0=0)
    SIM_BAD_TRAP   = -1,   // 命中 BAD TRAP  (a0≠0)
    SIM_DIFFTEST_FAIL = -2,// Difftest 不一致
    SIM_STALL      = -3,   // 死锁（超时无提交）
    SIM_MAX_STEPS  = -4,   // 达到最大步数
};

class Emulator {
public:
    Emulator(VCPU* cpu, MemoryBase* mem);
    ~Emulator() = default;

#ifdef CONFIG_DIFFTEST
    void initDifftest(const char* so_path, const char* img_path, uint32_t base_addr);
#endif

    // 运行仿真；n=0 表示无限制
    SimResult run(uint64_t max_steps = 0);

    uint64_t getCycles() const { return cycles; }
    uint64_t getInsts()  const { return insts;  }

private:
    VCPU*        cpu;
    MemoryBase*  mem;
#ifdef CONFIG_DIFFTEST
    DifftestHost difftest;
#endif
    uint64_t cycles = 0;
    uint64_t insts  = 0;
    uint64_t stall_cnt = 0;

    // 复位 DUT
    void reset(int cycles = 5);

    // 单周期步进（含内存服务）
    void tick();

    // 读取 DUT 架构寄存器堆（由 DUT 的 io_cmt_rf_* 信号）
    void readDutRF(uint32_t* rf_out);

    // 处理一个提交槽位；返回 true 表示需要退出
    // 参数：commit_idx (0..NCOMMIT-1), rf = DUT 全量寄存器
    SimResult handleCommit(int idx, const uint32_t* rf);

    // 检测 GOOD/BAD TRAP 指令编码
    static bool isBreak0(uint32_t inst) { return inst == 0x80000000u; }

#ifdef CONFIG_DUMP_WAVE
    VerilatedVcdC* trace = nullptr;
#endif
};
