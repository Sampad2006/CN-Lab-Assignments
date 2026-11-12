CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread -Iinclude
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
TESTS_DIR := tests

CORE_SRCS := $(SRC_DIR)/types.cpp \
             $(SRC_DIR)/arp_packet.cpp \
             $(SRC_DIR)/arp_cache.cpp \
             $(SRC_DIR)/virtual_switch.cpp \
             $(SRC_DIR)/host_node.cpp

CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(CORE_SRCS))

TARGETS := $(BIN_DIR)/arp_sim \
           $(BIN_DIR)/host_cli \
           $(BIN_DIR)/switch_daemon \
           $(BIN_DIR)/test_codec \
           $(BIN_DIR)/test_arp_cache

.PHONY: all clean test sim

all: $(TARGETS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Executables
$(BIN_DIR)/arp_sim: $(CORE_OBJS) $(OBJ_DIR)/main_sim.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/host_cli: $(CORE_OBJS) $(OBJ_DIR)/main_host.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/switch_daemon: $(CORE_OBJS) $(OBJ_DIR)/main_switch.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Unit Tests
$(BIN_DIR)/test_codec: $(OBJ_DIR)/types.o $(OBJ_DIR)/arp_packet.o $(TESTS_DIR)/test_codec.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_arp_cache: $(OBJ_DIR)/types.o $(OBJ_DIR)/arp_cache.o $(TESTS_DIR)/test_arp_cache.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BIN_DIR)/test_codec $(BIN_DIR)/test_arp_cache
	@echo "=== Running ARP Unit Tests ==="
	@./$(BIN_DIR)/test_codec
	@echo ""
	@./$(BIN_DIR)/test_arp_cache
	@echo "=== All Tests Completed Successfully ==="

sim: $(BIN_DIR)/arp_sim
	@./$(BIN_DIR)/arp_sim

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
