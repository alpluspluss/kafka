SRC_DIR = src
INC_DIR = include
OBJ_DIR = $(BUILD_DIR)/obj/drivers

SRCS = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

INCLUDES = -I$(INC_DIR) -I$(PUB_INCLUDE_DIR)
CXXFLAGS += $(INCLUDES)

$(shell mkdir -p $(OBJ_DIR))

all: $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ_DIR)/$(ARCH)-*

.PHONY: all clean