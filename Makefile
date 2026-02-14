CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
ifeq ($(OS),Windows_NT)
    LDFLAGS = -static
else
    LDFLAGS =
endif

ifeq ($(OS),Windows_NT)
    TARGET = shadowcrypt.exe
    export TMPDIR = $(HOME)/tmp
    export TMP = $(HOME)/tmp
    export TEMP = $(HOME)/tmp
else
    TARGET = shadowcrypt
endif

SRC_DIR = src
SRCS = $(filter-out $(SRC_DIR)/screenshot.cpp, $(wildcard $(SRC_DIR)/*.cpp))
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)

.PHONY: all clean
