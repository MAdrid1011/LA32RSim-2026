#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include "CSR.h"
#include "LA32R_ISA.h"

// 仿真状态
enum SimState {
    SIM_RUNNING = 0,
    SIM_END,        // 命中 GOOD/BAD TRAP
    SIM_ABORT,      // 内部错误
};

// 统一寄存器快照（用于 difftest ABI 传输）
struct CpuState {
    uint32_t rf[32];
    uint32_t pc;
};

// 物理内存：稀疏 word 映射
using PMemMap = std::unordered_map<uint32_t, uint32_t>;

// LA32R TLB 表项（LA32R 架构手册第 6 章）
struct TLBEntry {
    bool     e    = false; // 表项有效
    bool     g    = false; // 全局位（忽略 ASID 匹配）
    uint32_t asid = 0;     // 地址空间标识符（来自 CSR.ASID[9:0]）
    uint32_t vppn = 0;     // 虚拟页对号（TLBEHI[31:13]）
    uint32_t ps   = 12;    // 页大小指数（12=4KB, 21=2MB, 22=4MB）
    uint32_t ppn0 = 0;     // 偶页物理页号（TLBELO0[31:8]）
    uint32_t plv0 = 0;
    uint32_t mat0 = 0;
    bool     d0   = false;
    bool     v0   = false;
    uint32_t ppn1 = 0;     // 奇页物理页号（TLBELO1[31:8]）
    uint32_t plv1 = 0;
    uint32_t mat1 = 0;
    bool     d1   = false;
    bool     v1   = false;
};

// TLB 表项数量：由 Kconfig CONFIG_TLB_SIZE 控制，默认 16
// 应与被测 CPU 的实际 TLB 表项数保持一致
#ifndef CONFIG_TLB_SIZE
#  define CONFIG_TLB_SIZE 16
#endif
#define TLB_SIZE CONFIG_TLB_SIZE

// 用 C++ 异常传播 CPU 陷阱（避免多级返回的复杂性）
struct CPUTrap {
    uint32_t ecode;
    uint32_t badv;
};

class Simulator {
public:
    explicit Simulator(uint32_t reset_vector = 0x1c000000u);
    ~Simulator() = default;

    void loadImage(const char* path);
    void loadImageMem(const void* data, size_t size, uint32_t base);
    void step(uint64_t n = 1);

    uint32_t getPC()       const { return pc; }
    uint32_t getRf(int i)  const { return rf[i & 31]; }
    SimState getState()    const { return state; }
    int      getExitCode() const { return exit_code; }

    PMemMap& getMemRef()   { return pmem; }

    void getCpuState(CpuState& out) const {
        memcpy(out.rf, rf, sizeof(rf));
        out.pc = pc;
    }
    void setCpuState(const CpuState& in) {
        memcpy(rf, in.rf, sizeof(rf));
        pc = in.pc;
    }

    uint32_t pmemRead(uint32_t addr, int bytes) const;
    void     pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask);

    // ── Difftest TLB 同步接口 ─────────────────────────────────────────────────
    // 由 difftest_tlbfill_index_sync() 调用，在下一条 TLBFILL 执行前设置索引覆盖。
    // idx 为 CPU 实际使用的 TLB 槽位号；-1 表示无覆盖（使用内部计数器）。
    void setTlbfillOverride(int idx) { tlbfill_override = idx; }

private:
    uint32_t  rf[32]   = {};
    uint32_t  pc;
    CSRFile   csr;
    PMemMap   pmem;
    SimState  state     = SIM_RUNNING;
    int       exit_code = 0;
    bool      llbit     = false;

    TLBEntry  tlb[TLB_SIZE] = {};
    int       tlbfill_idx      = 0;  // 用于 TLBFILL 的顺序回绕索引（独立运行时使用）
    int       tlbfill_override = -1; // difftest 同步覆盖：>=0 时下一次 TLBFILL 用此索引

    // 地址翻译：DA 直通 / DMW / TLB；mem_type: 1=取指 2=读 4=写
    // 失败时抛出 CPUTrap
    uint32_t translate(uint32_t vaddr, uint32_t mem_type);

    // 取指（可抛 CPUTrap）
    uint32_t fetch(uint32_t addr);

    // 执行单条指令
    void execute(uint32_t inst);

    // 内存访问辅助（可抛 CPUTrap）
    uint32_t memRead(uint32_t vaddr, int bytes);
    void     memWrite(uint32_t vaddr, uint32_t data, uint8_t wmask);

    // TLB 操作
    void tlbWr(uint32_t idx);
    void tlbRd(uint32_t idx);
    void tlbSrch();
    void tlbInv(uint32_t op, uint32_t rj_v, uint32_t rk_v);

    // 触发例外（设置 CSR、修改 PC，然后抛出 CPUTrap）
    [[noreturn]] void raiseException(uint32_t ecode, uint32_t subcode = 0, uint32_t badv = 0);

    void setRf(int rd, uint32_t val) { if (rd != 0) rf[rd] = val; }
};
