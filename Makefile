# parp Makefile. Based on https://github.com/mbcrawfo/GenericMakefile
BIN_NAME := parp
CXX ?= g++
SRC_EXT = cc
SRC_DIR = src
COMPILE_FLAGS = -std=c++11 -Wall -Wextra
RCOMPILE_FLAGS = -D NDEBUG -02
DCOMPILE_FLAGS = -D DEBUG -g
INCLUDES = -I $(SRC_DIR)/

# Combine compiler and linker flags
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
release: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(RLINK_FLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
debug: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(DLINK_FLAGS)

# Build and output paths
release: export BUILD_PATH := build/release
release: export BIN_PATH := bin/release
debug: export BUILD_PATH := build/debug
debug: export BIN_PATH := bin/debug
install: export BIN_PATH := bin/release

SOURCES := main.cc

SOURCES := $(addprefix $(SRC_DIR)/, $(SOURCES))
OBJECTS = $(SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
DEPS = $(OBJECTS:.o=.d)

.PHONY: release
release: dirs
	@$(MAKE) all --no-print-directory

.PHONY: debug
debug: dirs
	@$(MAKE) all --no-print-directory

# Create the directories used in the build
.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(BIN_PATH)

.PHONY: clean
clean:
	@$(RM) $(BIN_NAME)
	@$(RM) -r build
	@$(RM) -r bin

# Main rule, checks the executable and symlinks to the output
all: $(BIN_PATH)/$(BIN_NAME)

# Link the executable
$(BIN_PATH)/$(BIN_NAME): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

# Add dependency files, if they exist
-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_DIR)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@
