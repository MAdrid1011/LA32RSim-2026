#include "Debug.h"
#include "LA32R_ISA.h"
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>

// ── ITRACE ────────────────────────────────────────────────────────────────────
#ifdef CONFIG_ITRACE

ITraceBuffer g_itrace;

// 极简反汇编：仅输出 opcode 名 + 字段，不依赖外部库
const char* ITraceBuffer::disasm(uint32_t inst, char* out, int len) {
    uint32_t op6  = GET_OP6(inst);
    uint32_t op10 = GET_OP10(inst);
    uint32_t op8  = GET_OP8(inst);
    uint32_t op17 = GET_OP17(inst);
    uint32_t rd   = GET_RD(inst);
    uint32_t rj   = GET_RJ(inst);
    uint32_t rk   = GET_RK(inst);
    (void)op8;

    // 快速识别常用指令
    if ((inst >> 25) == 0x0A) {
        snprintf(out, len, "lu12i.w  r%u, 0x%x", rd, GET_SI20(inst));
    } else if ((inst >> 25) == 0x0E) {
        snprintf(out, len, "pcaddu12i r%u, 0x%x", rd, GET_SI20(inst));
    } else if (op6 == OP6_JIRL) {
        snprintf(out, len, "jirl     r%u, r%u, %d", rd, rj, GET_SI16(inst)<<2);
    } else if (op6 == OP6_B) {
        snprintf(out, len, "b        0x%x", GET_OFF26(inst)<<2);
    } else if (op6 == OP6_BL) {
        snprintf(out, len, "bl       0x%x", GET_OFF26(inst)<<2);
    } else if (op6 == OP6_BEQ) {
        snprintf(out, len, "beq      r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    } else if (op6 == OP6_BNE) {
        snprintf(out, len, "bne      r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    } else if (op6 == OP6_BLT) {
        snprintf(out, len, "blt      r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    } else if (op6 == OP6_BGE) {
        snprintf(out, len, "bge      r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    } else if (op6 == OP6_BLTU){
        snprintf(out, len, "bltu     r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    } else if (op6 == OP6_BGEU){
        snprintf(out, len, "bgeu     r%u, r%u, %d", rj, rk, GET_OFF16(inst)<<2);
    // 2R 指令（op17 = bits[31:15]，唯一标识）
    } else if (op17 == (OP_EXT_W_B >> 15)) snprintf(out, len, "ext.w.b  r%u, r%u", rd, rj);
    else if (op17 == (OP_EXT_W_H >> 15))   snprintf(out, len, "ext.w.h  r%u, r%u", rd, rj);
    else if (op17 == (OP_CLZ_W   >> 15))   snprintf(out, len, "clz.w    r%u, r%u", rd, rj);
    else if (op17 == (OP_CTZ_W   >> 15))   snprintf(out, len, "ctz.w    r%u, r%u", rd, rj);
    else if (op17 == (OP_REVB_2H >> 15))   snprintf(out, len, "revb.2h  r%u, r%u", rd, rj);
    else if (op17 == (OP_REVB_4H >> 15))   snprintf(out, len, "revb.4h  r%u, r%u", rd, rj);
    // 3R ALU 和移位立即数：操作码在 bits[31:15]，与 op17 比较
    else if (op17 == OP10_ADD_W)    snprintf(out, len, "add.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SUB_W)    snprintf(out, len, "sub.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_AND)      snprintf(out, len, "and      r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_OR)       snprintf(out, len, "or       r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_XOR)      snprintf(out, len, "xor      r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_NOR)      snprintf(out, len, "nor      r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SLL_W)    snprintf(out, len, "sll.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SRL_W)    snprintf(out, len, "srl.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SRA_W)    snprintf(out, len, "sra.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MUL_W)    snprintf(out, len, "mul.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MULH_W)   snprintf(out, len, "mulh.w   r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MULH_WU)  snprintf(out, len, "mulh.wu  r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_DIV_W)    snprintf(out, len, "div.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MOD_W)    snprintf(out, len, "mod.w    r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_DIV_WU)   snprintf(out, len, "div.wu   r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MOD_WU)   snprintf(out, len, "mod.wu   r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_ROTR_W)   snprintf(out, len, "rotr.w   r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MASKEQZ)  snprintf(out, len, "maskeqz  r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_MASKNEZ)  snprintf(out, len, "masknez  r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_ORN)      snprintf(out, len, "orn      r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_ANDN)     snprintf(out, len, "andn     r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SLT)      snprintf(out, len, "slt      r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SLTU)     snprintf(out, len, "sltu     r%u, r%u, r%u", rd, rj, rk);
    else if (op17 == OP10_SLLI_W)   snprintf(out, len, "slli.w   r%u, r%u, %u",  rd, rj, GET_UI5(inst));
    else if (op17 == OP10_SRLI_W)   snprintf(out, len, "srli.w   r%u, r%u, %u",  rd, rj, GET_UI5(inst));
    else if (op17 == OP10_SRAI_W)   snprintf(out, len, "srai.w   r%u, r%u, %u",  rd, rj, GET_UI5(inst));
    else if (op17 == OP10_ROTRI_W)  snprintf(out, len, "rotri.w  r%u, r%u, %u",  rd, rj, GET_UI5(inst));
    else if (op17 == OP10_BREAK)    snprintf(out, len, "break    %u",  GET_CODE(inst));
    else if (op17 == OP10_SYSCALL)  snprintf(out, len, "syscall  %u",  GET_CODE(inst));
    // CSR 和特权指令
    else if ((inst >> 24) == 0x04u) snprintf(out, len, "csr      r%u, csr:0x%x", rd, GET_CSR(inst));
    else if (inst == 0x06483800u)   snprintf(out, len, "ertn");
    else if (inst == 0x06482000u)   snprintf(out, len, "tlbrd");
    else if (inst == 0x06482400u)   snprintf(out, len, "tlbwr");
    else if (inst == 0x06482800u)   snprintf(out, len, "tlbfill");
    else if (inst == 0x06484000u)   snprintf(out, len, "tlbsrch");
    else if ((inst >> 15) == 0x0CA4u) snprintf(out, len, "invtlb   %u, r%u, r%u", rd, rj, rk);
    // 立即数类：opcode 在 bits[31:22]（op10）
    else if (op10 == 0x00Au) snprintf(out, len, "addi.w   r%u, r%u, %d",   rd, rj, GET_SI12(inst));
    else if (op10 == 0x008u) snprintf(out, len, "slti     r%u, r%u, %d",   rd, rj, GET_SI12(inst));
    else if (op10 == 0x009u) snprintf(out, len, "sltui    r%u, r%u, %d",   rd, rj, GET_SI12(inst));
    else if (op10 == 0x00Du) snprintf(out, len, "andi     r%u, r%u, 0x%x", rd, rj, GET_UI12(inst));
    else if (op10 == 0x00Eu) snprintf(out, len, "ori      r%u, r%u, 0x%x", rd, rj, GET_UI12(inst));
    else if (op10 == 0x00Fu) snprintf(out, len, "xori     r%u, r%u, 0x%x", rd, rj, GET_UI12(inst));
    // Load/Store：opcode 在 bits[31:22]（op10）
    else if (op10 == 0xA0u) snprintf(out, len, "ld.b     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA1u) snprintf(out, len, "ld.h     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA2u) snprintf(out, len, "ld.w     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA8u) snprintf(out, len, "ld.bu    r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA9u) snprintf(out, len, "ld.hu    r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA4u) snprintf(out, len, "st.b     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA5u) snprintf(out, len, "st.h     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else if (op10 == 0xA6u) snprintf(out, len, "st.w     r%u, r%u, %d", rd, rj, GET_SI12(inst));
    else {
        snprintf(out, len, "??? 0x%08x", inst);
    }
    return out;
}

void ITraceBuffer::push(uint32_t pc, uint32_t inst, bool rd_valid,
                        uint32_t rd, uint32_t rd_val, uint64_t cycle) {
    buf[head].pc       = pc;
    buf[head].inst     = inst;
    buf[head].rd_valid = rd_valid;
    buf[head].rd       = rd;
    buf[head].rd_val   = rd_val;
    buf[head].cycle    = cycle;
    head = (head + 1) % CONFIG_ITRACE_SIZE;
    if (count < CONFIG_ITRACE_SIZE) count++;
}

void ITraceBuffer::dump() const {
    if (count == 0) return;
    fprintf(stdout, "\n" ANSI_BOLD "── 最近 %d 条指令追踪（ITRACE）──" ANSI_NONE "\n", count);
    fprintf(stdout, "  %-8s  %-10s  %-40s  %-12s  %s\n",
            "周期", "PC", "指令", "写入值", "目标寄存器");
    fprintf(stdout, "  %s\n",
            "─────────────────────────────────────────────────────────────────────────");
    int start = (head - count + CONFIG_ITRACE_SIZE) % CONFIG_ITRACE_SIZE;
    for (int i = 0; i < count; i++) {
        const ITraceEntry& e = buf[(start + i) % CONFIG_ITRACE_SIZE];
        char asm_buf[64];
        disasm(e.inst, asm_buf, sizeof(asm_buf));
        if (e.rd_valid && e.rd != 0) {
            fprintf(stdout, "  %-8llu  0x%08x  %-40s  0x%08x    r%u\n",
                    (unsigned long long)e.cycle, e.pc, asm_buf, e.rd_val, e.rd);
        } else {
            fprintf(stdout, "  %-8llu  0x%08x  %-40s  %-12s  -\n",
                    (unsigned long long)e.cycle, e.pc, asm_buf, "-");
        }
    }
    fprintf(stdout, "\n");
}
#endif // CONFIG_ITRACE

// ── 内存写入时间戳 ────────────────────────────────────────────────────────────
#ifdef CONFIG_MEM_WTRACE
void dumpMemWriteTimestamp(MemoryBase* mem, uint32_t addr, int nwords) {
    if (!mem) return;
    fprintf(stdout, "\n" ANSI_BOLD "── 内存写入时间戳（字节粒度）──" ANSI_NONE "\n");
    mem->dumpWriteTimestamp(addr, nwords);
}
#endif

// ── 进度显示后台线程 ──────────────────────────────────────────────────────────
void startProgressThread(const uint64_t* cycles_ptr, const uint64_t* insts_ptr) {
    std::thread([cycles_ptr, insts_ptr]() {
        uint64_t last_insts = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            uint64_t cyc  = *cycles_ptr;
            uint64_t inst = *insts_ptr;
            double   ipc  = (cyc > 0) ? (double)inst / cyc : 0.0;
            fprintf(stdout, "\r[进度] 周期: %-10llu  提交指令: %-10llu  IPC: %.3f   ",
                    (unsigned long long)cyc,
                    (unsigned long long)inst,
                    ipc);
            fflush(stdout);
            last_insts = inst;
        }
    }).detach();
}
