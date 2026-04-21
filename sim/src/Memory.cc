#include "Memory.h"
#include "common.h"
#include "VCPU.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

// ── MMIO（共用） ──────────────────────────────────────────────────────────────
uint32_t MemoryBase::mmioRead(uint32_t addr) {
    // 目前仅 UART（只写），读返回 0
    (void)addr;
    return 0;
}

// base-port trm.c writes to 0xa00003f8 (16550 UART DATA register)
#define UART_ADDR2 0xa00003f8u

void MemoryBase::mmioWrite(uint32_t addr, uint32_t data, uint8_t wmask) {
#ifdef CONFIG_UART_STDOUT
    bool is_uart = ((addr & ~0x7u) == (UART_ADDR  & ~0x7u)) ||
                   ((addr & ~0x7u) == (UART_ADDR2 & ~0x7u));
    if (is_uart) {
        for (int i = 0; i < 4; i++) {
            if ((wmask >> i) & 1) {
                uint8_t c = (data >> (i * 8)) & 0xFF;
                if (c) putchar(c);
            }
        }
        fflush(stdout);
        return;
    }
#endif
    (void)addr; (void)data; (void)wmask;
}

// ── pmem 通用加载 ─────────────────────────────────────────────────────────────
static void loadBinToMap(const char* path, PMemMap& pmem, uint32_t base) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { fprintf(stderr, "[mem] 无法打开: %s\n", path); exit(1); }
    size_t sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    for (size_t i = 0; i < sz; i++) {
        uint32_t addr = base + i;
        uint32_t key  = addr >> 2;
        int      bit  = (addr & 3) * 8;
        pmem[key] = (pmem[key] & ~(0xFFu << bit)) | (uint32_t(buf[i]) << bit);
    }
    fprintf(stdout, "[mem] 加载镜像 %s (%zu 字节) 到 0x%08x\n", path, sz, base);
}

// ── 字节粒度读取辅助 ──────────────────────────────────────────────────────────
static uint32_t mapRead(const PMemMap& pm, uint32_t addr, int bytes) {
    uint32_t val = 0;
    for (int i = 0; i < bytes; i++) {
        uint32_t a = addr + i;
        auto it = pm.find(a >> 2);
        if (it != pm.end()) val |= (uint32_t)((it->second >> ((a & 3)*8)) & 0xFF) << (i*8);
    }
    return val;
}

