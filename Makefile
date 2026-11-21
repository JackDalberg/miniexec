BUILD_DIR ?= build
TESTS := $(BUILD_DIR)/miniexec_tests

CXX := g++
CXXFLAGS := -Iinclude -std=c++26 -g -Wall -Wextra -pthread

HEADERS := $(wildcard include/miniexec/*.hpp)
TEST_FILES := test/main.cpp test/cpp_ref.cpp

.PHONY: test clean

test: $(TESTS)
	./$(TESTS)

$(TESTS): $(TEST_FILES) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_FILES) -o $@

clean:
	rm -rf $(BUILD_DIR)
