#include "VCPU.h"
#include "Emulator.h"
#include "Memory.h"
#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef CONFIG_DUMP_WAVE
#include "verilated_vcd_c.h"
#endif

static void print_usage(const char* prog) {
    fprintf(stdout,
        "用法: %s <镜像文件.bin> [参考库.so] [选项]\n"
        "\n"
        "参数:\n"
        "  <镜像文件.bin>      测试程序二进制镜像（必须）\n"
        "  [参考库.so]         参考模拟器共享库，用于 difftest（可选）\n"
        "\n"
        "选项:\n"
        "  --base <hex>        内存加载基地址（默认 0x%08x）\n"
        "  --max <n>           最大仿真周期数（0=无限制）\n"
        "  --wave              生成 VCD 波形文件\n"
        "  --quiet             减少输出\n"
        "  --help              显示此帮助\n",
        prog, (uint32_t)CONFIG_MEM_BASE);
}

int main(int argc, char* argv[]) {
    Verilated::commandArgs(argc, argv);

    const char* img_path    = nullptr;
    const char* so_path     = nullptr;
    uint32_t    base_addr   = CONFIG_MEM_BASE;
    uint64_t    max_steps   = CONFIG_MAX_STEPS;
    bool        quiet       = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); return 0; }
        else if (strcmp(argv[i], "--base") == 0 && i+1 < argc) {
            base_addr = (uint32_t)strtoul(argv[++i], nullptr, 0);
        }
        else if (strcmp(argv[i], "--max") == 0 && i+1 < argc) {
            max_steps = strtoull(argv[++i], nullptr, 0);
        }
        else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        }
        else if (strcmp(argv[i], "--wave") == 0) {
            // 波形录制（需编译时开启 CONFIG_DUMP_WAVE）
#ifndef CONFIG_DUMP_WAVE
            fprintf(stderr, "[sim] 警告: 未编译波形支持（需配置 CONFIG_DUMP_WAVE）\n");
#endif
        }
        else if (argv[i][0] != '-') {
            if (!img_path)   img_path = argv[i];
            else if (!so_path) so_path = argv[i];
        }
    }

    if (!img_path) {
        fprintf(stderr, "错误: 必须指定测试镜像文件\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (!quiet) {
        fprintf(stdout, "LA32RSim-2026\n");
        fprintf(stdout, "镜像文件:   %s\n", img_path);
        if (so_path) fprintf(stdout, "参考库:     %s\n", so_path);
        fprintf(stdout, "基地址:     0x%08x\n", base_addr);
        fprintf(stdout, "NCOMMIT:    %d\n", CONFIG_NCOMMIT);
#if defined(CONFIG_MEM_AXI)
        fprintf(stdout, "内存接口:   AXI\n");
#elif defined(CONFIG_MEM_HARVARD)
        fprintf(stdout, "内存接口:   哈佛分离双端口（64-bit 变宽）\n");
#else
        fprintf(stdout, "内存接口:   简单总线（32-bit）\n");
#endif
        fprintf(stdout, "\n");
    }

    // 构建 DUT
    VCPU* cpu = new VCPU;

    // 构建内存
#if defined(CONFIG_MEM_AXI)
    MemoryBase* mem = new AXIMemory();
#elif defined(CONFIG_MEM_HARVARD)
    MemoryBase* mem = new HarvardMemory();
#else
    MemoryBase* mem = new SimpleMemory();
#endif
    mem->loadImage(img_path);

    // 构建仿真器
    Emulator emu(cpu, mem);

#ifdef CONFIG_DIFFTEST
    if (so_path) {
        emu.initDifftest(so_path, img_path, base_addr);
    } else {
        fprintf(stdout, "[sim] 未指定参考库，跳过 difftest\n");
    }
#endif

    // 运行
    SimResult result = emu.run(max_steps);

    // 汇报
    int exit_code = 0;
    switch (result) {
    case SIM_GOOD_TRAP:
        if (!quiet) LogOk("仿真通过 (GOOD TRAP)");
        exit_code = 0; break;
    case SIM_BAD_TRAP:
        fprintf(stdout, ANSI_FG_RED "[sim] 仿真失败 (BAD TRAP)\n" ANSI_NONE);
        exit_code = 1; break;
    case SIM_DIFFTEST_FAIL:
        fprintf(stdout, ANSI_FG_RED "[sim] Difftest 不一致\n" ANSI_NONE);
        exit_code = 2; break;
    case SIM_STALL:
        fprintf(stdout, ANSI_FG_RED "[sim] 处理器死锁（无提交）\n" ANSI_NONE);
        exit_code = 3; break;
    case SIM_MAX_STEPS:
        fprintf(stdout, ANSI_FG_YELLOW "[sim] 达到最大步数\n" ANSI_NONE);
        exit_code = 4; break;
    }

    delete mem;
    delete cpu;
    return exit_code;
}
