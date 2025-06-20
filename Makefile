# Makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./src
SRC_DIR = src
BUILD_DIR = build
TARGET = Manlab

SRCS = $(SRC_DIR)/Manlab.cpp \
       $(SRC_DIR)/DBManager.cpp \
	   $(SRC_DIR)/command_handler.cpp \

OBJS = $(SRCS:.cpp=.o)

.PHONY: all run_daemon clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

run_daemon: $(TARGET)
	./$(TARGET) daemon

clean:
	rm -f $(TARGET)