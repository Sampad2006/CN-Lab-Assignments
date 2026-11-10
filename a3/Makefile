CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude

SRCDIR = src
INCDIR = include
BINDIR = .

COMMON_SRCS = $(SRCDIR)/frame.cpp \
              $(SRCDIR)/socket.cpp \
              $(SRCDIR)/channel.cpp \
              $(SRCDIR)/csma.cpp

COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

all: server station simulator

server: $(COMMON_OBJS) $(SRCDIR)/server.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/server $^

station: $(COMMON_OBJS) $(SRCDIR)/station.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/station $^

simulator: $(COMMON_OBJS) $(SRCDIR)/simulator.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/simulator $^

HEADERS = $(wildcard $(INCDIR)/*.hpp)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(BINDIR)/server $(BINDIR)/station $(BINDIR)/simulator $(BINDIR)/test_suite

test: $(COMMON_OBJS) tests/test_suite.o
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/test_suite $^
	./test_suite

.PHONY: all clean test
