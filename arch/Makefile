SRC_DIR = $(ARCH)/src
INC_DIR = $(ARCH)/include
OBJ_DIR = $(BUILD_DIR)/obj/arch-$(ARCH)

SRCS = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/$(ARCH)-%.o,$(SRCS))

INCLUDES = -I$(INC_DIR) -I$(PUB_INCLUDE_DIR)
CXXFLAGS += $(INCLUDES)

$(shell mkdir -p $(OBJ_DIR))

all: $(OBJS)

$(OBJ_DIR)/$(ARCH)-%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)/$(ARCH)-*

.PHONY: all clean