CROSS_COMPILE := loongarch32r-linux-gnusf-
COMMON_FLAGS  := -fno-pic 
CFLAGS        += $(COMMON_FLAGS) -static -fdata-sections -ffunction-sections
AFLAGS        += $(COMMON_FLAGS) 
LDFLAGS       += -static --gc-sections -e _start