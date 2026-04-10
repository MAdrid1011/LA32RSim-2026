#pragma once
#include <cstdint>
#include <cstdio>
#include "common.h"
#include "Memory.h"

// ── ITRACE 环形缓冲区 ─────────────────────────────────────────────────────────
#ifdef CONFIG_ITRACE

#ifndef CONFIG_ITRACE_SIZE
#define CONFIG_ITRACE_SIZE 32
#endif

struct ITraceEntry {
    uint32_t pc       = 0;
    uint32_t inst     = 0;
    uint32_t rd       = 0;    // 目标寄存器编号
    bool     rd_valid = false;
    uint32_t rd_val   = 0;    // 写入值
    uint64_t cycle    = 0;
};

class ITraceBuffer {
public:
    void push(uint32_t pc, uint32_t inst, bool rd_valid,
              uint32_t rd, uint32_t rd_val, uint64_t cycle);
    void dump() const;

private:
    ITraceEntry buf[CONFIG_ITRACE_SIZE];
    int  head  = 0;
    int  count = 0;

    static const char* disasm(uint32_t inst, char* out, int len);
};

extern ITraceBuffer g_itrace;

#define ITRACE_PUSH(pc, inst, rdv, rd, rdval, cyc) \
    g_itrace.push(pc, inst, rdv, rd, rdval, cyc)
#define ITRACE_DUMP() g_itrace.dump()

#else
#define ITRACE_PUSH(...) do {} while(0)
#define ITRACE_DUMP()    do {} while(0)
#endif // CONFIG_ITRACE

// ── 内存写入时间戳查询工具函数 ────────────────────────────────────────────────
#ifdef CONFIG_MEM_WTRACE
void dumpMemWriteTimestamp(MemoryBase* mem, uint32_t addr, int nwords = 4);
#endif

// ── 进度显示 ──────────────────────────────────────────────────────────────────
void startProgressThread(const uint64_t* cycles_ptr, const uint64_t* insts_ptr);
