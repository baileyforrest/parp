# parp Makefile. Based on https://github.com/mbcrawfo/GenericMakefile

CXX ?= g++
CLANG_FORMAT ?= clang-format

SAN_FLAGS := \
	-fsanitize=address \
	-fsanitize=leak \
	-fsanitize=undefined

BIN_NAME := parp
SRC_EXT = cc
SRC_DIR = src
COMPILE_FLAGS = -g -std=c++14 -Wall -Wextra -Werror -Wno-unused-parameter
RCOMPILE_FLAGS = -DNDEBUG -O3
DCOMPILE_FLAGS = -DDEBUG -g -fprofile-arcs -ftest-coverage $(SAN_FLAGS)
INCLUDES = -I$(SRC_DIR)/
LINK_FLAGS = -g -lreadline
RLINK_FLAGS =
DLINK_FLAGS = -lgcov -fsanitize=address $(SAN_FLAGS)

GTEST_DIR := third_party/googletest/googletest
TEST_CXXFLAGS := -isystem $(GTEST_DIR)/include -pthread
TEST_LDFLAGS := -lpthread
TEST_NAME := unittests

COVERAGE_PATH := coverage

RELEASE_BUILD_PATH := build/release
RELEASE_BIN_PATH := bin/release
DEBUG_BUILD_PATH := build/debug
DEBUG_BIN_PATH := bin/debug

# Combine compiler and linker flags
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
release: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(RLINK_FLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
debug: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(DLINK_FLAGS)
test: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
test: export LDFLAGS := $(LDFLAGS) $(LINK_FLAGS) $(DLINK_FLAGS)

# Build and output paths
release: export BUILD_PATH := $(RELEASE_BUILD_PATH)
release: export BIN_PATH := $(RELEASE_BIN_PATH)
debug: export BUILD_PATH := $(DEBUG_BUILD_PATH)
debug: export BIN_PATH := $(DEBUG_BIN_PATH)
test: export BUILD_PATH := $(DEBUG_BUILD_PATH)
test: export BIN_PATH := $(DEBUG_BIN_PATH)

GTEST_OUT := $(BUILD_PATH)/gtest
GTEST_LIB := $(GTEST_OUT)/libgtest.a

EXE_SOURCES := \
	main.cc \
	repl.cc

EXE_SOURCES := $(addprefix $(SRC_DIR)/, $(EXE_SOURCES))
EXE_OBJS = $(EXE_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

COMMON_SOURCES := \
	eval/eval.cc \
	expr/expr.cc \
	expr/number.cc \
	expr/primitive.cc \
	gc/gc.cc \
	parse/lexer.cc \
	parse/parse.cc \
	util/char_class.cc \
	util/exceptions.cc \
	util/flags.cc \
	util/mark.cc \
	util/text_stream.cc

COMMON_SOURCES := $(addprefix $(SRC_DIR)/, $(COMMON_SOURCES))
COMMON_OBJS = $(COMMON_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

# All test sources must be suffixed with _test
TEST_SOURCES := \
	eval/eval_test.cc \
	parse/lexer_test.cc \
	parse/parse_test.cc \
	test/main_test.cc

TEST_SOURCES := $(addprefix $(SRC_DIR)/, $(TEST_SOURCES))
TEST_OBJS = $(TEST_SOURCES:$(SRC_DIR)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)

DEPS = $(EXE_OBJS:.o=.d) $(COMMON_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
ALL_OBJS = $(EXE_OBJS) $(COMMON_OBJS) $(TEST_OBJS)

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
	@mkdir -p $(dir $(ALL_OBJS))
	@mkdir -p $(BIN_PATH)

.PHONY: clean
clean:
	$(RM) -r build
	$(RM) -r bin
	$(RM) -r $(COVERAGE_PATH)

$(GTEST_LIB): $(GTEST_DIR)/src/gtest-all.cc
	@mkdir -p $(GTEST_OUT)
	$(CXX) $(TEST_CXXFLAGS) -I$(GTEST_DIR) \
		-c $(GTEST_DIR)/src/gtest-all.cc -o $(GTEST_OUT)/gtest-all.o
	ar -rv $(GTEST_LIB) $(GTEST_OUT)/gtest-all.o

# Main rule, build executable
all: $(BIN_PATH)/$(BIN_NAME)

# Rule to build unittests
unittests: $(BIN_PATH)/$(TEST_NAME)

# Link the executable
$(BIN_PATH)/$(BIN_NAME): $(COMMON_OBJS) $(EXE_OBJS)
	$(CXX) $(COMMON_OBJS) $(EXE_OBJS) $(LDFLAGS) -o $@

# Link the tests
$(BIN_PATH)/$(TEST_NAME): $(COMMON_OBJS) $(TEST_OBJS) $(GTEST_LIB)
	$(CXX) $(COMMON_OBJS) $(TEST_OBJS) $(GTEST_LIB) $(TEST_LDFLAGS) \
		$(LDFLAGS) -o $@

# Add dependency files, if they exist
-include $(DEPS)

# Unit tests have additional flags
$(BUILD_PATH)/%_test.o: $(SRC_DIR)/%_test.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(TEST_CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

$(BUILD_PATH)/%.o: $(SRC_DIR)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

ALL_SRC_FILES := \
	$(shell find $(SRC_DIR) -type f -name '*.$(SRC_EXT)' -o -name "*.h")

.PHONY: lint
lint:
	@./third_party/styleguide/cpplint/cpplint.py --verbose=0 \
		--root=$(SRC_DIR) $(ALL_SRC_FILES)

.PHONY: format
format:
	@$(CLANG_FORMAT) -i -style=Chromium $(ALL_SRC_FILES)

.PHONY: presubmit
presubmit: format lint

.PHONY: run_test
run_test: test
	@./$(DEBUG_BIN_PATH)/$(TEST_NAME)

.PHONY: run_mem_test
run_mem_test: test
	@./$(DEBUG_BIN_PATH)/$(TEST_NAME) --debug-memory

.PHONY: coverage
coverage:
	mkdir -p $(COVERAGE_PATH)
	lcov --no-external -c -i -d . -o $(COVERAGE_PATH)/coverage.base
	lcov --no-external -c -d . -o $(COVERAGE_PATH)/coverage.run
	lcov -d . \
		-a $(COVERAGE_PATH)/coverage.base \
		-a $(COVERAGE_PATH)/coverage.run \
		-o $(COVERAGE_PATH)/coverage.total
	lcov -r $(COVERAGE_PATH)/coverage.total "*third_party/*" \
		-o $(COVERAGE_PATH)/coverage.total
	genhtml -o $(COVERAGE_PATH) $(COVERAGE_PATH)/coverage.total
