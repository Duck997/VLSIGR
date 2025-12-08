CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I./src
LDFLAGS := 

SRC_DIR := src
ROUTER_DIR := $(SRC_DIR)/router
BIN := router

SRCS := $(SRC_DIR)/main.cpp $(wildcard $(ROUTER_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)

TEST_BIN := gtest
TEST_SRCS := $(wildcard tests/*.cpp)
TEST_OBJS := $(TEST_SRCS:.cpp=.o)

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(TEST_BIN): $(TEST_OBJS) $(filter-out $(SRC_DIR)/main.o,$(OBJS))
	$(CXX) $(LDFLAGS) -o $@ $^ -lgtest -lpthread

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(RM) $(OBJS) $(TEST_OBJS) $(BIN) $(TEST_BIN)


