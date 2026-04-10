#include "asm.h"
#include "regdef.h"
#include "cpu_cde.h"
#include "lab_config.h"

.global cacop_disable
cacop_disable:
    li.w        t0, 0x8 
    li.w        t1, 0x18 
    csrxchg   t0, t1, csr_crmd
    csrwr     zero, csr_dmw0 
    csrwr     zero, csr_dmw1  
    jirl      zero, ra, 0x0
/*
 *  exception handle
 */
.global exception_handler
.align 6
exception_handler:
1:  
    GET_EXIMM(t1)
    li.w    t0, IMM_SYSCALL 
    beq   t1, t0, syscall_ex  #syscall  
    li.w    t0, IMM_BREAK 
    beq   t1, t0, brk_ex      #break 
    li.w    t0, IMM_INE 
    beq   t1, t0, ine_ex      #reserved inst 
    li.w    t0, IMM_INT 
    beq   t1, t0, int_ex      #interrupt 
    li.w    t0, IMM_ADEF 
    beq   t1, t0, adef_ex     #adef 
    li.w    t0, IMM_ALE 
    beq   t1, t0, ale_ex      #ale 
    li.w    t0, IMM_PME 
    beq   t1, t0, pme_ex      #pme
    li.w    t0, IMM_LDINV
    beq   t1, t0, ld_page_invalid
    li.w    t0, IMM_STINV
    beq   t1, t0, st_page_invalid
    li.w    t0, IMM_TLBPRV
    beq   t1, t0, tlb_prl_error
    li.w    t0, IMM_TLBM
    beq   t1, t0, tlb_page_modified_error
    li.w    t0, IMM_FETCHINV
    beq   t1, t0, fetch_page_invalid
    li.w    t0, IMM_TLBADEM
    beq   t1, t0, tlb_adem_error
    li.w    t0, IMM_TLBADEF
    beq   t1, t0, tlb_adef_error
    li.w    t0, IMM_KERNEL 
    beq   t1, t0, syscall_bak_kernel_mode
	break 0

syscall_ex:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x000b0000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    lu12i.w  s2, 0x10 
    b        ex_finish  

brk_ex: 
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x000c0000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    lu12i.w  s2, 0x20 
    b        ex_finish  

ine_ex:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0xd0000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    lu12i.w  s2, 0x30 
    b        ex_finish 

int_ex:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00000000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    li.w       t1, 0x1 
    csrwr    t1, csr_ticlr 
    bne      t1, zero, ex_finish 
    li.w       t1, 0x0 
    li.w       t0, 0x3 
    csrxchg  t1, t0, csr_estat 
    csrrd    t1, csr_estat
    li.w       t0, 0x1fff 
    and      t0, t0, t1 
    bne      t0, zero, ex_finish 
    lu12i.w  s2, 0x40 
    b        ex_finish 

adef_ex:
    add.w    s2, zero, zero
    csrwr    s5, csr_era
    bne      s4, s5, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00080000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    lu12i.w  s2, 0x50 
    b        ex_finish  

ale_ex:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00090000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    lu12i.w  s2, 0x60 
    b        ex_finish  
    
pme_ex:
    add.w    s2, zero, zero 
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00040000
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish 
    srli.w   s8, s8, 13 
    slli.w   s8, s8, 13
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x70 
    b        ex_finish  

ld_page_invalid:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00010000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x1
    b        ex_finish  
    
st_page_invalid:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00020000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x2
    b        ex_finish  
    
tlb_prl_error:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00070000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x7
    b        ex_finish  
    
tlb_page_modified_error:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00040000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t5, t0, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x4
    b        ex_finish  
     
fetch_page_invalid:
    add.w    s2, zero, zero
    li.w       t0, 0xffffffff
    csrrd    t0, csr_era 
    bne      s8, t0, ex_finish 
    addi.w   t0, s4, 0x0
    csrwr    t0, csr_era
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00030000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t5, t0, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0x3
    b        ex_finish  

tlb_adem_error:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00480000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish 
    csrrd    t0, csr_tlbehi
    bne      t0, zero, ex_finish 
    lu12i.w  s2, 0x81
    b        ex_finish  
     
tlb_adef_error:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s8, t0, ex_finish 
    addi.w   t0, s4, 0x0
    csrwr    t0, csr_era
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x00080000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t5, t0, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish 
    csrrd    t0, csr_tlbehi
    bne      t0, zero, ex_finish 
    lu12i.w  s2, 0x80
    b        ex_finish  

syscall_bak_kernel_mode:
    li.w       t0, 0x3 
    csrxchg  zero, t0, csr_prmd 
    csrrd    t1, csr_era 
    addi.w   t1, t1, 0x4 
    csrwr    t1, csr_era 
    ertn

// .org TLBREBASE   //TLBREBASE
.global tlbrebase
.align 6
tlbrebase:
    add.w    s2, zero, zero
    csrrd    t0, csr_era 
    bne      s4, t0, ex_finish 
    csrrd    t0, csr_estat 
    li.w       t1, 0x7fff0000 
    and      t0, t0, t1 
    li.w       t1, 0x003f0000 
    bne      t1, t0, ex_finish 
    csrrd    t0, csr_crmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    li.w       t1, 0x0 
    bne      t1, t0, ex_finish
    csrrd    t0, csr_prmd 
    li.w       t1, 0x7 
    and      t0, t0, t1 
    bne      t0, t5, ex_finish 
    csrrd    t0, csr_badv
    bne      t0, s8, ex_finish
    csrrd    t0, csr_tlbehi
    bne      t0, s8, ex_finish
    lu12i.w  s2, 0xe
    b        ex_finish  
 
ex_finish:
    add.w    zero, zero, zero
    csrrd    t1, csr_era 
    addi.w   t1, t1, 0x4 
    csrwr    t1, csr_era 
    addi.w   t1, s2, 0x0
    bne      s2, zero, ex_ret
    lu12i.w  s2, 0xffff 
ex_ret:
    ertn