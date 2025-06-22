# Makefile for building the C++ libcurl wrapper on Linux and Windows (MinGW)

# Compiler and base flags
CXX = g++
# Use C++23, enable all warnings, and optimize. -pthread is for thread safety.
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -pthread
# Linker flags
LDFLAGS = -lcurl

# --- Platform-specific setup ---
# Default executable name
TARGET = http_client_app

# Detect OS and adjust settings. This must be done before TARGET is used in rules.
ifeq ($(OS),Windows_NT)
    # --- Windows (MinGW/MSYS2) specific settings ---
    TARGET := $(TARGET).exe
    RM = del /Q /F
else
    # --- Linux/macOS specific settings ---
    RM = rm -f
endif

# --- File definitions ---
# Source and object files
SRCS = main.cpp http_client.cpp
OBJS = $(SRCS:.cpp=.o)

# --- Rules ---

# Default target: the first rule in the file
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Rule to compile a .cpp file into a .o file
# It depends on the .cpp file and the main header to ensure recompilation if the header changes.
%.o: %.cpp http_client.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up build artifacts
clean:
	$(RM) $(OBJS) $(TARGET)

# Phony targets are not files.
.PHONY: all clean

