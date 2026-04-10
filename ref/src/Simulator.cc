#include "Simulator.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdexcept>

// 内存访问类型
#define MEM_FETCH  1u
#define MEM_LOAD   2u
#define MEM_STORE  4u

Simulator::Simulator(uint32_t reset_vector) : pc(reset_vector) {
    memset(rf, 0, sizeof(rf));
    csr.crmd = 0x8u;  // DA=1, PG=0, PLV=0
}

void Simulator::loadImage(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        fprintf(stderr, "[ref] 无法打开镜像文件: %s\n", path);
        exit(1);
    }
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    loadImageMem(buf.data(), sz, pc);
}

void Simulator::loadImageMem(const void* data, size_t size, uint32_t base) {
    const uint8_t* src = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; i++) {
        uint32_t addr = base + i;
        uint32_t key  = addr >> 2;
        int      byte = addr & 3;
        uint32_t& word = pmem[key];
        word = (word & ~(0xFFu << (byte * 8))) | (uint32_t(src[i]) << (byte * 8));
    }
}

uint32_t Simulator::pmemRead(uint32_t addr, int bytes) const {
    uint32_t result = 0;
    for (int i = 0; i < bytes; i++) {
        uint32_t a   = addr + i;
        uint32_t key = a >> 2;
        int      bit = (a & 3) * 8;
        auto it = pmem.find(key);
        if (it != pmem.end())
            result |= (uint32_t)((it->second >> bit) & 0xFF) << (i * 8);
    }
    return result;
}

void Simulator::pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask) {
    for (int i = 0; i < 4; i++) {
        if (!((wmask >> i) & 1)) continue;
        uint32_t a   = addr + i;
        uint32_t key = a >> 2;
        int      bit = (a & 3) * 8;
        uint32_t& word = pmem[key];
        word = (word & ~(0xFFu << bit)) | (((data >> (i*8)) & 0xFF) << bit);
    }
}

// ── 例外处理 ──────────────────────────────────────────────────────────────────
[[noreturn]] void Simulator::raiseException(uint32_t ecode, uint32_t subcode, uint32_t badv) {
    // 保存 CRMD[2:0]（PLV+IE）到 PRMD
    csr.prmd = csr.crmd & 0x7u;
    // 清零 PLV=0, IE=0
    csr.crmd &= ~0x7u;
    // TLBR（ecode=0x3f）：切换到直接地址模式（DA=1, PG=0）
    if (ecode == 0x3Fu) {
        csr.crmd = (csr.crmd & ~0x18u) | 0x08u;  // bit3=DA=1, bit4=PG=0
    }
    // 保存 PC 到 ERA
    csr.era = pc;
    // 写 ESTAT.Ecode（bits[21:16]）和 EsubCode（bits[30:22]）
    csr.estat = (csr.estat & ~0x7FFF0000u)
              | ((ecode    & 0x3Fu) << 16)
              | ((subcode  & 0x1FFu) << 22);
    // 设置 BADV（访存相关例外）
    if (ecode == ECODE_ADEF) {
        csr.badv = pc;
    } else if (ecode == ECODE_ALE || ecode == 0x3Fu
               || (ecode >= 0x1u && ecode <= 0x4u) || ecode == ECODE_PPI) {
        csr.badv = badv;
    }
    // 设置 TLBEHI（TLB 相关例外：TLBR, PIL, PIS, PIF, PME, PPI）
    if (ecode == 0x3Fu || (ecode >= 0x1u && ecode <= 0x4u) || ecode == ECODE_PPI) {
        csr.tlbehi = badv & ~0x1FFFu;  // 仅保留 VPPN，清除低 13 位
    }
    // 跳转到例外入口
    pc = (ecode == 0x3Fu) ? csr.tlbrentry : csr.eentry;
    throw CPUTrap{ecode, badv};
}

