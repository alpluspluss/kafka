SRC_DIR = src
INC_DIR = include
OBJ_DIR = $(BUILD_DIR)/obj/kernel
DRIVERS_OBJ_DIR = $(BUILD_DIR)/obj/drivers
ARCH_LIB_OBJ_DIR = $(BUILD_DIR)/obj/arch-$(ARCH)
MM_OBJ_DIR = $(BUILD_DIR)/obj/mm

KERNEL_NAME = kafka-kernel-$(ARCH)
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME)

CPP_SRCS = $(shell find $(SRC_DIR) -type f -name "*.cpp")
C_SRCS = $(shell find $(SRC_DIR) -type f -name "*.c")

CPP_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/$(ARCH)-%.o,$(CPP_SRCS))
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/$(ARCH)-%.o,$(C_SRCS))

OBJS = $(CPP_OBJS) $(C_OBJS)

MM_OBJS = $(shell find $(MM_OBJ_DIR) -name "$(ARCH)-*.o")
ARCH_OBJS = $(shell find $(ARCH_LIB_OBJ_DIR) -name "$(ARCH)-*.o")
DRIVER_OBJS = $(shell find $(DRIVERS_OBJ_DIR) -name "$(ARCH)-*.o")

INCLUDES = -I$(INC_DIR) -I$(PUB_INCLUDE_DIR)
LDFLAGS = -T $(CURDIR)/../arch/$(ARCH)/linker.ld -nostdlib

$(shell mkdir -p $(OBJ_DIR))

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJS) driver_objs
	@echo "Linking kernel..."
	@$(LD) $(LDFLAGS) -o $@ $(OBJS) $(MM_OBJS) $(ARCH_OBJS) $(DRIVER_OBJS)

mm_objs:
	@echo "Checking for memory objects..."
	@if [ ! -d "$(MM_OBJ_DIR)" ]; then \
		echo "Memory objects directory not found!"; \
		exit 1; \
	if

arch_objs:
	@echo "Checking for arch objects..."
	@if [ ! -d "$(ARCH_LIB_OBJ_DIR)" ]; then \
		echo "Arch objects directory not found!"; \
		exit 1; \
	fi

driver_objs:
	@echo "Checking for driver objects..."
	@if [ ! -d "$(DRIVERS_OBJ_DIR)" ]; then \
		echo "Driver objects directory not found!"; \
		exit 1; \
	fi

$(OBJ_DIR)/$(ARCH)-%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/$(ARCH)-%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)/$(ARCH)-* $(KERNEL_BIN)

.PHONY: all clean arch_objs driver_objs