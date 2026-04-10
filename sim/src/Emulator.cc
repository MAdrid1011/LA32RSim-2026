#include "Emulator.h"
#include "VCPU.h"
#include <cstdio>
#include <cstring>

#ifdef CONFIG_DUMP_WAVE
#include "verilated_vcd_c.h"
#endif

// ── DUT 信号访问宏 ────────────────────────────────────────────────────────────
// 每个提交端口 i 的信号（Verilator 将 io_cmt_0_valid 等展开为成员）
// 宏接受端口索引 (0..3)
#define CMT_VALID(i)   (cpu->io_cmt_##i##_valid)
#define CMT_PC(i)      (cpu->io_cmt_##i##_pc)
#define CMT_INST(i)    (cpu->io_cmt_##i##_inst)
#define CMT_RD_V(i)    (cpu->io_cmt_##i##_rd_valid)
#define CMT_RD(i)      (cpu->io_cmt_##i##_rd)
#define CMT_EXC(i)     (cpu->io_cmt_##i##_exception)
#define CMT_ECODE(i)   (cpu->io_cmt_##i##_exception_code)

// 全量架构寄存器堆：DUT 输出 io_cmt_rf_0 .. io_cmt_rf_31
#define RF_PORT(i)     (cpu->io_cmt_rf_##i)

// TLBFILL 同步信号（需 CONFIG_DIFFTEST_TLBFILL_SYNC + CPU 支持）
// CPU 顶层需输出：
//   io_cmt_tlbfill_valid (1 bit)  — 本周期是否有 tlbfill 提交
//   io_cmt_tlbfill_idx   (N bit)  — 实际写入的 TLB 槽位号
#ifdef CONFIG_DIFFTEST_TLBFILL_SYNC
#  define TLBFILL_VALID  (cpu->io_cmt_tlbfill_valid)
#  define TLBFILL_IDX    (cpu->io_cmt_tlbfill_idx)
#endif

Emulator::Emulator(VCPU* cpu_, MemoryBase* mem_)
    : cpu(cpu_), mem(mem_) {}

#ifdef CONFIG_DIFFTEST
void Emulator::initDifftest(const char* so_path, const char* img_path,
                            uint32_t base_addr) {
    difftest.init(so_path, img_path, base_addr, mem);
}
#endif

void Emulator::reset(int n) {
    cpu->reset = 1;
    for (int i = 0; i < n; i++) {
        cpu->clock = 0; cpu->eval();
        cpu->clock = 1; cpu->eval();
    }
    cpu->reset = 0;
    fprintf(stdout, "[sim] 复位完成，复位向量 = 0x%08x\n", (uint32_t)CONFIG_RESET_VECTOR);
}

void Emulator::tick() {
    // 下降沿：处理写操作
    cpu->clock = 0;
    cpu->eval();
    mem->write(cpu, cycles);

    // 上升沿：处理读操作、更新内存读数据
    cpu->clock = 1;
    mem->read(cpu, cycles);
    cpu->eval();

#ifdef CONFIG_DUMP_WAVE
    if (trace) {
        bool in_range = (cycles >= (uint64_t)CONFIG_WAVE_BEGIN);
#if CONFIG_WAVE_END > 0
        in_range = in_range && (cycles <= (uint64_t)CONFIG_WAVE_END);
#endif
        if (in_range) trace->dump((uint64_t)(cycles * 2 + 1));
    }
#endif
    cycles++;
}

// 读取 DUT 全量架构寄存器堆
void Emulator::readDutRF(uint32_t* rf_out) {
    rf_out[ 0] = RF_PORT( 0); rf_out[ 1] = RF_PORT( 1);
    rf_out[ 2] = RF_PORT( 2); rf_out[ 3] = RF_PORT( 3);
    rf_out[ 4] = RF_PORT( 4); rf_out[ 5] = RF_PORT( 5);
    rf_out[ 6] = RF_PORT( 6); rf_out[ 7] = RF_PORT( 7);
    rf_out[ 8] = RF_PORT( 8); rf_out[ 9] = RF_PORT( 9);
    rf_out[10] = RF_PORT(10); rf_out[11] = RF_PORT(11);
    rf_out[12] = RF_PORT(12); rf_out[13] = RF_PORT(13);
    rf_out[14] = RF_PORT(14); rf_out[15] = RF_PORT(15);
    rf_out[16] = RF_PORT(16); rf_out[17] = RF_PORT(17);
    rf_out[18] = RF_PORT(18); rf_out[19] = RF_PORT(19);
    rf_out[20] = RF_PORT(20); rf_out[21] = RF_PORT(21);
    rf_out[22] = RF_PORT(22); rf_out[23] = RF_PORT(23);
    rf_out[24] = RF_PORT(24); rf_out[25] = RF_PORT(25);
    rf_out[26] = RF_PORT(26); rf_out[27] = RF_PORT(27);
    rf_out[28] = RF_PORT(28); rf_out[29] = RF_PORT(29);
    rf_out[30] = RF_PORT(30); rf_out[31] = RF_PORT(31);
}