// ── 地址翻译 ──────────────────────────────────────────────────────────────────
uint32_t Simulator::translate(uint32_t vaddr, uint32_t mem_type) {
    // DA 模式：直接地址，直通
    if ((csr.crmd >> 3) & 1u) {
        return vaddr;
    }

    // PG 模式：先查 DMW，再查 TLB
    uint32_t plv = csr.crmd & 0x3u;

    // 检查 DMW0
    uint32_t dmw0 = csr.dmw[0];
    if (((dmw0 >> 29) & 0x7u) == (vaddr >> 29) && ((dmw0 >> plv) & 1u)) {
        return ((dmw0 >> 25) & 0x7u) << 29 | (vaddr & 0x1FFFFFFFu);
    }
    // 检查 DMW1
    uint32_t dmw1 = csr.dmw[1];
    if (((dmw1 >> 29) & 0x7u) == (vaddr >> 29) && ((dmw1 >> plv) & 1u)) {
        return ((dmw1 >> 25) & 0x7u) << 29 | (vaddr & 0x1FFFFFFFu);
    }

    // TLB 查找
    uint32_t asid = csr.asid & 0x3FFu;
    int hit = -1;
    for (int i = 0; i < TLB_SIZE; i++) {
        const TLBEntry& e = tlb[i];
        if (!e.e) continue;
        if (!e.g && e.asid != asid) continue;
        // 对比 VPPN（屏蔽掉页大小以下的位）
        if ((e.vppn >> (e.ps - 12)) == (vaddr >> (e.ps + 1))) {
            hit = i;
            break;
        }
    }

    if (hit < 0) {
        raiseException(0x3Fu, 0, vaddr);  // TLBR
    }

    const TLBEntry& entry = tlb[hit];
    bool     is_odd = (vaddr >> entry.ps) & 1u;
    uint32_t ppn    = is_odd ? entry.ppn1  : entry.ppn0;
    bool     v      = is_odd ? entry.v1    : entry.v0;
    bool     d      = is_odd ? entry.d1    : entry.d0;
    uint32_t eplv   = is_odd ? entry.plv1  : entry.plv0;

    if (!v) {
        switch (mem_type) {
        case MEM_FETCH: raiseException(ECODE_PIF, 0, vaddr);
        case MEM_LOAD:  raiseException(ECODE_PIL, 0, vaddr);
        case MEM_STORE: raiseException(ECODE_PIS, 0, vaddr);
        default:        raiseException(ECODE_PIL, 0, vaddr);
        }
    }
    if (plv > eplv) {
        raiseException(ECODE_PPI, 0, vaddr);
    }
    if (mem_type == MEM_STORE && !d) {
        raiseException(ECODE_PME, 0, vaddr);
    }

    // 计算物理地址：PA = (ppn[19:(ps-12)] << ps) | vaddr[ps-1:0]
    uint32_t offset = vaddr & ((1u << entry.ps) - 1u);
    uint32_t pfn    = (ppn >> (entry.ps - 12)) << entry.ps;
    return pfn | offset;
}

// ── 取指 ──────────────────────────────────────────────────────────────────────
uint32_t Simulator::fetch(uint32_t addr) {
    // 取指地址对齐检查（ADEF）
    if (addr & 0x3u) {
        raiseException(ECODE_ADEF, 0, addr);
    }
    return pmemRead(translate(addr, MEM_FETCH), 4);
}

// ── 内存读写 ──────────────────────────────────────────────────────────────────
#define REF_UART_ADDR 0xa00003f8u  // base-port trm.c 串口地址

uint32_t Simulator::memRead(uint32_t vaddr, int bytes) {
    return pmemRead(translate(vaddr, MEM_LOAD), bytes);
}

void Simulator::memWrite(uint32_t vaddr, uint32_t data, uint8_t wmask) {
    uint32_t paddr = translate(vaddr, MEM_STORE);
    // 拦截串口输出
    if ((paddr & ~0x7u) == (REF_UART_ADDR & ~0x7u)) {
        for (int i = 0; i < 4; i++) {
            if ((wmask >> i) & 1) {
                uint8_t c = (data >> (i * 8)) & 0xFF;
                if (c) { putchar(c); fflush(stdout); }
            }
        }
        return;
    }
    pmemWrite(paddr, data, wmask);
}

// ── TLB 操作 ─────────────────────────────────────────────────────────────────

