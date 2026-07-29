# Logic Analyzer Server - Makefile
# Builds the C++ server with mmap GPIO support (Raspberry Pi)
# and simulation mode (x86_64 for development)

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pthread
LDFLAGS  := -lpthread

# Detect architecture
UNAME_M := $(shell uname -m)
UNAME_S := $(shell uname -s)

# On ARM (Raspberry Pi), enable GPIO mmap
ifeq ($(findstring arm,$(UNAME_M)),arm)
    CXXFLAGS += -DGPIO_MMAP=1
endif
ifeq ($(findstring aarch64,$(UNAME_M)),aarch64)
    CXXFLAGS += -DGPIO_MMAP=1
endif

# Target
TARGET := logic_server

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="
	@echo "Architecture: $(UNAME_M)"
	@echo "GPIO mmap: $([ $(UNAME_M) =~ arm ] && echo ENABLED || echo DISABLED (simulation mode))"

run: $(TARGET)
	@echo "=== Starting Logic Analyzer Server on port 8080 ==="
	@echo "Open http://localhost:8080 in your browser"
	./$(TARGET) 8080

clean:
	rm -f $(TARGET)
