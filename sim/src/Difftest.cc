#include "Difftest.h"
#ifdef CONFIG_DIFFTEST

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

DifftestHost::~DifftestHost() {
    if (handle) dlclose(handle);
}

// 加载符号辅助宏
#define LOAD_SYM(fn, name) do { \
    fn = (decltype(fn))dlsym(handle, name); \
    if (!fn) { fprintf(stderr, "[difftest] 找不到符号 " name ": %s\n", dlerror()); exit(1); } \
} while (0)

void DifftestHost::init(const char* so_path, const char* img_path,
                        uint32_t base_addr, MemoryBase* mem) {
    handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "[difftest] 无法加载参考库 %s: %s\n", so_path, dlerror());
        exit(1);
    }
    LOAD_SYM(f_init,   "difftest_init");
    LOAD_SYM(f_memcpy, "difftest_memcpy");
    LOAD_SYM(f_regcpy, "difftest_regcpy");
    LOAD_SYM(f_exec,   "difftest_exec");
    LOAD_SYM(f_intr,   "difftest_raise_intr");
    LOAD_SYM(f_get_pc, "difftest_get_pc");
    LOAD_SYM(f_get_rf, "difftest_get_rf");

    // 可选符号：旧版 simulator.so 可能不含此接口，无则跳过
    f_tlbfill_sync = (fn_difftest_tlbfill_index_sync)dlsym(handle, "difftest_tlbfill_index_sync");
    if (!f_tlbfill_sync) {
        dlerror(); // 清除错误信息
        fprintf(stdout, "[difftest] 提示: simulator.so 不含 difftest_tlbfill_index_sync，"
                        "TLBFILL 同步已禁用\n");
    }

    // 初始化参考模拟器并同步内存
    f_init(img_path, base_addr);
    f_memcpy(mem->getMemRef(), true /*TO_REF*/);

    fprintf(stdout, "[difftest] 参考库加载成功: %s\n", so_path);
}

bool DifftestHost::step(const uint32_t* dut_rf, uint32_t dut_pc,
                        uint32_t n, MemoryBase* mem) {
    // 1. 检查参考 PC 是否与 DUT 提交 PC 一致
    uint32_t ref_pc = f_get_pc();
    if (ref_pc != dut_pc) {
        fprintf(stderr,
            ANSI_FG_RED "[difftest] PC 不一致！\n"
            "  DUT  PC = 0x%08x\n"
            "  REF  PC = 0x%08x\n" ANSI_NONE,
            dut_pc, ref_pc);
        printMismatch(dut_rf, dut_pc);
        return false;
    }

    // 2. 参考前进 n 步
    f_exec(n);

    // 3. 比较寄存器
#ifdef CONFIG_DIFFTEST_FULL_RF
    // 全量比较32个寄存器
    bool ok = true;
    for (int i = 0; i < 32; i++) {
        uint32_t ref_v = f_get_rf(i);
        if (dut_rf[i] != ref_v) {
            fprintf(stderr,
                ANSI_FG_RED "[difftest] 寄存器不一致 (提交PC=0x%08x)\n" ANSI_NONE,
                dut_pc);
            printMismatch(dut_rf, dut_pc);
            ok = false;
            break;
        }
    }
    if (!ok) return false;
#else
    // 仅检查 PC（快速模式，由调用方检查单个寄存器）
    // 此处不做额外检查
#endif

    (void)mem;
    return true;
}

void DifftestHost::raiseIntr(uint32_t irq) {
    f_intr(irq);
}

void DifftestHost::tlbfillSync(uint32_t idx) {
    if (f_tlbfill_sync) {
        f_tlbfill_sync(idx);
    }
}

void DifftestHost::printMismatch(const uint32_t* dut_rf, uint32_t dut_pc) const {
    fprintf(stderr, "\n" ANSI_BOLD "── DUT vs REF 寄存器对比 (提交PC=0x%08x) ──" ANSI_NONE "\n",
            dut_pc);
    fprintf(stderr, "  %-12s  %-12s  %-12s  %s\n",
            "寄存器", "DUT 值", "REF 值", "状态");
    fprintf(stderr, "  %s\n", "──────────────────────────────────────────────");
    for (int i = 0; i < 32; i++) {
        uint32_t ref_v = f_get_rf(i);
        uint32_t dut_v = dut_rf[i];
        bool mismatch  = (dut_v != ref_v);
        fprintf(stderr,
            "%s  %-12s  0x%08x    0x%08x    %s\n" ANSI_NONE,
            mismatch ? ANSI_FG_RED : "",
            LA32R_REG_NAMES[i], dut_v, ref_v,
            mismatch ? "<<< MISMATCH" : "OK");
    }
    uint32_t ref_pc = f_get_pc();
    fprintf(stderr, "\n  DUT PC = 0x%08x\n  REF PC = 0x%08x\n\n",
            dut_pc, ref_pc);
}

#endif // CONFIG_DIFFTEST
