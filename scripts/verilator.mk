# Verilator 编译规则
# 变量：
#   RTL_DIR        — 用户 RTL 目录（含 CPU.sv 的文件夹）
#   RTL_TOP        — 顶层模块名，默认 CPU
#   BUILD_DIR      — 构建输出目录
#   SIM_SRCS       — C++ 仿真源文件列表
#   SIM_INCS       — C++ include 目录列表
#   VERILATOR_OPTS — 额外 Verilator 选项

RTL_TOP    ?= CPU
BUILD_DIR  ?= $(WORK_DIR)/build/verilator
VTOP       := V$(RTL_TOP)

VERILATOR  := verilator
VFLAGS     := --cc --exe -O3 --x-assign fast --x-initial fast \
              --noassert -Wno-WIDTHTRUNC -Wno-WIDTHEXPAND \
              -Wno-UNUSED -Wno-UNDRIVEN

ifeq ($(CONFIG_DUMP_WAVE),1)
VFLAGS     += --trace
endif

VERILATOR_OPTS ?=

# 收集 RTL 源文件
RTL_SRCS := $(shell find $(RTL_DIR) -name "*.sv" -o -name "*.v" 2>/dev/null)

ifeq ($(RTL_SRCS),)
$(error RTL 目录 $(RTL_DIR) 中没有找到 .sv/.v 文件，请将 CPU Verilog 代码放到 rtl/ 目录)
endif

SIM_INC_FLAGS := $(addprefix -I, $(SIM_INCS))

$(BUILD_DIR)/$(VTOP).mk: $(RTL_SRCS) $(SIM_SRCS_DEPS)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VFLAGS) $(VERILATOR_OPTS) \
	    -Mdir $(BUILD_DIR) \
	    $(SIM_INC_FLAGS) \
	    $(addprefix -I, $(RTL_DIR)) \
	    $(RTL_SRCS) \
	    $(SIM_SRCS) \
	    --top-module $(RTL_TOP)
	@echo "  VERILATE  $(RTL_TOP)"

VERILATED_BIN := $(WORK_DIR)/build/la32r-sim

$(VERILATED_BIN): $(BUILD_DIR)/$(VTOP).mk
	$(MAKE) -C $(BUILD_DIR) -f $(VTOP).mk CXX=g++ \
	    VM_PARALLEL_BUILDS=1 \
	    OPT_FAST="-O3" \
	    -j$(shell nproc)
	@cp $(BUILD_DIR)/$(VTOP) $(VERILATED_BIN)
	@echo "  BIN     $(VERILATED_BIN)"

.PHONY: verilate
verilate: $(VERILATED_BIN)
