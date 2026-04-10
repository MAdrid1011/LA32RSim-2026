#pragma once
#include <cstdint>

// ── LA32R 寄存器约定 ──────────────────────────────────────────────────────────
// r0=zero, r1=ra, r2=tp, r3=sp, r4=a0 ... r11=a7, r12=t0 ... r20=t8
// r21=reserved, r22=fp, r23=s0 ... r31=s8

// ── 指令编码字段提取宏 ────────────────────────────────────────────────────────
#define BITS(x, hi, lo)  (((x) >> (lo)) & ((1u << ((hi)-(lo)+1)) - 1))
#define SEXT(x, n)       ((int32_t)(((x) ^ (1u<<((n)-1))) - (1u<<((n)-1))))

// 常用字段
#define GET_RD(inst)    BITS(inst,  4,  0)
#define GET_RJ(inst)    BITS(inst,  9,  5)
#define GET_RK(inst)    BITS(inst, 14, 10)
#define GET_OP6(inst)   BITS(inst, 31, 26)
#define GET_OP8(inst)   BITS(inst, 31, 24)
#define GET_OP10(inst)  BITS(inst, 31, 22)
#define GET_OP12(inst)  BITS(inst, 31, 20)
#define GET_OP14(inst)  BITS(inst, 31, 18)
#define GET_OP17(inst)  BITS(inst, 31, 15)
#define GET_OP22(inst)  BITS(inst, 31, 10)

// 立即数字段
#define GET_SI12(inst)  SEXT(BITS(inst, 21, 10), 12)
#define GET_SI14(inst)  SEXT(BITS(inst, 23, 10), 14)
#define GET_SI16(inst)  SEXT(BITS(inst, 25, 10), 16)
#define GET_SI20(inst)  SEXT(BITS(inst, 24,  5), 20)
#define GET_UI5(inst)   BITS(inst, 14, 10)
#define GET_UI12(inst)  BITS(inst, 21, 10)
#define GET_CODE(inst)  BITS(inst, 14,  0)   // break/syscall code

// 偏移字段（跳转）
#define GET_OFF16(inst) SEXT((BITS(inst,25,10)), 16)
#define GET_OFF21(inst) SEXT((BITS(inst,25,10) | (BITS(inst,4,0)<<16)), 21)
#define GET_OFF26(inst) SEXT((BITS(inst,25,10) | (BITS(inst,9,0)<<16)), 26)

// CSR 字段
#define GET_CSR(inst)   BITS(inst, 23, 10)
#define GET_RD_CSR(inst) BITS(inst,4,0)
#define GET_RJ_CSR(inst) BITS(inst,9,5)

// ── 操作码常量 ────────────────────────────────────────────────────────────────

// 2R 类
#define OP_REVB_2H    0x0000C000u
#define OP_REVB_4H    0x0000C001u
#define OP_REVB_2W    0x0000C002u
#define OP_REVB_D     0x0000C003u
#define OP_BITREV_4B  0x0000C004u
#define OP_CLZ_W      0x00001400u
#define OP_CTZ_W      0x00001401u
#define OP_EXT_W_B    0x00005C00u
#define OP_EXT_W_H    0x00005800u

// 3R 类和移位立即数类：操作码在 bits[31:15]（17位），mask=0xffff8000
// 常量值 = 基准指令编码 >> 15
// 3R ALU (base encoding >> 15)
#define OP10_ADD_W    0x020u  // 0x00100000 >> 15
#define OP10_SUB_W    0x022u  // 0x00110000 >> 15
#define OP10_SLT      0x024u  // 0x00120000 >> 15
#define OP10_SLTU     0x025u  // 0x00128000 >> 15
#define OP10_MASKEQZ  0x026u  // 0x00130000 >> 15 (approx)
#define OP10_MASKNEZ  0x027u  // 0x00138000 >> 15 (approx)
#define OP10_NOR      0x028u  // 0x00140000 >> 15
#define OP10_AND      0x029u  // 0x00148000 >> 15
#define OP10_OR       0x02Au  // 0x00150000 >> 15
#define OP10_XOR      0x02Bu  // 0x00158000 >> 15
#define OP10_ORN      0x02Cu  // 0x00160000 >> 15 (approx)
#define OP10_ANDN     0x02Du  // 0x00168000 >> 15 (approx)
#define OP10_SLL_W    0x02Eu  // 0x00170000 >> 15
#define OP10_SRL_W    0x02Fu  // 0x00178000 >> 15
#define OP10_SRA_W    0x030u  // 0x00180000 >> 15
#define OP10_ROTR_W   0x036u  // 0x001b0000 >> 15 (approx)
#define OP10_MUL_W    0x038u  // 0x001c0000 >> 15
#define OP10_MULH_W   0x039u  // 0x001c8000 >> 15
#define OP10_MULH_WU  0x03Au  // 0x001d0000 >> 15
#define OP10_DIV_W    0x040u  // 0x00200000 >> 15
#define OP10_MOD_W    0x041u  // 0x00208000 >> 15
#define OP10_DIV_WU   0x042u  // 0x00210000 >> 15
#define OP10_MOD_WU   0x043u  // 0x00218000 >> 15
#define OP10_BREAK    0x054u  // 0x002a0000 >> 15
#define OP10_SYSCALL  0x056u  // 0x002b0000 >> 15
// 移位立即数 (2RI5 format, opcode in bits[31:15])
#define OP10_SLLI_W   0x081u  // 0x00408000 >> 15
#define OP10_SRLI_W   0x089u  // 0x00448000 >> 15
#define OP10_SRAI_W   0x091u  // 0x00488000 >> 15
#define OP10_ROTRI_W  0x099u  // 0x004c8000 >> 15 (approx)

