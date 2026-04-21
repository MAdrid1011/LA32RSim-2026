#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "common.h"

// 前向声明 Verilated CPU（具体类型在编译时确定）
class VCPU;

// 物理内存类型（与参考模拟器共享）
using PMemMap = std::unordered_map<uint32_t, uint32_t>;

// ── 内存写入时间戳追踪 ────────────────────────────────────────────────────────
#ifdef CONFIG_MEM_WTRACE
// 每个字节对应一个 uint64_t，记录最后一次写入该字节的仿真周期号
// 使用稀疏 map（key = 字节地址）节省内存
struct MemWTrace {
    std::unordered_map<uint32_t, uint64_t> last_write_cycle;

    void record(uint32_t addr, int nbytes, uint64_t cycle) {
        for (int i = 0; i < nbytes; i++) {
            last_write_cycle[addr + i] = cycle;
        }
    }

    // 查询某字节最后写入周期，返回 -1 表示从未写入
    int64_t query(uint32_t addr) const {
        auto it = last_write_cycle.find(addr);
        return it != last_write_cycle.end() ? (int64_t)it->second : -1;
    }

    // 打印某地址范围（word 对齐）的写入时间戳
    void dump(uint32_t addr, int nwords = 4) const {
        fprintf(stdout, "  内存写入时间戳 (addr=0x%08x):\n", addr);
        for (int w = 0; w < nwords; w++) {
            uint32_t a = addr + w * 4;
            fprintf(stdout, "  0x%08x:", a);
            for (int b = 0; b < 4; b++) {
                int64_t cyc = query(a + b);
                if (cyc < 0) fprintf(stdout, "  [never ]");
                else          fprintf(stdout, "  [cyc%-5lld]", (long long)cyc);
            }
            fprintf(stdout, "\n");
        }
    }
};
#endif

// ── 内存接口抽象基类 ──────────────────────────────────────────────────────────
class MemoryBase {
public:
    virtual ~MemoryBase() = default;

    // 每半周期调用（下降沿后：write，上升沿前：read）
    virtual void write(VCPU* cpu, uint64_t cycle) = 0;
    virtual void read (VCPU* cpu, uint64_t cycle) = 0;

    // 从文件加载二进制镜像
    virtual void loadImage(const char* path) = 0;

    // 获取 DUT 侧 pmem 引用（供 difftest 同步内存）
    virtual PMemMap& getMemRef() = 0;

    // 调试读（不经过总线协议）
    virtual uint32_t debugRead(uint32_t addr) const = 0;

    // MMIO 读写
    uint32_t mmioRead(uint32_t addr);
    void     mmioWrite(uint32_t addr, uint32_t data, uint8_t wmask);

#ifdef CONFIG_MEM_WTRACE
    MemWTrace wtrace;
    // 外部查询接口
    int64_t queryWriteCycle(uint32_t byte_addr) const {
        return wtrace.query(byte_addr);
    }
    void dumpWriteTimestamp(uint32_t addr, int nwords = 4) const {
        wtrace.dump(addr, nwords);
    }
#endif
};

// ── 简单单周期总线内存 ────────────────────────────────────────────────────────
#ifdef CONFIG_MEM_SIMPLE
class SimpleMemory : public MemoryBase {
public:
    SimpleMemory();
    void write(VCPU* cpu, uint64_t cycle) override;
    void read (VCPU* cpu, uint64_t cycle) override;
    void loadImage(const char* path) override;
    PMemMap& getMemRef() override { return pmem; }
    uint32_t debugRead(uint32_t addr) const override;

private:
    PMemMap pmem;
    uint32_t pmemRead(uint32_t addr, int bytes) const;
    void     pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle);
};
#endif

// ── 哈佛分离双端口内存（io_imem_* + io_dmem_*，变宽 64-bit） ─────────────────
#ifdef CONFIG_MEM_HARVARD
class HarvardMemory : public MemoryBase {
public:
    HarvardMemory();
    void write(VCPU* cpu, uint64_t cycle) override;
    void read (VCPU* cpu, uint64_t cycle) override;
    void loadImage(const char* path) override;
    PMemMap& getMemRef() override { return pmem; }
    uint32_t debugRead(uint32_t addr) const override;

private:
    PMemMap pmem;
    uint32_t pmemRead (uint32_t addr, int bytes) const;
    void     pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle);
    // 读出最多 8 个字节（按 bytes ∈ {1,2,4,8} 返回，高位补零）
    // 非 const：mmioRead 可能带副作用
    uint64_t readWide (uint32_t addr, int bytes);
    void     writeWide(uint32_t addr, uint64_t data, uint8_t wstrb, uint64_t cycle);
};
#endif

// ── AXI 内存 ──────────────────────────────────────────────────────────────────
#ifdef CONFIG_MEM_AXI
class AXIMemory : public MemoryBase {
public:
    AXIMemory();
    void write(VCPU* cpu, uint64_t cycle) override;
    void read (VCPU* cpu, uint64_t cycle) override;
    void loadImage(const char* path) override;
    PMemMap& getMemRef() override { return pmem; }
    uint32_t debugRead(uint32_t addr) const override;

private:
    PMemMap  pmem;

    // AXI 读通道状态机
    enum { AR_IDLE = 0, AR_RESP } rstate = AR_IDLE;
    uint32_t araddr = 0, arlen = 0, arsize = 0;
    uint32_t rcnt   = 0;

    // AXI 写通道状态机
    enum { AW_IDLE = 0, AW_DATA, AW_RESP } wstate = AW_IDLE;
    uint32_t awaddr = 0, awlen = 0, awsize = 0;
    uint32_t wcnt   = 0;

    uint32_t pmemRead(uint32_t addr, int bytes) const;
    void     pmemWrite(uint32_t addr, uint32_t data, uint8_t wmask, uint64_t cycle);

#ifdef CONFIG_MEM_AXI_BACKPRESSURE
    // 随机背压：随机拉低 ready 信号
    bool     randReady();
    uint32_t bp_seed = 0x12345678u;
#endif
};
#endif
