CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -MMD -MP -Iinclude
LDFLAGS :=

TARGET := bin/tplay
SRC_DIR := src
OBJ_DIR := obj

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

bin:
	mkdir -p bin

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

-include $(DEPS)