void Simulator::tlbWr(uint32_t idx) {
    TLBEntry& e = tlb[idx & (TLB_SIZE - 1)];
    bool ne = (csr.tlbidx >> 31) & 1u;
    // 在 TLBR 例外中 tlbwr 始终写有效表项
    bool in_tlbr = (((csr.estat >> 16) & 0x3Fu) == 0x3Fu);
    e.e    = in_tlbr ? true : !ne;
    e.ps   = (csr.tlbidx >> 24) & 0x3Fu;
    e.vppn = (csr.tlbehi >> 13);
    e.asid = csr.asid & 0x3FFu;
    e.g    = ((csr.tlbelo0 >> 6) & 1u) && ((csr.tlbelo1 >> 6) & 1u);
    e.ppn0 = csr.tlbelo0 >> 8;
    e.plv0 = (csr.tlbelo0 >> 2) & 0x3u;
    e.mat0 = (csr.tlbelo0 >> 4) & 0x3u;
    e.d0   = (csr.tlbelo0 >> 1) & 1u;
    e.v0   = (csr.tlbelo0     ) & 1u;
    e.ppn1 = csr.tlbelo1 >> 8;
    e.plv1 = (csr.tlbelo1 >> 2) & 0x3u;
    e.mat1 = (csr.tlbelo1 >> 4) & 0x3u;
    e.d1   = (csr.tlbelo1 >> 1) & 1u;
    e.v1   = (csr.tlbelo1     ) & 1u;
}

void Simulator::tlbRd(uint32_t idx) {
    const TLBEntry& e = tlb[idx & (TLB_SIZE - 1)];
    if (e.e) {
        csr.asid   = (csr.asid & ~0x3FFu) | e.asid;
        csr.tlbehi = e.vppn << 13;
        csr.tlbelo0 = (e.ppn0 << 8) | ((uint32_t)e.g << 6)
                    | (e.mat0 << 4) | (e.plv0 << 2)
                    | ((uint32_t)e.d0 << 1) | (uint32_t)e.v0;
        csr.tlbelo1 = (e.ppn1 << 8) | ((uint32_t)e.g << 6)
                    | (e.mat1 << 4) | (e.plv1 << 2)
                    | ((uint32_t)e.d1 << 1) | (uint32_t)e.v1;
        // 更新 TLBIDX：清除 NE，写入 PS
        csr.tlbidx = (csr.tlbidx & ~0xBF000000u) | ((e.ps << 24) & 0xBF000000u);
    } else {
        csr.asid    = 0;
        csr.tlbehi  = 0;
        csr.tlbelo0 = 0;
        csr.tlbelo1 = 0;
        // 设置 NE=1（无效表项）
        csr.tlbidx  = (csr.tlbidx & ~0xBF000000u) | (0x80000000u & 0xBF000000u);
    }
}

void Simulator::tlbSrch() {
    uint32_t asid = csr.asid & 0x3FFu;
    uint32_t vppn = csr.tlbehi >> 13;
    int hit = -1;
    for (int i = 0; i < TLB_SIZE; i++) {
        const TLBEntry& e = tlb[i];
        if (e.e && (e.g || e.asid == asid) && e.vppn == vppn) {
            hit = i;
            break;
        }
    }
    if (hit < 0) {
        // 未命中：NE=1
        csr.tlbidx = (csr.tlbidx & ~0x80000000u) | 0x80000000u;
    } else {
        uint32_t mask = (uint32_t)(TLB_SIZE - 1) | 0x80000000u;
        csr.tlbidx = (csr.tlbidx & ~mask) | ((uint32_t)hit & mask);
    }
}

void Simulator::tlbInv(uint32_t op, uint32_t rj_v, uint32_t rk_v) {
    uint32_t asid = rj_v & 0x3FFu;
    uint32_t va   = rk_v;
    switch (op) {
    case 0x0:
    case 0x1:
        for (int i = 0; i < TLB_SIZE; i++) tlb[i].e = false;
        break;
    case 0x2:
        for (int i = 0; i < TLB_SIZE; i++) if (tlb[i].g)  tlb[i].e = false;
        break;
    case 0x3:
        for (int i = 0; i < TLB_SIZE; i++) if (!tlb[i].g) tlb[i].e = false;
        break;
    case 0x4:
        for (int i = 0; i < TLB_SIZE; i++)
            if (!tlb[i].g && tlb[i].asid == asid) tlb[i].e = false;
        break;
    case 0x5:
        for (int i = 0; i < TLB_SIZE; i++)
            if (!tlb[i].g && tlb[i].asid == asid && tlb[i].vppn == (va >> 13))
                tlb[i].e = false;
        break;
    case 0x6:
        for (int i = 0; i < TLB_SIZE; i++)
            if ((tlb[i].asid == asid || tlb[i].g) && tlb[i].vppn == (va >> 13))
                tlb[i].e = false;
        break;
    default:
        raiseException(ECODE_INE, 0);  // 无效 op → INE 例外
    }
}

