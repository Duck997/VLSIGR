CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I./src -I./third_party
LDFLAGS := -pthread

# Enable debug logging with `make Debug=1`
ifeq ($(Debug),1)
  CXXFLAGS += -DROUTER_DEBUG
endif

SRC_DIR := src
ROUTER_DIR := $(SRC_DIR)/router
API_DIR := $(SRC_DIR)/api
BIN := router
DRAW_BIN := draw

THIRD_PARTY := third_party/LayerAssignment.cpp
SRCS := $(SRC_DIR)/main.cpp $(wildcard $(ROUTER_DIR)/*.cpp) $(wildcard $(API_DIR)/*.cpp) $(SRC_DIR)/tools/draw_lib.cpp $(THIRD_PARTY)
OBJS := $(SRCS:.cpp=.o)

DRAW_SRCS := $(SRC_DIR)/tools/draw.cpp $(SRC_DIR)/router/ispd_data.cpp
DRAW_OBJS := $(DRAW_SRCS:.cpp=.o)

# Cleanup patterns (do not touch .gr inputs)
CLEAN_EXAMPLES := $(wildcard examples/*.ppm) $(wildcard examples/*.txt) $(wildcard examples/*_map.txt) $(wildcard examples/*_stats.txt) $(wildcard examples/*_overflow.ppm) $(wildcard examples/*_nets.ppm)
# Workspace-level generated artifacts (do not touch .gr)
CLEAN_ROOT := $(wildcard *.ppm) $(wildcard *.txt)

TEST_BIN := gtest
TEST_SRCS := $(wildcard tests/*.cpp)
TEST_OBJS := $(TEST_SRCS:.cpp=.o)

.PHONY: all clean test

all: $(BIN) $(DRAW_BIN)

$(BIN): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(DRAW_BIN): $(DRAW_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(TEST_BIN): $(TEST_OBJS) $(filter-out $(SRC_DIR)/main.o,$(OBJS))
	$(CXX) $(LDFLAGS) -o $@ $^ -lgtest -lpthread

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(RM) $(OBJS) $(TEST_OBJS) $(DRAW_OBJS) $(BIN) $(DRAW_BIN) $(TEST_BIN) $(CLEAN_EXAMPLES) $(CLEAN_ROOT)


