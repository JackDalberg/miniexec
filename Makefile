BUILD_DIR ?= build
EXAMPLE_FILES := examples/cpp_reference.cpp
TEST_FILES := tests/main.cpp

TESTS := $(BUILD_DIR)/mini_tests
EXAMPLES := $(BUILD_DIR)/mini_examples

.PHONY: test example clean

$(BUILD_DIR):
	mkdir -p $@

$(TESTS): $(BUILD_DIR)
	g++ -Iinclude -std=c++26 -g -Wall -Wextra -pthread $(TEST_FILES) -o $@

$(EXAMPLES): $(BUILD_DIR)
	g++ -Iinclude -std=c++26 -g -Wall -Wextra -pthread $(EXAMPLE_FILES) -o $@

test: $(TESTS)
	./$(TESTS)

example: $(EXAMPLES)
	./$(EXAMPLES)

clean:
	rm -rf $(BUILD_DIR)