// ── 指令执行 ──────────────────────────────────────────────────────────────────
void Simulator::execute(uint32_t inst) {
    uint32_t op6  = GET_OP6(inst);
    uint32_t op8  = GET_OP8(inst);
    uint32_t op10 = GET_OP10(inst);
    uint32_t op17 = GET_OP17(inst);

    uint32_t rd   = GET_RD(inst);
    uint32_t rj   = GET_RJ(inst);
    uint32_t rk   = GET_RK(inst);
    uint32_t rj_v = rf[rj];
    uint32_t rk_v = rf[rk];
    uint32_t next_pc = pc + 4;

    // ── HALT（魔法字 0x80000000）────────────────────────────────────────────
    if (inst == 0x80000000u) {
        state    = SIM_END;
        exit_code = (int)rf[4];  // a0: 0=GOOD TRAP，非0=BAD TRAP
        return;
    }

    // ── BREAK ────────────────────────────────────────────────────────────────
    if (op17 == OP10_BREAK) {
        uint32_t code = GET_CODE(inst);
        if (code == 0) {
            state = SIM_END;
            exit_code = (int)rf[4];
            return;
        }
        fprintf(stderr, "[ref] BREAK %u at pc=0x%08x (trap)\n", code, pc);
        state = SIM_END;
        exit_code = (int)code;
        return;
    }

    // ── SYSCALL ──────────────────────────────────────────────────────────────
    if (op17 == OP10_SYSCALL) {
        raiseException(ECODE_SYS, 0);  // 抛出异常，不会执行到这里
    }

    // ── ERTN ─────────────────────────────────────────────────────────────────
    // LA32R 架构规范：ERTN 恢复 CRMD[2:0] 来自 PRMD，TLBR 返回时恢复 PG 模式
    if (inst == 0x06483800u) {
        csr.crmd = (csr.crmd & ~0x7u) | (csr.prmd & 0x7u);
        // TLBR 例外返回：恢复页表模式（DA=0, PG=1）
        if (((csr.estat >> 16) & 0x3Fu) == 0x3Fu) {
            csr.crmd = (csr.crmd & ~0x18u) | 0x10u;  // PG=1, DA=0
        }
        pc = csr.era;
        return;
    }

    // ── TLBSRCH（0x06482800）────────────────────────────────────────────────
    if (inst == 0x06482800u) { tlbSrch(); pc = next_pc; return; }

    // ── TLBRD（0x06482c00）──────────────────────────────────────────────────
    if (inst == 0x06482c00u) {
        tlbRd(csr.tlbidx & (TLB_SIZE - 1));
        pc = next_pc; return;
    }

    // ── TLBWR（0x06483000）──────────────────────────────────────────────────
    if (inst == 0x06483000u) {
        tlbWr(csr.tlbidx & (TLB_SIZE - 1));
        pc = next_pc; return;
    }

    // ── TLBFILL（0x06483400）────────────────────────────────────────────────
    // difftest 模式：外部通过 difftest_tlbfill_index_sync() 传入 CPU 实际
    // 使用的槽位索引；独立运行模式：使用内部顺序回绕计数器。
    if (inst == 0x06483400u) {
        int idx;
        if (tlbfill_override >= 0) {
            idx = tlbfill_override % TLB_SIZE;
            tlbfill_override = -1;  // 消费后清除，避免影响下一次 TLBFILL
        } else {
            idx = tlbfill_idx++ % TLB_SIZE;
        }
        tlbWr(idx);
        pc = next_pc; return;
    }

    // ── INVTLB（0x06498000，掩码 0xffff8000）────────────────────────────────
    // bits[4:0] = op, bits[9:5] = rj, bits[14:10] = rk（与 rk/rj 字段对应）
    if ((inst & 0xFFFF8000u) == 0x06498000u) {
        uint32_t op_inv = inst & 0x1Fu;        // bits[4:0]
        tlbInv(op_inv, rj_v, rk_v);            // rj=rj_v, rk=rk_v
        pc = next_pc; return;
    }

    // ── LU12I.W（inst[31:25] = 0b0001010）──────────────────────────────────
    if ((inst >> 25) == 0x0Au) {
        setRf(rd, GET_SI20(inst) << 12);
        pc = next_pc; return;
    }

    // ── PCADDU12I（inst[31:25] = 0b0001110）─────────────────────────────────
    if ((inst >> 25) == 0x0Eu) {
        setRf(rd, pc + (GET_SI20(inst) << 12));
        pc = next_pc; return;
    }

    // ── JIRL ─────────────────────────────────────────────────────────────────
    if (op6 == OP6_JIRL) {
        uint32_t target = rj_v + (GET_SI16(inst) << 2);
        setRf(rd, next_pc);
        pc = target; return;
    }

    // ── B / BL ───────────────────────────────────────────────────────────────
    if (op6 == OP6_B)  { pc = pc + (GET_OFF26(inst) << 2); return; }
    if (op6 == OP6_BL) { setRf(1, next_pc); pc = pc + (GET_OFF26(inst) << 2); return; }

    // ── 条件分支 ─────────────────────────────────────────────────────────────
    // LA32R 格式：op6|offs16|rj[9:5]|rd[4:0]，比较 rj_v OP rf[rd]
    {
        uint32_t br_rj_v = rj_v;     // bits[9:5]
        uint32_t br_rk_v = rf[rd];   // bits[4:0]
        if (op6==OP6_BEQ) { pc=(br_rj_v==br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
        if (op6==OP6_BNE) { pc=(br_rj_v!=br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
        if (op6==OP6_BLT) { pc=((int32_t)br_rj_v<(int32_t)br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
        if (op6==OP6_BGE) { pc=((int32_t)br_rj_v>=(int32_t)br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
        if (op6==OP6_BLTU){ pc=(br_rj_v<br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
        if (op6==OP6_BGEU){ pc=(br_rj_v>=br_rk_v)?pc+(GET_OFF16(inst)<<2):next_pc; return; }
    }

    // ── Load / Store（2RI12，op10 = bits[31:22]）─────────────────────────────
    if (op10 >= 0xA0u && op10 <= 0xAAu) {
        uint32_t addr = rj_v + GET_SI12(inst);
        switch (op10) {
        case 0xA0: setRf(rd,(int32_t)(int8_t) memRead(addr,1)); break; // LD.B
        case 0xA1: setRf(rd,(int32_t)(int16_t)memRead(addr,2)); break; // LD.H
        case 0xA2: setRf(rd,                   memRead(addr,4)); break; // LD.W
        case 0xA4: memWrite(addr, rf[rd], 0x1); break; // ST.B
        case 0xA5: memWrite(addr, rf[rd], 0x3); break; // ST.H
        case 0xA6: memWrite(addr, rf[rd], 0xF); break; // ST.W
        case 0xA8: setRf(rd,(uint8_t) memRead(addr,1)); break; // LD.BU
        case 0xA9: setRf(rd,(uint16_t)memRead(addr,2)); break; // LD.HU
        default:
            fprintf(stderr,"[ref] 未知 load/store op10=0x%02x pc=0x%08x\n",op10,pc);
            raiseException(ECODE_INE, 0);
        }
        pc = next_pc; return;
    }

    // ── 算术/逻辑立即数（SI12）──────────────────────────────────────────────
    if ((inst>>22)==0x00Au){ setRf(rd,rj_v+GET_SI12(inst)); pc=next_pc; return; } // ADDI.W
    if ((inst>>22)==0x008u){ setRf(rd,(int32_t)rj_v<(int32_t)GET_SI12(inst)?1:0); pc=next_pc; return; } // SLTI
    if ((inst>>22)==0x009u){ setRf(rd,rj_v<(uint32_t)GET_SI12(inst)?1:0); pc=next_pc; return; } // SLTUI
    if ((inst>>22)==0x00Du){ setRf(rd,rj_v&GET_UI12(inst)); pc=next_pc; return; } // ANDI
    if ((inst>>22)==0x00Eu){ setRf(rd,rj_v|GET_UI12(inst)); pc=next_pc; return; } // ORI
    if ((inst>>22)==0x00Fu){ setRf(rd,rj_v^GET_UI12(inst)); pc=next_pc; return; } // XORI

    // ── CSR 访问（op8 = 0x04）───────────────────────────────────────────────
    if (op8 == 0x04u) {
        uint32_t csr_no  = GET_CSR(inst);
        uint32_t old_val = csr.read(csr_no);
        if (rj == 0) {
            setRf(rd, old_val);                                      // CSRRD
        } else if (rj == 1) {
            csr.write(csr_no, rf[rd]); setRf(rd, old_val);          // CSRWR
        } else {
            uint32_t mask = rf[rj];
            csr.write(csr_no, (old_val & ~mask) | (rf[rd] & mask)); // CSRXCHG
            setRf(rd, old_val);
        }
        pc = next_pc; return;
    }

    // ── 移位立即数（2RI5，op17 = bits[31:15]）────────────────────────────────
    if (op17 == OP10_SLLI_W) { setRf(rd,rj_v<<GET_UI5(inst));                   pc=next_pc; return; }
    if (op17 == OP10_SRLI_W) { setRf(rd,rj_v>>GET_UI5(inst));                   pc=next_pc; return; }
    if (op17 == OP10_SRAI_W) { setRf(rd,(int32_t)rj_v>>GET_UI5(inst));          pc=next_pc; return; }
    if (op17 == OP10_ROTRI_W){
        uint32_t sa=GET_UI5(inst);
        setRf(rd,(rj_v>>sa)|(rj_v<<(32-sa))); pc=next_pc; return;
    }

    // ── 3R 算术逻辑（op17 = bits[31:15]）────────────────────────────────────
    switch (op17) {
    case OP10_ADD_W:   setRf(rd, rj_v + rk_v);                                   break;
    case OP10_SUB_W:   setRf(rd, rj_v - rk_v);                                   break;
    case OP10_AND:     setRf(rd, rj_v & rk_v);                                   break;
    case OP10_OR:      setRf(rd, rj_v | rk_v);                                   break;
    case OP10_NOR:     setRf(rd, ~(rj_v | rk_v));                                break;
    case OP10_XOR:     setRf(rd, rj_v ^ rk_v);                                   break;
    case OP10_ANDN:    setRf(rd, rj_v & ~rk_v);                                  break;
    case OP10_ORN:     setRf(rd, rj_v | ~rk_v);                                  break;
    case OP10_SLL_W:   setRf(rd, rj_v << (rk_v & 0x1Fu));                        break;
    case OP10_SRL_W:   setRf(rd, rj_v >> (rk_v & 0x1Fu));                        break;
    case OP10_SRA_W:   setRf(rd, (int32_t)rj_v >> (rk_v & 0x1Fu));               break;
    case OP10_ROTR_W:  { uint32_t sa=rk_v&0x1Fu; setRf(rd,(rj_v>>sa)|(rj_v<<(32-sa))); } break;
    case OP10_MUL_W:   setRf(rd, (uint32_t)((int32_t)rj_v*(int32_t)rk_v));       break;
    case OP10_MULH_W:  setRf(rd, (uint32_t)(((int64_t)(int32_t)rj_v*(int64_t)(int32_t)rk_v)>>32)); break;
    case OP10_MULH_WU: setRf(rd, (uint32_t)(((uint64_t)rj_v*(uint64_t)rk_v)>>32)); break;
    case OP10_DIV_W:   setRf(rd, rk_v?(uint32_t)((int32_t)rj_v/(int32_t)rk_v):0); break;
    case OP10_MOD_W:   setRf(rd, rk_v?(uint32_t)((int32_t)rj_v%(int32_t)rk_v):0); break;
    case OP10_DIV_WU:  setRf(rd, rk_v?rj_v/rk_v:0);                              break;
    case OP10_MOD_WU:  setRf(rd, rk_v?rj_v%rk_v:0);                              break;
    case OP10_SLT:     setRf(rd, (int32_t)rj_v<(int32_t)rk_v?1:0);               break;
    case OP10_SLTU:    setRf(rd, rj_v<rk_v?1:0);                                 break;
    case OP10_MASKEQZ: setRf(rd, rk_v==0?rj_v:0);                                break;
    case OP10_MASKNEZ: setRf(rd, rk_v!=0?rj_v:0);                                break;
    default:
        fprintf(stderr, "[ref] 未识别指令 0x%08x pc=0x%08x (op6=%x op10=%x op17=%x)\n",
                inst, pc, op6, op10, op17);
        raiseException(ECODE_INE, 0);
    }
    pc = next_pc;
}

// ── 执行步进 ──────────────────────────────────────────────────────────────────
void Simulator::step(uint64_t n) {
    while (n-- && state == SIM_RUNNING) {
        try {
            uint32_t inst = fetch(pc);
            execute(inst);
        } catch (const CPUTrap&) {
            // PC 已由 raiseException 设置为例外入口，继续执行
        }
        rf[0] = 0;  // r0 恒为 0
    }
}