static void mapWrite(PMemMap& pm, uint32_t addr, uint32_t data, uint8_t wmask) {
    for (int i = 0; i < 4; i++) {
        if (!((wmask >> i) & 1)) continue;
        uint32_t a = addr + i;
        uint32_t& word = pm[a >> 2];
        int bit = (a & 3) * 8;
        word = (word & ~(0xFFu << bit)) | (((data >> (i*8)) & 0xFF) << bit);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// 简单总线内存实现
// ═══════════════════════════════════════════════════════════════════════════════
#ifdef CONFIG_MEM_SIMPLE

SimpleMemory::SimpleMemory() {}

void SimpleMemory::loadImage(const char* path) {
    loadBinToMap(path, pmem, CONFIG_MEM_BASE);
}

uint32_t SimpleMemory::debugRead(uint32_t addr) const {
    return mapRead(pmem, addr, 4);
}

uint32_t SimpleMemory::pmemRead(uint32_t addr, int bytes) const {
    return mapRead(pmem, addr, bytes);
}

void SimpleMemory::pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle) {
    mapWrite(pmem, addr, data, wmask);
#ifdef CONFIG_MEM_WTRACE
    int nbytes = __builtin_popcount(wmask);
    // 记录写入的每个字节
    for (int i = 0; i < 4; i++) {
        if ((wmask >> i) & 1) wtrace.record(addr + i, 1, cycle);
    }
    (void)nbytes;
#else
    (void)cycle;
#endif
}

// 简单总线：io_mem_valid/ready/addr/wdata/wstrb/rdata
// 当 valid=1 且 wstrb=0 时为读，wstrb!=0 时为写
// 本实现：ready 始终为1（单周期响应）
void SimpleMemory::write(VCPU* cpu, uint64_t cycle) {
    // 下降沿：处理写操作
    if (cpu->io_mem_valid && cpu->io_mem_wstrb != 0) {
        uint32_t addr  = cpu->io_mem_addr;
        uint32_t data  = cpu->io_mem_wdata;
        uint8_t  wmask = cpu->io_mem_wstrb;
#ifdef CONFIG_MTRACE
        fprintf(stdout, "[mtrace] cycle=%-8llu WRITE addr=0x%08x data=0x%08x wmask=0x%x\n",
                (unsigned long long)cycle, addr, data, wmask);
#endif
        if (in_pmem(addr)) {
            pmemWrite(addr, data, wmask, cycle);
        } else {
            mmioWrite(addr, data, wmask);
        }
    }
}

void SimpleMemory::read(VCPU* cpu, uint64_t cycle) {
    // 上升沿前：处理读操作，设置 ready/rdata
    cpu->io_mem_ready = 1;
    if (cpu->io_mem_valid && cpu->io_mem_wstrb == 0) {
        uint32_t addr = cpu->io_mem_addr;
        uint32_t rdata;
        if (in_pmem(addr)) {
            rdata = pmemRead(addr, 4);
        } else {
            rdata = mmioRead(addr);
        }
#ifdef CONFIG_MTRACE
        fprintf(stdout, "[mtrace] cycle=%-8llu READ  addr=0x%08x data=0x%08x\n",
                (unsigned long long)cycle, addr, rdata);
#endif
        cpu->io_mem_rdata = rdata;
    }
    (void)cycle;
}

#endif // CONFIG_MEM_SIMPLE

// ═══════════════════════════════════════════════════════════════════════════════
// 哈佛分离双端口内存实现（CONFIG_MEM_HARVARD）
//   io_imem_*  : 只读指令端口，固定 8 字节返回（64-bit rdata）
//   io_dmem_*  : 读写数据端口，变 size ∈ {1,2,4,8} 字节（64-bit rdata/wdata）
//   两个端口互不干扰，零延迟（ready 始终为 1）
// ═══════════════════════════════════════════════════════════════════════════════
#ifdef CONFIG_MEM_HARVARD

HarvardMemory::HarvardMemory() {}

void HarvardMemory::loadImage(const char* path) {
    loadBinToMap(path, pmem, CONFIG_MEM_BASE);
}

uint32_t HarvardMemory::debugRead(uint32_t addr) const {
    return mapRead(pmem, addr, 4);
}

uint32_t HarvardMemory::pmemRead(uint32_t addr, int bytes) const {
    return mapRead(pmem, addr, bytes);
}

void HarvardMemory::pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle) {
    mapWrite(pmem, addr, data, wmask);
#ifdef CONFIG_MEM_WTRACE
    for (int i = 0; i < 4; i++) {
        if ((wmask >> i) & 1) wtrace.record(addr + i, 1, cycle);
    }
#else
    (void)cycle;
#endif
}

// 读最多 8 个字节（低位先出），bytes ∈ {1,2,4,8}，高位补零
uint64_t HarvardMemory::readWide(uint32_t addr, int bytes) {
    uint64_t v = 0;
    int lo_bytes = bytes <= 4 ? bytes : 4;
    int hi_bytes = bytes <= 4 ? 0     : bytes - 4;
    if (in_pmem(addr)) {
        v = pmemRead(addr, lo_bytes);
    } else {
        v = mmioRead(addr);
    }
    if (hi_bytes > 0) {
        uint32_t hi = 0;
        if (in_pmem(addr + 4)) {
            hi = pmemRead(addr + 4, hi_bytes);
        } else {
            hi = mmioRead(addr + 4);
        }
        v |= ((uint64_t)hi) << 32;
    }
    return v;
}

// 写最多 8 个字节（按 8-bit wstrb 逐字节），跨 4B 边界拆两次
void HarvardMemory::writeWide(uint32_t addr, uint64_t data, uint8_t wstrb, uint64_t cycle) {
    uint8_t  wm_lo = wstrb & 0xF;
    uint8_t  wm_hi = (wstrb >> 4) & 0xF;
    uint32_t d_lo  = (uint32_t)(data & 0xFFFFFFFFu);
    uint32_t d_hi  = (uint32_t)(data >> 32);
    if (wm_lo) {
        if (in_pmem(addr)) pmemWrite(addr, d_lo, wm_lo, cycle);
        else               mmioWrite(addr, d_lo, wm_lo);
    }
    if (wm_hi) {
        if (in_pmem(addr + 4)) pmemWrite(addr + 4, d_hi, wm_hi, cycle);
        else                   mmioWrite(addr + 4, d_hi, wm_hi);
    }
}

