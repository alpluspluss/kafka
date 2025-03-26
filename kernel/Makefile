SRC_DIR = src
INC_DIR = include
OBJ_DIR = $(BUILD_DIR)/obj/kernel

KERNEL_NAME = kafka-kernel-$(ARCH)
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME)

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

INCLUDES = -I$(INC_DIR)
LDFLAGS = -T $(CURDIR)/../arch/$(ARCH)/linker.ld -nostdlib

$(shell mkdir -p $(OBJ_DIR))

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(OBJS)
	@echo "Linking kernel..."
	@$(LD) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR) $(KERNEL_BIN)

.PHONY: all clean