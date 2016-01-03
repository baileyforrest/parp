# parp Makefile. Based on https://github.com/mbcrawfo/GenericMakefile
BIN_NAME := parp
CXX ?= g++
SRC_EXT = cc
SRC_DIR = src
COMPILE_FLAGS = -std=c++11 -Wall -Wextra
RCOMPILE_FLAGS = -D NDEBUG -O2
DCOMPILE_FLAGS = -D DEBUG -g
INCLUDES = -I $(SRC_DIR)/

GTEST_DIR := $(SRC_DIR)/gtest
TEST_CXXFLAGS := -isystem $(GTEST_DIR)/include -pthread
TEST_LDFLAGS := -lpthread
TEST_NAME := unittests

# Combine compiler and linker flags
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
test: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)

# Build and output paths
release: export BUILD_PATH := build/release
release: export BIN_PATH := bin/release
debug: export BUILD_PATH := build/debug
debug: export BIN_PATH := bin/debug
test: export BUILD_PATH := build/debug
test: export BIN_PATH := bin/debug

GTEST_OUT := $(BUILD_PATH)/gtest
GTEST_LIB := $(GTEST_OUT)/libgtest.a

EXE_SOURCES := \
	main.cc

EXE_SOURCES := $(addprefix $(SRC_DIR)/, $(EXE_SOURCES))
EXE_OBJECTS = $(EXE_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

COMMON_SOURCES := \
	parse/lexer.cc \
	util/char_class.cc \
	util/exceptions.cc \
	util/mark.cc \
	util/text_stream.cc

COMMON_SOURCES := $(addprefix $(SRC_DIR)/, $(COMMON_SOURCES))
COMMON_OBJECTS = $(COMMON_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

# All test sources must be suffixed with _unittest
TEST_SOURCES := \
	main_unittest.cc \
	parse/lexer_unittest.cc

TEST_SOURCES := $(addprefix $(SRC_DIR)/, $(TEST_SOURCES))
TEST_OBJECTS = $(TEST_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

DEPS = $(EXE_OBJECTS:.o=.d) $(COMMON_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)
ALL_OBJECTS = $(EXE_OBJECTS) $(COMMON_OBJECTS) $(TEST_OBJECTS)

.PHONY: release
release: dirs
	@$(MAKE) all --no-print-directory

.PHONY: debug
debug: dirs
	@$(MAKE) all --no-print-directory

.PHONY: test
test: dirs
	@$(MAKE) unittests --no-print-directory

# Create the directories used in the build
.PHONY: dirs
dirs:
	@mkdir -p $(dir $(ALL_OBJECTS))
	@mkdir -p $(BIN_PATH)

.PHONY: clean
clean:
	$(RM) -r build
	$(RM) -r bin

$(GTEST_LIB): $(GTEST_DIR)/src/gtest-all.cc
	@mkdir -p $(GTEST_OUT)
	$(CXX) $(TEST_CXXFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc -o $(GTEST_OUT)/gtest-all.o
	ar -rv $(GTEST_LIB) $(GTEST_OUT)/gtest-all.o

# Main rule, build executable
all: $(BIN_PATH)/$(BIN_NAME)

# Rule to build unittests
unittests: $(BIN_PATH)/$(TEST_NAME)

# Link the executable
$(BIN_PATH)/$(BIN_NAME): $(COMMON_OBJECTS) $(EXE_OBJECTS)
	$(CXX) $(COMMON_OBJECTS) $(EXE_OBJECTS) -o $@

# Link the tests
$(BIN_PATH)/$(TEST_NAME): $(COMMON_OBJECTS) $(TEST_OBJECTS) $(GTEST_LIB)
	$(CXX) $(COMMON_OBJECTS) $(TEST_OBJECTS) $(GTEST_LIB) $(TEST_LDFLAGS) -o $@

# Add dependency files, if they exist
-include $(DEPS)

# Unit tests have additional flags
$(BUILD_PATH)/%unittest.o: $(SRC_DIR)/%unittest.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(TEST_CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

$(BUILD_PATH)/%.o: $(SRC_DIR)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@
