# functest — 多二进制测试套件
# 每个 src/*.c 独立编译为一个二进制，test/run/wave 目标逐一执行。
#
# make          → 编译所有测试二进制
# make test     → 用参考模拟器逐一验证（无需 CPU RTL）
# make run      → 用 CPU 全仿真 + difftest 逐一验证
# make wave     → 同上，附带生成 VCD 波形
# make clean    → 删除编译产物

BASE_PORT := $(abspath ../base-port)
SIM_ROOT  := $(abspath ../..)

REF_BIN := $(SIM_ROOT)/build/ref-sim
SIM_BIN := $(SIM_ROOT)/build/la32r-sim
REF_SO  := $(SIM_ROOT)/build/simulator.so

NAMES := $(sort $(basename $(notdir $(wildcard src/*.c))))

.PHONY: image test run wave clean

# 构建所有测试二进制
image:
	@for name in $(NAMES); do \
	    BASE_PORT=$(BASE_PORT) NAMES=$$name SRCS=src/$$name.c \
	    $(MAKE) -s -f $(BASE_PORT)/Makefile image; \
	done

# 用参考模拟器逐一运行，打印 PASS/FAIL 汇总
test: image
	@if [ ! -f "$(REF_BIN)" ]; then \
	    echo "错误: 未找到 $(REF_BIN)"; \
	    echo "请先在项目根目录执行: make ref"; exit 1; fi
	@PASS=0; FAIL=0; \
	for bin in build/*-la32r.bin; do \
	    [ -f "$$bin" ] || continue; \
	    name=$$(basename $$bin -la32r.bin); \
	    if $(REF_BIN) --quiet $$bin 2>&1 | grep -q "GOOD TRAP"; then \
	        printf "  %28s: \033[32m[PASS]\033[0m\n" $$name; PASS=$$((PASS+1)); \
	    else \
	        printf "  %28s: \033[31m[FAIL]\033[0m\n" $$name; FAIL=$$((FAIL+1)); \
	    fi; \
	done; \
	echo ""; echo "functest: $$PASS 通过  $$FAIL 失败"

# 用 CPU 全仿真 + difftest 逐一运行
run: image
	@if [ ! -f "$(SIM_BIN)" ]; then \
	    echo "错误: 未找到 $(SIM_BIN)"; \
	    echo "请先将 CPU Verilog 放入 rtl/，然后执行: make all"; exit 1; fi
	@PASS=0; FAIL=0; \
	for bin in build/*-la32r.bin; do \
	    [ -f "$$bin" ] || continue; \
	    name=$$(basename $$bin -la32r.bin); \
	    if $(SIM_BIN) $$bin $(REF_SO) --quiet 2>&1 | grep -q "GOOD TRAP"; then \
	        printf "  %28s: \033[32m[PASS]\033[0m\n" $$name; PASS=$$((PASS+1)); \
	    else \
	        printf "  %28s: \033[31m[FAIL]\033[0m\n" $$name; FAIL=$$((FAIL+1)); \
	    fi; \
	done; \
	echo ""; echo "functest: $$PASS 通过  $$FAIL 失败"

wave: image
	@if [ ! -f "$(SIM_BIN)" ]; then \
	    echo "错误: 未找到 $(SIM_BIN)"; \
	    echo "请先将 CPU Verilog 放入 rtl/，然后执行: make all"; exit 1; fi
	@for bin in build/*-la32r.bin; do \
	    [ -f "$$bin" ] || continue; \
	    echo "[wave] $$bin"; \
	    $(SIM_BIN) $$bin $(REF_SO) --wave; \
	done

clean:
	rm -rf build
