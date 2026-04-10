#pragma once
#include <cstdint>
#include <cstdio>
#include <cassert>
#include "generated/config.h"

// ── ANSI 颜色 ─────────────────────────────────────────────────────────────────
#ifdef CONFIG_DEBUG_COLORS
#define ANSI_FG_RED     "\033[31m"
#define ANSI_FG_GREEN   "\033[32m"
#define ANSI_FG_YELLOW  "\033[33m"
#define ANSI_FG_BLUE    "\033[34m"
#define ANSI_FG_CYAN    "\033[36m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_NONE       "\033[0m"
#else
#define ANSI_FG_RED     ""
#define ANSI_FG_GREEN   ""
#define ANSI_FG_YELLOW  ""
#define ANSI_FG_BLUE    ""
#define ANSI_FG_CYAN    ""
#define ANSI_BOLD       ""
#define ANSI_NONE       ""
#endif

// ── 日志宏 ────────────────────────────────────────────────────────────────────
#define Log(fmt, ...) \
    fprintf(stdout, "[sim] " fmt "\n", ##__VA_ARGS__)

#define LogErr(fmt, ...) \
    fprintf(stderr, ANSI_FG_RED "[sim ERROR] " fmt ANSI_NONE "\n", ##__VA_ARGS__)

#define LogWarn(fmt, ...) \
    fprintf(stdout, ANSI_FG_YELLOW "[sim WARN]  " fmt ANSI_NONE "\n", ##__VA_ARGS__)

#define LogOk(fmt, ...) \
    fprintf(stdout, ANSI_FG_GREEN "[sim OK]    " fmt ANSI_NONE "\n", ##__VA_ARGS__)

// ── 地址工具 ─────────────────────────────────────────────────────────────────
#ifndef CONFIG_MEM_BASE
#define CONFIG_MEM_BASE 0x1c000000u
#endif
#ifndef CONFIG_MEM_SIZE_MB
#define CONFIG_MEM_SIZE_MB 128
#endif
#define CONFIG_MEM_SIZE ((size_t)(CONFIG_MEM_SIZE_MB) * 1024 * 1024)
#define CONFIG_MEM_END  (CONFIG_MEM_BASE + CONFIG_MEM_SIZE)

// 判断地址是否应走稀疏物理内存（pmem）而非 MMIO。
// 使用稀疏哈希表，支持任意 32 位地址；仅将已知 MMIO 窗口排除在外。
static inline bool in_pmem(uint32_t addr) {
    // UART MMIO：排除，由 mmioRead/mmioWrite 处理
    uint32_t aligned = addr & ~0x7u;
    if (aligned == (0x1FE001E0u & ~0x7u)) return false;  // LA32R UART
    if (aligned == (0xa00003f8u & ~0x7u)) return false;  // base-port UART
    return true;  // 其余全部映射到稀疏 pmem（包括 TLB 测试使用的高地址）
}

// ── UART MMIO 地址 ────────────────────────────────────────────────────────────
#define UART_ADDR 0x1FE001E0u  // LA32R 典型 UART 地址（可配置）

// ── NCOMMIT 默认值 ────────────────────────────────────────────────────────────
#ifndef CONFIG_NCOMMIT
#define CONFIG_NCOMMIT 2
#endif
