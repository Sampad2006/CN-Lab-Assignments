CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude

SRCDIR = src
INCDIR = include
BINDIR = .

COMMON_SRCS = $(SRCDIR)/crc.cpp \
              $(SRCDIR)/frame.cpp \
              $(SRCDIR)/socket.cpp \
              $(SRCDIR)/timer.cpp \
              $(SRCDIR)/channel.cpp \
              $(SRCDIR)/protocol.cpp \
              $(SRCDIR)/stop_and_wait.cpp \
              $(SRCDIR)/go_back_n.cpp \
              $(SRCDIR)/selective_repeat.cpp

SENDER_SRC = $(SRCDIR)/sender.cpp
RECEIVER_SRC = $(SRCDIR)/receiver.cpp

COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

all: sender receiver

sender: $(COMMON_OBJS) $(SRCDIR)/sender.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/sender $^

receiver: $(COMMON_OBJS) $(SRCDIR)/receiver.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/receiver $^

HEADERS = $(wildcard $(INCDIR)/*.hpp)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(BINDIR)/sender $(BINDIR)/receiver received.txt test_results.txt

.PHONY: all clean