SimResult Emulator::handleCommit(int idx, const uint32_t* rf) {
    // 用宏根据编译期索引选择信号（需要展开每个 case）
    uint8_t  valid = 0;
    uint32_t pc    = 0;
    uint32_t inst  = 0;
    bool     rd_v  = false;
    uint32_t rd    = 0;
    bool     exc   = false;
    uint32_t ecode = 0;

    switch (idx) {
    case 0:
        valid = CMT_VALID(0); pc = CMT_PC(0); inst = CMT_INST(0);
        rd_v  = CMT_RD_V(0);  rd = CMT_RD(0);
        exc   = CMT_EXC(0);   ecode = CMT_ECODE(0); break;
#if CONFIG_NCOMMIT >= 2
    case 1:
        valid = CMT_VALID(1); pc = CMT_PC(1); inst = CMT_INST(1);
        rd_v  = CMT_RD_V(1);  rd = CMT_RD(1);
        exc   = CMT_EXC(1);   ecode = CMT_ECODE(1); break;
#endif
#if CONFIG_NCOMMIT >= 4
    case 2:
        valid = CMT_VALID(2); pc = CMT_PC(2); inst = CMT_INST(2);
        rd_v  = CMT_RD_V(2);  rd = CMT_RD(2);
        exc   = CMT_EXC(2);   ecode = CMT_ECODE(2); break;
    case 3:
        valid = CMT_VALID(3); pc = CMT_PC(3); inst = CMT_INST(3);
        rd_v  = CMT_RD_V(3);  rd = CMT_RD(3);
        exc   = CMT_EXC(3);   ecode = CMT_ECODE(3); break;
#endif
    default: return SIM_GOOD_TRAP;  // 不可达，哑值
    }

    if (!valid) return SIM_GOOD_TRAP;  // 此槽位本周期没有提交

    insts++;
    stall_cnt = 0;

    // 记录 ITRACE
    ITRACE_PUSH(pc, inst, rd_v, rd, rd_v ? rf[rd] : 0, cycles);

    // 检测 GOOD/BAD TRAP
    if (isBreak0(inst)) {
        int exit_code = (int)rf[4];  // a0
        if (exit_code == 0) {
            LogOk("HIT GOOD TRAP at PC=0x%08x", pc);
            ITRACE_DUMP();
            return SIM_GOOD_TRAP;
        } else {
            fprintf(stdout,
                ANSI_FG_RED "[sim] HIT BAD TRAP at PC=0x%08x (a0=%d)\n" ANSI_NONE,
                pc, exit_code);
            ITRACE_DUMP();
            return SIM_BAD_TRAP;
        }
    }

    // 注意：difftest 在外层循环统一批量调用，这里不单独调用
    (void)exc; (void)ecode;
    return (SimResult)1;  // 继续（非标准码，表示"ok,继续"）
}