// 下降沿：处理 dmem 写
void HarvardMemory::write(VCPU* cpu, uint64_t cycle) {
    if (cpu->io_dmem_valid && cpu->io_dmem_wstrb != 0) {
        uint32_t addr  = cpu->io_dmem_addr;
        uint64_t data  = cpu->io_dmem_wdata;
        uint8_t  wstrb = cpu->io_dmem_wstrb;
#ifdef CONFIG_MTRACE
        fprintf(stdout, "[mtrace] cycle=%-8llu D-WR addr=0x%08x data=0x%016llx wstrb=0x%02x size=%u\n",
                (unsigned long long)cycle, addr, (unsigned long long)data, wstrb,
                (unsigned)cpu->io_dmem_size);
#endif
        writeWide(addr, data, wstrb, cycle);
    }
}

// 上升沿前：处理 imem 读 / dmem 读，零延迟
void HarvardMemory::read(VCPU* cpu, uint64_t cycle) {
    cpu->io_imem_ready = 1;
    cpu->io_dmem_ready = 1;

    // ── 指令端口（只读，size 字段告知需要多少字节） ─────────────────────────
    if (cpu->io_imem_valid) {
        uint32_t addr = cpu->io_imem_addr;
        int      bytes = 1 << (cpu->io_imem_size & 0x3);
        uint64_t rdata = readWide(addr, bytes);
#ifdef CONFIG_MTRACE
        fprintf(stdout, "[mtrace] cycle=%-8llu I-RD addr=0x%08x size=%d data=0x%016llx\n",
                (unsigned long long)cycle, addr, bytes, (unsigned long long)rdata);
#endif
        cpu->io_imem_rdata = rdata;
    }

    // ── 数据端口读（wstrb == 0） ────────────────────────────────────────────
    if (cpu->io_dmem_valid && cpu->io_dmem_wstrb == 0) {
        uint32_t addr  = cpu->io_dmem_addr;
        int      bytes = 1 << (cpu->io_dmem_size & 0x3);
        uint64_t rdata = readWide(addr, bytes);
#ifdef CONFIG_MTRACE
        fprintf(stdout, "[mtrace] cycle=%-8llu D-RD addr=0x%08x size=%d data=0x%016llx\n",
                (unsigned long long)cycle, addr, bytes, (unsigned long long)rdata);
#endif
        cpu->io_dmem_rdata = rdata;
    }
    (void)cycle;
}

#endif // CONFIG_MEM_HARVARD

// ═══════════════════════════════════════════════════════════════════════════════
// AXI 内存实现
// ═══════════════════════════════════════════════════════════════════════════════
#ifdef CONFIG_MEM_AXI

AXIMemory::AXIMemory() {}

void AXIMemory::loadImage(const char* path) {
    loadBinToMap(path, pmem, CONFIG_MEM_BASE);
}

uint32_t AXIMemory::debugRead(uint32_t addr) const {
    return mapRead(pmem, addr, 4);
}

uint32_t AXIMemory::pmemRead(uint32_t addr, int bytes) const {
    return mapRead(pmem, addr, bytes);
}

void AXIMemory::pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle) {
    mapWrite(pmem, addr, data, wmask);
#ifdef CONFIG_MEM_WTRACE
    for (int i = 0; i < 4; i++) {
        if ((wmask >> i) & 1) wtrace.record(addr + i, 1, cycle);
    }
#else
    (void)cycle;
#endif
}

#ifdef CONFIG_MEM_AXI_BACKPRESSURE
bool AXIMemory::randReady() {
    bp_seed ^= bp_seed << 13;
    bp_seed ^= bp_seed >> 17;
    bp_seed ^= bp_seed << 5;
    return (bp_seed & 3) != 0;  // 75% 概率 ready=1
}
#endif

