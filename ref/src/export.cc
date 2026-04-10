// difftest ABI 导出符号（编译为 simulator.so 时使用）
// 独立可执行模式下此文件不参与链接
#ifndef REF_STANDALONE

#include "difftest_export.h"
#include "Simulator.h"
#include <cstring>

static Simulator* g_sim = nullptr;

__EXPORT void difftest_init(const char* img_path, uint32_t base_addr) {
    delete g_sim;
    g_sim = new Simulator(base_addr);
    if (img_path && img_path[0]) {
        g_sim->loadImage(img_path);
    }
}

__EXPORT void difftest_memcpy(PMemMap& dut_pmem, bool direction) {
    if (direction == DIFFTEST_TO_REF) {
        g_sim->getMemRef() = dut_pmem;
    } else {
        dut_pmem = g_sim->getMemRef();
    }
}

__EXPORT void difftest_regcpy(uint32_t* buf, bool direction) {
    // buf 布局: [rf[0]..rf[31], pc]
    if (direction == DIFFTEST_TO_REF) {
        CpuState s;
        memcpy(s.rf, buf, 32 * sizeof(uint32_t));
        s.pc = buf[32];
        g_sim->setCpuState(s);
    } else {
        CpuState s;
        g_sim->getCpuState(s);
        memcpy(buf, s.rf, 32 * sizeof(uint32_t));
        buf[32] = s.pc;
    }
}

__EXPORT void difftest_exec(uint64_t n) {
    g_sim->step(n);
}

__EXPORT void difftest_raise_intr(uint32_t irq) {
    // 将中断注入参考模拟器（简化实现：直接执行例外入口）
    // TODO: 实现完整中断处理
    (void)irq;
    g_sim->step(1);
}

__EXPORT uint32_t difftest_get_pc() {
    return g_sim->getPC();
}

__EXPORT uint32_t difftest_get_rf(int i) {
    return g_sim->getRf(i);
}

__EXPORT void difftest_tlbfill_index_sync(uint32_t idx) {
    g_sim->setTlbfillOverride((int)idx);
}

#endif // REF_STANDALONE
