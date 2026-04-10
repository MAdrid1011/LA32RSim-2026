#include "regdef.h"
#include "cpu_cde.h"

.global main
main:
    li.w      t0, 0x08
    li.w      t1, 0x1f
    csrxchg t0, t1, csr_crmd
    li.w s1, 0x1c200000
    la.local t0, exception_handler
    csrwr t0, csr_eentry 
    la.local    t1, tlbrebase
    csrwr t1, csr_tlbrentry
    // bl n1_lu12i_w_test
    // bl n2_add_w_test
    // bl n3_addi_w_test
    // bl n4_sub_w_test
    // bl n5_slt_test
    // bl n6_sltu_test
    // bl n7_and_test
    // bl n8_or_test
    // bl n9_xor_test
    // bl n10_nor_test
    // bl n11_slli_w_test
    // bl n12_srli_w_test
    // bl n13_srai_w_test

    // bl n14_ld_w_test
    // bl n15_st_w_test

    // bl n16_beq_test
    // bl n17_bne_test
    // bl n18_bl_test
    // bl n19_jirl_test
    // bl n20_b_test

    // bl n21_pcaddu12i_test
    // bl n22_slti_test
    // bl n23_sltui_test
    // bl n24_andi_test
    // bl n25_ori_test
    // bl n26_xori_test
    // bl n27_sll_w_test
    // bl n28_sra_w_test
    // bl n29_srl_w_test

    // bl n30_div_w_test
    // bl n31_div_wu_test
    // bl n32_mul_w_test
    // bl n33_mulh_w_test
    // bl n34_mulh_wu_test
    // bl n35_mod_w_test
    // bl n36_mod_wu_test

    // bl n37_blt_test
    // bl n38_bge_test
    // bl n39_bltu_test
    // bl n40_bgeu_test

    // bl n41_ld_b_test
    // bl n42_ld_h_test
    // bl n43_ld_bu_test
    // bl n44_ld_hu_test
    // bl n45_st_b_test
    // bl n46_st_h_test
    
    // bl n47_syscall_ex_test
    // bl n48_brk_ex_test
    // bl n49_ti_ex_test
    // bl n50_ine_ex_test
    // bl n51_soft_int_ex_test
    // bl n52_adef_ex_test
    // bl n53_ale_ld_w_ex_test
    // bl n54_ale_ld_h_ex_test
    // bl n55_ale_ld_hu_ex_test
    // bl n56_ale_st_h_ex_test
    // bl n57_ale_st_w_ex_test
    // bl n58_rdcnt_test

    bl tlb_initialization
    bl n59_tlbrd_tlbwr_test
    bl tlb_initialization
    bl n60_tlbfill_test
    bl tlb_initialization
    bl n61_tlbsrch_test
    bl tlb_initialization
    bl n62_invtlb_0x0_test
    bl tlb_initialization
    bl n63_invtlb_0x1_test
    bl tlb_initialization
    bl n64_invtlb_0x2_test
    bl tlb_initialization
    bl n65_invtlb_0x3_test
    bl tlb_initialization
    bl n66_invtlb_0x4_test
    bl tlb_initialization
    bl n67_invtlb_0x5_test
    bl tlb_initialization
    bl n68_invtlb_0x6_test
    bl tlb_initialization
    bl n69_invtlb_inv_op_test
    bl tlb_initialization
    bl n70_tlb_4MB
    bl tlb_initialization
    bl n71_tlb_ex_test
    bl n72_dmw_test

    // bl cacop_disable
    // bl n73_icacop_op0
    // bl n74_dcacop_op0
    // bl n75_icacop_op1
    // bl n76_dcacop_op1
    // bl n77_icacop_op2
    // bl n78_dcacop_op2
    // bl n79_cache_writeback
    // bl n80_ti_ex_idle_test
    // bl n81_atomic_ins_test

    li.w a0, 14
    bne a0, s3, failed
    li.w a0, 0
    .word 0x80000000
failed:
    .word 0x80000000

// failed:
//     li.w a0, 1
//     NOP4
//     .word 0x80000000