SimResult Emulator::run(uint64_t max_steps) {
#ifdef CONFIG_DUMP_WAVE
    Verilated::traceEverOn(true);
    trace = new VerilatedVcdC;
    cpu->trace(trace, 99);
    trace->open("build/waveform.vcd");
    fprintf(stdout, "[sim] 波形文件: build/waveform.vcd\n");
#endif

    reset();
    startProgressThread(&cycles, &insts);

    uint32_t rf[32];

    while (true) {
        // 超步检测
        if (max_steps > 0 && cycles >= max_steps) {
            fprintf(stdout, "\n[sim] 达到最大仿真周期数 %llu，停止\n",
                    (unsigned long long)max_steps);
            ITRACE_DUMP();
            return SIM_MAX_STEPS;
        }

        // 读取本周期所有提交信号（在 tick 之前读，对应当前周期提交）
        // cmt_rf 是组合输出，已包含本周期所有 commit 写入
        readDutRF(rf);

        // ── 第一步：处理所有提交槽位（ITRACE/TRAP检测），收集有效提交数和首提交PC ──
        int     valid_cnt       = 0;
        uint32_t first_cmt_pc  = 0;
        SimResult early_exit   = (SimResult)1;   // 初始"继续"

        for (int i = 0; i < CONFIG_NCOMMIT; i++) {
            SimResult r = handleCommit(i, rf);

            if (r == (SimResult)1) {
                // 有效但非 TRAP：统计有效提交数
                // 此处 handleCommit 已在内部计数，但我们需要首提交 PC
                // 重新读一次 valid 和 pc
                uint8_t  valid = 0;
                uint32_t pc    = 0;
                switch (i) {
                case 0: valid = CMT_VALID(0); pc = CMT_PC(0); break;
#if CONFIG_NCOMMIT >= 2
                case 1: valid = CMT_VALID(1); pc = CMT_PC(1); break;
#endif
#if CONFIG_NCOMMIT >= 4
                case 2: valid = CMT_VALID(2); pc = CMT_PC(2); break;
                case 3: valid = CMT_VALID(3); pc = CMT_PC(3); break;
#endif
                default: break;
                }
                if (valid) {
                    if (valid_cnt == 0) first_cmt_pc = pc;
                    valid_cnt++;
                }
            } else if (r == SIM_GOOD_TRAP) {
                // TRAP：检查是否真正命中（有效槽位）
                uint8_t valid = 0;
                switch (i) {
                case 0: valid = CMT_VALID(0); break;
#if CONFIG_NCOMMIT >= 2
                case 1: valid = CMT_VALID(1); break;
#endif
#if CONFIG_NCOMMIT >= 4
                case 2: valid = CMT_VALID(2); break;
                case 3: valid = CMT_VALID(3); break;
#endif
                default: break;
                }
                if (valid) { early_exit = SIM_GOOD_TRAP; goto sim_done; }
            } else {
                // 错误（BAD TRAP、DIFFTEST_FAIL 等）
                early_exit = r;
                goto sim_done;
            }
        }

        // ── 第二步：对本周期所有有效提交做一次批量 difftest ──────────────────────
        // cmt_rf 已反映本周期全部 commit 后的 ARF 状态；
        // REF 需步进 valid_cnt 步，到达与 cmt_rf 匹配的状态。
#ifdef CONFIG_DIFFTEST
        if (valid_cnt > 0 && difftest.isLoaded()) {
#ifdef CONFIG_DIFFTEST_TLBFILL_SYNC
            // 若本周期有 tlbfill 提交，先通知参考模拟器使用相同的 TLB 槽位，
            // 再调用 step()（step 内部会执行 difftest_exec，届时 tlbfill 才在 REF 运行）
            if (TLBFILL_VALID) {
                difftest.tlbfillSync((uint32_t)TLBFILL_IDX);
            }
#endif
            if (!difftest.step(rf, first_cmt_pc, valid_cnt, mem)) {
                ITRACE_DUMP();
#ifdef CONFIG_MEM_WTRACE
                dumpMemWriteTimestamp(mem, first_cmt_pc & ~0xFu, 4);
#endif
                return SIM_DIFFTEST_FAIL;
            }
        }
#endif

        // 死锁检测
#if CONFIG_STALL_TIMEOUT > 0
        if (valid_cnt > 0) stall_cnt = 0;
        else stall_cnt++;
        if (stall_cnt > (uint64_t)CONFIG_STALL_TIMEOUT) {
            fprintf(stdout,
                "\n" ANSI_FG_RED "[sim] 死锁：连续 %llu 周期无指令提交，已提交总计 %llu 条\n" ANSI_NONE,
                (unsigned long long)stall_cnt,
                (unsigned long long)insts);
            ITRACE_DUMP();
            return SIM_STALL;
        }
#endif

        tick();
    }

sim_done:;
    SimResult final_result = SIM_GOOD_TRAP;
    // 重新确定结果：通过最后 handleCommit 的 r 值
    // （已在循环内 goto 时确定，此处取最后有效值）
    // 通过读取 rf[4] 判断
    readDutRF(rf);
    if (rf[4] != 0 && insts > 0) final_result = SIM_BAD_TRAP;

    fprintf(stdout, "\n[sim] 仿真结束: 周期=%llu, 指令=%llu, IPC=%.3f\n",
            (unsigned long long)cycles,
            (unsigned long long)insts,
            cycles > 0 ? (double)insts/cycles : 0.0);

#ifdef CONFIG_DUMP_WAVE
    if (trace) { trace->close(); delete trace; trace = nullptr; }
#endif
    return final_result;
}
