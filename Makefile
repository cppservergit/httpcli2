# Makefile for building the C++ libcurl wrapper on Linux and Windows (MinGW)

# Compiler and base flags
CXX = g++
# Use C++23, enable all warnings, and optimize. -pthread is for thread safety.
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -pthread
# Linker flags
LDFLAGS = -lcurl
# Archiver command for creating static libraries
AR = ar
ARFLAGS = rcs

# --- Platform-specific setup ---
# Default executable name
TARGET = http_client_app
# Static library name
LIB_TARGET = libhttpclient.a

# Detect OS and adjust settings. This must be done before TARGET is used in rules.
ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    RM = del /Q /F
else
    # --- Linux/macOS specific settings ---
    RM = rm -f
endif

# --- File definitions ---
# Source and object files for the executable
APP_SRCS = main.cpp http_client.cpp
APP_OBJS = $(APP_SRCS:.cpp=.o)

# Source and object files for the static library
LIB_SRCS = http_client.cpp
LIB_OBJS = $(LIB_SRCS:.cpp=.o)

# --- Rules ---

# Default target: build the executable application
all: $(TARGET)

# Target to build only the static library
lib: $(LIB_TARGET)

# New target to build only the library's object files (.o)
objects: $(LIB_OBJS)

# Rule to link the final executable
$(TARGET): $(APP_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_OBJS) $(LDFLAGS)

# Rule to create the static library
$(LIB_TARGET): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

# Rule to compile a .cpp file into a .o file
# It depends on the .cpp file and the main header to ensure recompilation if the header changes.
%.o: %.cpp http_client.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up all build artifacts
clean:
	$(RM) $(APP_OBJS) $(TARGET) $(LIB_TARGET)

# Phony targets are not files.
.PHONY: all lib objects clean

