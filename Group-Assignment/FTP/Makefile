CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread -Iinclude
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
TESTS_DIR := tests

CORE_SRCS := $(SRC_DIR)/common.cpp \
             $(SRC_DIR)/socket_utils.cpp \
             $(SRC_DIR)/ftp_command.cpp \
             $(SRC_DIR)/ftp_reply.cpp \
             $(SRC_DIR)/sandbox_fs.cpp \
             $(SRC_DIR)/ftp_session.cpp \
             $(SRC_DIR)/ftp_server.cpp \
             $(SRC_DIR)/ftp_client.cpp

CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(CORE_SRCS))

TARGETS := $(BIN_DIR)/ftp_server \
           $(BIN_DIR)/ftp_client \
           $(BIN_DIR)/ftp_sim \
           $(BIN_DIR)/test_command_codec \
           $(BIN_DIR)/test_sandbox_fs \
           $(BIN_DIR)/test_transfer_integrity

.PHONY: all clean test sim server client

all: $(TARGETS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Executables
$(BIN_DIR)/ftp_server: $(CORE_OBJS) $(OBJ_DIR)/main_server.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/ftp_client: $(CORE_OBJS) $(OBJ_DIR)/main_client.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/ftp_sim: $(CORE_OBJS) $(OBJ_DIR)/main_sim.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

server: $(BIN_DIR)/ftp_server
client: $(BIN_DIR)/ftp_client
sim: $(BIN_DIR)/ftp_sim
	@./$(BIN_DIR)/ftp_sim

# Unit Tests
$(BIN_DIR)/test_command_codec: $(OBJ_DIR)/common.o $(OBJ_DIR)/ftp_command.o $(OBJ_DIR)/ftp_reply.o $(TESTS_DIR)/test_command_codec.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_sandbox_fs: $(OBJ_DIR)/common.o $(OBJ_DIR)/sandbox_fs.o $(TESTS_DIR)/test_sandbox_fs.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN_DIR)/test_transfer_integrity: $(OBJ_DIR)/common.o $(TESTS_DIR)/test_transfer_integrity.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BIN_DIR)/test_command_codec $(BIN_DIR)/test_sandbox_fs $(BIN_DIR)/test_transfer_integrity
	@echo "=== Running FTP Unit Tests ==="
	@./$(BIN_DIR)/test_command_codec
	@echo ""
	@./$(BIN_DIR)/test_sandbox_fs
	@echo ""
	@./$(BIN_DIR)/test_transfer_integrity
	@echo "=== All Tests Completed Successfully ==="

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) ./ftp_root ./ftp_sim_root ./ftp_client_scratch
