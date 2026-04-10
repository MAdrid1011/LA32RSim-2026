#pragma once
#include <cstdint>
#include <unordered_map>
#include "LA32R_ISA.h"

// LA32R CSR 寄存器组
struct CSRFile {
    uint32_t crmd   = 0x8;   // DA=1, PG=0, PLV=0（直接地址翻译模式）
    uint32_t prmd   = 0;
    uint32_t euen   = 0;
    uint32_t ecfg   = 0;
    uint32_t estat  = 0;
    uint32_t era    = 0;
    uint32_t badv   = 0;
    uint32_t eentry = 0;
    uint32_t tlbidx = 0;
    uint32_t tlbehi = 0;
    uint32_t tlbelo0= 0;
    uint32_t tlbelo1= 0;
    uint32_t asid   = 0;
    uint32_t pgdl   = 0;
    uint32_t pgdh   = 0;
    uint32_t save[4]= {};
    uint32_t tid    = 0;
    uint32_t tcfg   = 0;
    uint32_t tval   = 0;
    uint32_t ticlr  = 0;
    uint32_t llbctl = 0;
    uint32_t tlbrentry = 0;
    uint32_t dmw[2] = {};

    uint32_t read(uint32_t csr_no) const {
        switch (csr_no) {
        case CSR_CRMD:    return crmd;
        case CSR_PRMD:    return prmd;
        case CSR_EUEN:    return euen;
        case CSR_ECFG:    return ecfg;
        case CSR_ESTAT:   return estat;
        case CSR_ERA:     return era;
        case CSR_BADV:    return badv;
        case CSR_EENTRY:  return eentry;
        case CSR_TLBIDX:  return tlbidx;
        case CSR_TLBEHI:  return tlbehi;
        case CSR_TLBELO0: return tlbelo0;
        case CSR_TLBELO1: return tlbelo1;
        case CSR_ASID:    return asid;
        case CSR_PGDL:    return pgdl;
        case CSR_PGDH:    return pgdh;
        case CSR_SAVE0:   return save[0];
        case CSR_SAVE1:   return save[1];
        case CSR_SAVE2:   return save[2];
        case CSR_SAVE3:   return save[3];
        case CSR_TID:     return tid;
        case CSR_TCFG:    return tcfg;
        case CSR_TVAL:    return tval;
        case CSR_TICLR:   return ticlr;
        case CSR_LLBCTL:  return llbctl;
        case CSR_TLBRENTRY: return tlbrentry;
        case CSR_DMW0:    return dmw[0];
        case CSR_DMW1:    return dmw[1];
        default:          return 0;
        }
    }

    void write(uint32_t csr_no, uint32_t val) {
        switch (csr_no) {
        case CSR_CRMD:    crmd    = val & 0x1FFu; break;
        case CSR_PRMD:    prmd    = val & 0x7u;   break;
        case CSR_EUEN:    euen    = val & 0x1u;   break;
        case CSR_ECFG:    ecfg    = val & 0x1FFBFFu; break;
        case CSR_ESTAT:   estat   = (estat & ~0x1FFu) | (val & 0x1FFu); break;
        case CSR_ERA:     era     = val;           break;
        case CSR_BADV:    badv    = val;           break;
        case CSR_EENTRY:  eentry  = val & ~0x3Fu;  break;
        case CSR_TLBIDX:  tlbidx  = val;            break;
        // LA32R 手册规定 TLBEHI 仅 bits[31:13] 为 VPPN（可写），
        // bits[12:0] 保留且始终为 0。写入时需清除低 13 位。
        case CSR_TLBEHI:  tlbehi  = val & ~0x1FFFu; break;
        case CSR_TLBELO0: tlbelo0 = val;           break;
        case CSR_TLBELO1: tlbelo1 = val;           break;
        case CSR_ASID:    asid    = val & 0x3FFu;  break;
        case CSR_PGDL:    pgdl    = val & ~0xFFFu; break;
        case CSR_PGDH:    pgdh    = val & ~0xFFFu; break;
        case CSR_SAVE0:   save[0] = val;           break;
        case CSR_SAVE1:   save[1] = val;           break;
        case CSR_SAVE2:   save[2] = val;           break;
        case CSR_SAVE3:   save[3] = val;           break;
        case CSR_TID:     tid     = val;           break;
        case CSR_TCFG:    tcfg    = val & 0xFFFFFFFFu; break;
        case CSR_TVAL:    /* read-only */           break;
        case CSR_TICLR:   if (val & 1) estat &= ~(1u<<11); break;
        case CSR_LLBCTL:  llbctl  = val & ~0x2u;   break;
        case CSR_TLBRENTRY: tlbrentry = val & ~0x3Fu; break;
        case CSR_DMW0:    dmw[0]  = val;           break;
        case CSR_DMW1:    dmw[1]  = val;           break;
        default: break;
        }
    }
};