// AXI 写通道（下降沿后调用）
void AXIMemory::write(VCPU* cpu, uint64_t cycle) {
    // ── AW 通道 ──────────────────────────────────────────────────────────────
    if (wstate == AW_IDLE) {
        cpu->io_axi_aw_ready = 1;
#ifdef CONFIG_MEM_AXI_BACKPRESSURE
        cpu->io_axi_aw_ready = randReady() ? 1 : 0;
#endif
        cpu->io_axi_w_ready  = 0;
        cpu->io_axi_b_valid  = 0;
        if (cpu->io_axi_aw_valid && cpu->io_axi_aw_ready) {
            awaddr = cpu->io_axi_aw_addr;
            awlen  = cpu->io_axi_aw_len + 1;
            awsize = 1u << cpu->io_axi_aw_size;
            wcnt   = 0;
            wstate = AW_DATA;
        }
    }
    // ── W 通道 ───────────────────────────────────────────────────────────────
    else if (wstate == AW_DATA) {
        cpu->io_axi_aw_ready = 0;
        cpu->io_axi_w_ready  = 1;
#ifdef CONFIG_MEM_AXI_BACKPRESSURE
        cpu->io_axi_w_ready = randReady() ? 1 : 0;
#endif
        cpu->io_axi_b_valid  = 0;
        if (cpu->io_axi_w_valid && cpu->io_axi_w_ready) {
            uint32_t addr  = awaddr + wcnt * awsize;
            uint8_t  wmask = cpu->io_axi_w_strb;
            uint32_t data  = cpu->io_axi_w_data;
#ifdef CONFIG_MTRACE
            fprintf(stdout, "[mtrace] cycle=%-8llu AXI-W addr=0x%08x data=0x%08x wmask=0x%x\n",
                    (unsigned long long)cycle, addr, data, wmask);
#endif
            if (in_pmem(addr)) {
                pmemWrite(addr, data, wmask, cycle);
            } else {
                mmioWrite(addr, data, wmask);
            }
            wcnt++;
            if (cpu->io_axi_w_last) wstate = AW_RESP;
        }
    }
    // ── B 通道 ───────────────────────────────────────────────────────────────
    else if (wstate == AW_RESP) {
        cpu->io_axi_aw_ready = 0;
        cpu->io_axi_w_ready  = 0;
        cpu->io_axi_b_valid  = 1;
        cpu->io_axi_b_resp   = 0;
        cpu->io_axi_b_id     = 0;
        if (cpu->io_axi_b_ready) wstate = AW_IDLE;
    }
    (void)cycle;
}

// AXI 读通道（上升沿前调用）
void AXIMemory::read(VCPU* cpu, uint64_t cycle) {
    if (rstate == AR_IDLE) {
        cpu->io_axi_ar_ready = 1;
#ifdef CONFIG_MEM_AXI_BACKPRESSURE
        cpu->io_axi_ar_ready = randReady() ? 1 : 0;
#endif
        cpu->io_axi_r_valid  = 0;
        cpu->io_axi_r_last   = 0;
        rcnt = 0;
        if (cpu->io_axi_ar_valid && cpu->io_axi_ar_ready) {
            araddr = cpu->io_axi_ar_addr;
            arlen  = cpu->io_axi_ar_len + 1;
            arsize = 1u << cpu->io_axi_ar_size;
            rstate = AR_RESP;
        }
    }
    else if (rstate == AR_RESP) {
        cpu->io_axi_ar_ready = 0;
        cpu->io_axi_r_valid  = 1;
        cpu->io_axi_r_last   = (rcnt == arlen - 1);
        cpu->io_axi_r_resp   = 0;
        cpu->io_axi_r_id     = 0;
        uint32_t addr = araddr + rcnt * arsize;
        uint32_t rdata;
        if (in_pmem(addr)) {
            rdata = pmemRead(addr, arsize < 4 ? arsize : 4);
        } else {
            rdata = mmioRead(addr);
        }
#ifdef CONFIG_MTRACE
        if (cpu->io_axi_r_ready) {
            fprintf(stdout, "[mtrace] cycle=%-8llu AXI-R addr=0x%08x data=0x%08x\n",
                    (unsigned long long)cycle, addr, rdata);
        }
#endif
        cpu->io_axi_r_data = rdata;
        if (cpu->io_axi_r_valid && cpu->io_axi_r_ready) {
            rcnt++;
            if (cpu->io_axi_r_last) rstate = AR_IDLE;
        }
    }
    (void)cycle;
}

#endif // CONFIG_MEM_AXI