// 2RI12 立即数类：操作码在 bits[31:22]（10位），常量值 = 基准编码 >> 22
#define OP10_ADDI_W   0x00Au  // 0x02800000 >> 22
#define OP10_SLTI     0x008u  // 0x02000000 >> 22
#define OP10_SLTUI    0x009u  // 0x02400000 >> 22
#define OP10_ANDI     0x00Du  // 0x03400000 >> 22
#define OP10_ORI      0x00Eu  // 0x03800000 >> 22
#define OP10_XORI     0x00Fu  // 0x03c00000 >> 22
// Load/Store (2RI12 format, op10 = bits[31:22])
#define OP10_LD_B     0xA0u  // 0x28000000 >> 22
#define OP10_LD_H     0xA1u  // 0x28400000 >> 22
#define OP10_LD_W     0xA2u  // 0x28800000 >> 22
#define OP10_ST_B     0xA4u  // 0x29000000 >> 22
#define OP10_ST_H     0xA5u  // 0x29400000 >> 22
#define OP10_ST_W     0xA6u  // 0x29800000 >> 22
#define OP10_LD_BU    0xA8u  // 0x2a000000 >> 22
#define OP10_LD_HU    0xA9u  // 0x2a400000 >> 22

// 跳转类
#define OP6_BEQ       0x16u
#define OP6_BNE       0x17u
#define OP6_BLT       0x18u
#define OP6_BGE       0x19u
#define OP6_BLTU      0x1Au
#define OP6_BGEU      0x1Bu
#define OP6_B         0x14u
#define OP6_BL        0x15u
#define OP6_JIRL      0x13u

// CSR 类
#define OP8_CSRRD     0x04u  // inst[31:24] = 0x04, rj=0
#define OP8_CSRWR     0x04u  // inst[31:24] = 0x04, rj=1
#define OP8_CSRXCHG   0x04u  // inst[31:24] = 0x04, rj!=0,1
#define OP6_LU12I_W_G 0x0Au  // inst[31:25] = 0b0001010

// BREAK/SYSCALL 指令编码
// break 0   = 0x002a0005 (LA32R LoongArch 编码)
// good trap 约定：执行 break 0，r4(a0)=0 则 GOOD TRAP
#define INST_BREAK0   0x002a0005u
#define INST_SYSCALL0 0x002b0000u

// CSR 寄存器编号（LA32R 子集）
#define CSR_CRMD    0x000u  // 当前模式
#define CSR_PRMD    0x001u  // 例外前模式
#define CSR_EUEN    0x002u  // 扩展部件使能
#define CSR_ECFG    0x004u  // 例外控制
#define CSR_ESTAT   0x005u  // 例外状态
#define CSR_ERA     0x006u  // 例外返回地址
#define CSR_BADV    0x007u  // 出错虚地址
#define CSR_EENTRY  0x00Cu  // 例外入口地址
#define CSR_TLBIDX  0x010u
#define CSR_TLBEHI  0x011u
#define CSR_TLBELO0 0x012u
#define CSR_TLBELO1 0x013u
#define CSR_ASID    0x018u
#define CSR_PGDL    0x019u
#define CSR_PGDH    0x01Au
#define CSR_SAVE0   0x030u
#define CSR_SAVE1   0x031u
#define CSR_SAVE2   0x032u
#define CSR_SAVE3   0x033u
#define CSR_TID     0x040u
#define CSR_TCFG    0x041u
#define CSR_TVAL    0x042u
#define CSR_TICLR   0x044u
#define CSR_LLBCTL  0x060u
#define CSR_TLBRENTRY 0x088u
#define CSR_CTAG    0x098u
#define CSR_DMW0    0x180u
#define CSR_DMW1    0x181u

// CRMD 字段
#define CRMD_PLV_MASK  0x3u
#define CRMD_IE        (1u << 2)
#define CRMD_DA        (1u << 3)
#define CRMD_PG        (1u << 4)
#define CRMD_DATF_MASK (0x3u << 5)
#define CRMD_DATM_MASK (0x3u << 7)

// 例外码
#define ECODE_PIL   0x01u
#define ECODE_PIS   0x02u
#define ECODE_PIF   0x03u
#define ECODE_PME   0x04u
#define ECODE_PPI   0x07u
#define ECODE_ADEF  0x08u
#define ECODE_ALE   0x09u
#define ECODE_SYS   0x0Bu
#define ECODE_BRK   0x0Cu
#define ECODE_INE   0x0Du
#define ECODE_INT   0x00u
