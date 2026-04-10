// 参考模拟器独立可执行入口
// 编译条件：-DREF_STANDALONE
#ifdef REF_STANDALONE

#include "Simulator.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void print_usage(const char* prog) {
    fprintf(stderr,
        "用法: %s <镜像文件.bin> [选项]\n"
        "选项:\n"
        "  --base <hex>    内存加载基地址（默认 0x1c000000）\n"
        "  --max <n>       最大执行步数（默认无限制）\n"
        "  --quiet         不打印寄存器转储\n"
        "  --help          显示此帮助\n",
        prog);
}

static void dump_regs(const Simulator& sim) {
    fprintf(stdout, "\n寄存器转储:\n");
    fprintf(stdout, "  PC = 0x%08x\n", sim.getPC());
    static const char* names[] = {
        "r0/zero","r1/ra ","r2/tp ","r3/sp ","r4/a0 ","r5/a1 ",
        "r6/a2 ","r7/a3 ","r8/a4 ","r9/a5 ","r10/a6","r11/a7",
        "r12/t0","r13/t1","r14/t2","r15/t3","r16/t4","r17/t5",
        "r18/t6","r19/t7","r20/t8","r21/rsv","r22/fp","r23/s0",
        "r24/s1","r25/s2","r26/s3","r27/s4","r28/s5","r29/s6",
        "r30/s7","r31/s8"
    };
    for (int i = 0; i < 32; i++) {
        fprintf(stdout, "  %s = 0x%08x", names[i], sim.getRf(i));
        if (i % 4 == 3) fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    const char* img_path = nullptr;
    uint32_t    base     = 0x1c000000u;
    uint64_t    max_steps = 0;
    bool        quiet    = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); return 0; }
        else if (strcmp(argv[i], "--base") == 0 && i+1 < argc) {
            base = (uint32_t)strtoul(argv[++i], nullptr, 0);
        }
        else if (strcmp(argv[i], "--max") == 0 && i+1 < argc) {
            max_steps = strtoull(argv[++i], nullptr, 0);
        }
        else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        }
        else if (argv[i][0] != '-') {
            img_path = argv[i];
        }
    }

    if (!img_path) { fprintf(stderr, "错误: 缺少镜像文件路径\n"); return 1; }

    Simulator sim(base);
    sim.loadImage(img_path);

    fprintf(stdout, "[ref-sim] 加载镜像: %s\n", img_path);
    fprintf(stdout, "[ref-sim] 基地址:   0x%08x\n", base);
    fprintf(stdout, "[ref-sim] 开始执行...\n");

    uint64_t steps = 0;
    while (sim.getState() == SIM_RUNNING) {
        sim.step(1);
        steps++;
        if (max_steps > 0 && steps >= max_steps) {
            fprintf(stdout, "[ref-sim] 达到最大执行步数 %llu，停止\n",
                    (unsigned long long)max_steps);
            break;
        }
    }

    fprintf(stdout, "[ref-sim] 执行完成，共 %llu 步\n",
            (unsigned long long)steps);

    int exit_code = sim.getExitCode();
    if (sim.getState() == SIM_END) {
        if (exit_code == 0) {
            fprintf(stdout, "\033[32m[ref-sim] HIT GOOD TRAP  (a0 = 0)\033[0m\n");
        } else {
            fprintf(stdout, "\033[31m[ref-sim] HIT BAD TRAP   (a0 = %d)\033[0m\n", exit_code);
        }
    } else {
        fprintf(stdout, "\033[33m[ref-sim] 停止（非正常退出）\033[0m\n");
        exit_code = 1;
    }

    if (!quiet) dump_regs(sim);

    return exit_code;
}

#endif // REF_STANDALONE
