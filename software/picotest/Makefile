NAMES     = pico
SRCS      = $(basename $(shell find src/ -name "*.S"))
BASE_PORT = $(abspath ../base-port)
SIM_ROOT  = $(abspath ../..)
include $(BASE_PORT)/Makefile
include $(abspath ../scripts/sim.mk)
