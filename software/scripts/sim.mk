# sim.mk — 单二进制测试套件的仿真目标
#
# 使用方法：在测试目录的 Makefile 中，定义 NAMES/SRCS/BASE_PORT 并
# include $(BASE_PORT)/Makefile 之后，再 include 本文件：
#
#   NAMES     = mytest
#   SRCS      = main.c
#   BASE_PORT = $(abspath ../base-port)
#   SIM_ROOT  = $(abspath ../..)
#   include $(BASE_PORT)/Makefile
#   include $(SIM_ROOT)/scripts/sim.mk
#
# 提供的目标：
#   make          → 编译（image，来自 base-port/Makefile）
#   make test     → 用参考模拟器验证（无需 CPU RTL）
#   make run      → 用 CPU 全仿真 + difftest（需要 make all 先构建仿真器）
#   make wave     → 同上，附带生成 VCD 波形

SIM_ROOT ?= $(abspath ../..)

REF_BIN := $(SIM_ROOT)/build/ref-sim
SIM_BIN := $(SIM_ROOT)/build/la32r-sim
REF_SO  := $(SIM_ROOT)/build/simulator.so

.PHONY: test run wave

# 用参考模拟器运行，无需 CPU RTL
test: image
	@if [ ! -f "$(REF_BIN)" ]; then \
	    echo "错误: 未找到 $(REF_BIN)"; \
	    echo "请先在项目根目录执行: make ref"; \
	    exit 1; fi
	@$(REF_BIN) $(IMAGE).bin

# 用 CPU 全仿真 + difftest 运行
run: image
	@if [ ! -f "$(SIM_BIN)" ]; then \
	    echo "错误: 未找到 $(SIM_BIN)"; \
	    echo "请先将 CPU Verilog 放入 rtl/，然后执行: make all"; \
	    exit 1; fi
	@$(SIM_BIN) $(IMAGE).bin $(REF_SO)

# 用 CPU 全仿真 + difftest + VCD 波形
wave: image
	@if [ ! -f "$(SIM_BIN)" ]; then \
	    echo "错误: 未找到 $(SIM_BIN)"; \
	    echo "请先将 CPU Verilog 放入 rtl/，然后执行: make all"; \
	    exit 1; fi
	@$(SIM_BIN) $(IMAGE).bin $(REF_SO) --wave
