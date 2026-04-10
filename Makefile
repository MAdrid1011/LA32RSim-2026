WORK_DIR := $(shell pwd)

# ── 工具链 PATH ───────────────────────────────────────────────────────────────
export PATH := /opt/loongson-gnu-toolchain-8.3-x86_64-loongarch32r-linux-gnusf-v2.0/bin:$(PATH)

# ── 配置加载 ──────────────────────────────────────────────────────────────────
DOTCONFIG := $(WORK_DIR)/.config

ifneq ($(wildcard $(DOTCONFIG)),)
include $(DOTCONFIG)
export $(shell sed '/^#/d; /^$$/d; s/=.*//' $(DOTCONFIG))
else
CONFIG_NCOMMIT        ?= 2
CONFIG_MEM_AXI        ?= y
CONFIG_REF_STANDALONE ?= y
CONFIG_DIFFTEST       ?= y
endif

include $(WORK_DIR)/scripts/gen-config.mk

# ── 路径配置 ──────────────────────────────────────────────────────────────────
RTL_DIR   ?= $(WORK_DIR)/rtl
BUILD_DIR := $(WORK_DIR)/build
REF_DIR   := $(WORK_DIR)/ref
SIM_DIR   := $(WORK_DIR)/sim

SIM_INCS  := $(SIM_DIR)/include \
             $(REF_DIR)/include \
             $(WORK_DIR)/include/generated \
             $(WORK_DIR)/include

REF_SO    := $(BUILD_DIR)/simulator.so
REF_BIN   := $(BUILD_DIR)/ref-sim
SIM_BIN   := $(BUILD_DIR)/la32r-sim

# ── Kconfig 工具 ──────────────────────────────────────────────────────────────
MENUCONFIG := python3.12 -m menuconfig

.PHONY: menuconfig
menuconfig:
	@KCONFIG_CONFIG=$(DOTCONFIG) $(MENUCONFIG) $(WORK_DIR)/Kconfig

.PHONY: defconfig
defconfig:
	@echo "使用默认配置"
	@KCONFIG_CONFIG=$(DOTCONFIG) python3.12 -c "\
import kconfiglib, sys; \
kconf = kconfiglib.Kconfig('$(WORK_DIR)/Kconfig'); \
kconf.load_config($(WORK_DIR)/Kconfig); \
kconf.write_config('$(DOTCONFIG)')"
	@echo "已生成 .config"

