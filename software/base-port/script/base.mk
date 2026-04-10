
# make ar
CROSS_COMPILE := loongarch32r-linux-gnusf-
AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)gcc


CFLAGS = -MMD -Og $(INC_PATH)
CFLAGS += -fno-asynchronous-unwind-tables -fno-builtin -fno-stack-protector
AFLAGS =  $(INC_PATH)
ARFLAGS = rcs

BUILD_DIR = $(abspath ./build)
TAR_DIR = $(BUILD_DIR)/loongarch32r
OBJS = $(addprefix $(TAR_DIR)/, $(addsuffix .o, $(basename $(KER_SRCS))))
LIBKER = $(BUILD_DIR)/lib$(LIBNAME).a

# Compile C to object file
$(TAR_DIR)/%.o: %.c
	@mkdir -p $(dir $@) && echo "\033[33m[CC]\033[0m $<"
	@$(CC) $(CFLAGS) -c -o $@ $(realpath $<)

# Compile assembly to object file
$(TAR_DIR)/%.o: %.S
	@mkdir -p $(dir $@) && echo "\033[33m[AS]\033[0m $<"
	@$(AS) $(AFLAGS) -c -o $@ $(realpath $<)

libkernel: $(LIBKER)
$(LIBKER): $(OBJS)
	@echo "\033[33m[AR]\033[0m build/$(notdir $@)"
	@$(AR) $(ARFLAGS) $@ $(realpath $^)

clean:
	rm -rf $(BUILD_DIR)