# ── 参考模拟器构建 ────────────────────────────────────────────────────────────
REF_SRCS := $(wildcard $(REF_DIR)/src/*.cc)
REF_INCS := $(REF_DIR)/include \
            $(WORK_DIR)/include/generated \
            $(WORK_DIR)/include
REF_CXXFLAGS := -O2 -std=c++17 $(addprefix -I,$(REF_INCS)) \
                -DCONFIG_REF -fPIC

$(REF_SO): $(WORK_DIR)/include/generated/config.h $(REF_SRCS)
	@mkdir -p $(BUILD_DIR)
	@echo "  CXX     参考模拟器 (共享库)"
	$(CXX) $(REF_CXXFLAGS) -shared -o $@ \
	    $(filter-out $(REF_DIR)/src/main.cc, $(REF_SRCS))
	@echo "  SO      $@"

$(REF_BIN): $(WORK_DIR)/include/generated/config.h $(REF_SRCS)
	@mkdir -p $(BUILD_DIR)
	@echo "  CXX     参考模拟器 (独立可执行)"
	$(CXX) $(REF_CXXFLAGS) -DREF_STANDALONE -o $@ $(REF_SRCS)
	@echo "  BIN     $@"

.PHONY: ref
ref: $(REF_SO)
ifeq ($(CONFIG_REF_STANDALONE),y)
ref: $(REF_BIN)
endif

# ── 仿真主程序构建（Verilator）────────────────────────────────────────────────
SIM_SRCS_ALL := $(wildcard $(SIM_DIR)/src/*.cc)
SIM_CXXFLAGS := -O2 -std=c++17 $(addprefix -I,$(SIM_INCS))
ifeq ($(CONFIG_DUMP_WAVE),y)
SIM_CXXFLAGS += -DCONFIG_DUMP_WAVE
endif

VTOP_DIR  := $(BUILD_DIR)/verilator
VTOP_NAME := CPU
VTOP_MK   := $(VTOP_DIR)/V$(VTOP_NAME).mk
VTOP_BIN  := $(VTOP_DIR)/V$(VTOP_NAME)

RTL_SRCS  := $(shell find $(RTL_DIR) -name "*.sv" -o -name "*.v" 2>/dev/null | sort)

VFLAGS := --cc --exe -O3 --x-assign fast --x-initial fast \
          --noassert -Wno-WIDTHTRUNC -Wno-WIDTHEXPAND \
          -Wno-UNUSED -Wno-UNDRIVEN -Wno-TIMESCALEMOD \
          $(addprefix -I,$(SIM_INCS)) \
          -I$(RTL_DIR)
ifeq ($(CONFIG_DUMP_WAVE),y)
VFLAGS += --trace
endif

$(VTOP_MK): $(WORK_DIR)/include/generated/config.h $(RTL_SRCS) $(SIM_SRCS_ALL)
	@if [ -z "$(RTL_SRCS)" ]; then \
	    echo ""; \
	    echo "错误：rtl/ 目录中没有找到 .sv/.v 文件"; \
	    echo "请将被测 CPU 的 Verilog 代码放到 $(RTL_DIR)/"; \
	    echo "接口规范见 docs/接口规范.md"; \
	    echo ""; exit 1; fi
	@mkdir -p $(VTOP_DIR)
	verilator $(VFLAGS) \
	    --Mdir $(VTOP_DIR) \
	    $(RTL_SRCS) \
	    $(SIM_SRCS_ALL) \
	    --top-module $(VTOP_NAME)
	@echo "  VERILATE  $(VTOP_NAME)"

$(SIM_BIN): $(VTOP_MK) $(REF_SO)
	$(MAKE) -C $(VTOP_DIR) -f V$(VTOP_NAME).mk \
	    VM_PARALLEL_BUILDS=1 \
	    OPT_FAST="-O3" \
	    CXXFLAGS="$(SIM_CXXFLAGS)" \
	    -j$(shell nproc)
	@cp $(VTOP_BIN) $(SIM_BIN)
	@echo "  BIN     $(SIM_BIN)"

.PHONY: sim
sim: $(SIM_BIN)

# ── 主构建目标 ────────────────────────────────────────────────────────────────
.PHONY: all
all: $(WORK_DIR)/include/generated/config.h ref sim

# ── 运行目标 ──────────────────────────────────────────────────────────────────
IMG  ?=
ARGS ?=

REF_SO_ARG := $(if $(CONFIG_DIFFTEST),$(REF_SO),)

.PHONY: run
run: all
	@if [ -z "$(IMG)" ]; then echo "用法: make run IMG=path/to/test.bin"; exit 1; fi
	$(SIM_BIN) $(IMG) $(REF_SO_ARG) $(ARGS)

.PHONY: run-ref
run-ref: $(REF_BIN)
	@if [ -z "$(IMG)" ]; then echo "用法: make run-ref IMG=path/to/test.bin"; exit 1; fi
	$(REF_BIN) $(IMG) $(ARGS)

.PHONY: wave
wave: ARGS += --wave
wave: run

# ── 软件构建 ──────────────────────────────────────────────────────────────────
# 编译 software/ 下的所有测试套件
.PHONY: software-all
software-all:
	$(MAKE) -C $(WORK_DIR)/software all

# ── 参考模拟器测试（无需 CPU RTL）──────────────────────────────────────────────
# 在各测试目录中直接用 make test 也可以单独运行某套测试
.PHONY: test-all
test-all: $(REF_BIN) software-all
	@$(MAKE) -C $(WORK_DIR)/software test-all

# ── CPU 全仿真测试（含 difftest，需先 make all）─────────────────────────────────
# 在各测试目录中直接用 make run 也可以单独运行某套测试
.PHONY: sim-all
sim-all: $(SIM_BIN) software-all
	@$(MAKE) -C $(WORK_DIR)/software sim-all

# ── 清理 ─────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(WORK_DIR)/include/generated/config.h

.PHONY: distclean
distclean: clean
	rm -f $(DOTCONFIG)

# ── 帮助 ─────────────────────────────────────────────────────────────────────
.PHONY: help
help:
	@echo "LA32RSim-2026 构建系统"
	@echo ""
	@echo "配置："
	@echo "  make menuconfig     — 图形化配置界面（首次使用必须运行）"
	@echo ""
	@echo "构建仿真器："
	@echo "  make ref            — 构建参考模拟器 build/ref-sim（无需 CPU RTL）"
	@echo "  make all            — 构建 CPU 全仿真器 build/la32r-sim（需要 rtl/ 下有 CPU Verilog）"
	@echo ""
	@echo "编译测试软件："
	@echo "  make software-all   — 编译所有测试套件（委托给 software/Makefile）"
	@echo "  cd software/coremark && make   — 单独编译某套测试"
	@echo ""
	@echo "参考模拟器验证（无需 CPU RTL，快速回归）："
	@echo "  make test-all       — 运行所有测试套件（ref-sim）"
	@echo "  cd software/functest && make test      — 单套测试"
	@echo "  cd software/coremark && make test"
	@echo "  cd software/dhrystone && make test"
	@echo "  cd software/picotest && make test"
	@echo ""
	@echo "CPU 全仿真 + difftest（需先 make all）："
	@echo "  make sim-all        — 运行所有测试套件（CPU + difftest）"
	@echo "  cd software/functest && make run       — 单套测试"
	@echo "  cd software/coremark && make run"
	@echo "  make run IMG=<bin>  — 运行单个二进制（含 difftest）"
	@echo "  make wave IMG=<bin> — 运行并生成 VCD 波形"
	@echo ""
	@echo "清理："
	@echo "  make clean          — 清理仿真器构建产物"
	@echo "  make distclean      — 清理仿真器产物和配置"
	@echo ""
	@echo "变量："
	@echo "  RTL_DIR=<path>      — 指定 CPU RTL 目录（默认 rtl/）"
	@echo "  IMG=<bin>           — 测试程序镜像路径"
	@echo "  ARGS=<args>         — 额外命令行参数"
