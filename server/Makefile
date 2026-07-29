# Logic Analyzer Server - Makefile
# Modular build: main.cpp + gpio + ring_buffer + websocket + sha1 + 
#                 trigger + protocol + config + logger + server
# Licencia: MIT

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pthread
LDFLAGS  := -lpthread

# Source files (all modules)
SRCS := main.cpp gpio.cpp ring_buffer.cpp websocket.cpp sha1.cpp \
        trigger.cpp protocol.cpp config.cpp logger.cpp server.cpp
OBJS := $(SRCS:.cpp=.o)

# Detect architecture
UNAME_M := $(shell uname -m)
UNAME_S := $(shell uname -s)

# On ARM (Raspberry Pi), enable GPIO mmap
ifneq (,$(findstring arm,$(UNAME_M)))
    CXXFLAGS += -DGPIO_MMAP=1
endif
ifneq (,$(findstring aarch64,$(UNAME_M)))
    CXXFLAGS += -DGPIO_MMAP=1
endif

# Target
TARGET := logic_server

.PHONY: all clean run test test-build docs

all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="
	@echo "Architecture: $(UNAME_M)"
	@echo "GPIO mmap: $(if $(filter arm%,$(UNAME_M)),ENABLED,$(if $(filter aarch64,$(UNAME_M)),ENABLED,DISABLED (simulation)))"

# Compilation rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: $(TARGET)
	@echo "=== Starting Logic Analyzer Server on port 8080 ==="
	@echo "Open http://localhost:8080 in your browser"
	./$(TARGET) 8080

# Test build (compiles but doesn't run)
test-build: $(TARGET)
	@echo "=== Test build: OK ==="

# Quick smoke test
test: $(TARGET)
	@echo "=== Running smoke test ==="
	@timeout 2 ./$(TARGET) --port=8080 2>&1 || true
	@echo "=== Test complete ==="

# Doxygen documentation
docs:
	@echo "Generating documentation..."
	@which doxygen > /dev/null 2>&1 && doxygen Doxyfile 2>/dev/null || \
	 echo "Warning: doxygen not installed. Install with: sudo apt install doxygen"
	@echo "Docs: docs/doxygen/html/index.html"

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
gpio.o:        gpio.h
ring_buffer.o: ring_buffer.h
websocket.o:   websocket.h sha1.h
sha1.o:        sha1.h
trigger.o:     trigger.h ring_buffer.h
protocol.o:    protocol.h ring_buffer.h trigger.h
config.o:      config.h
logger.o:      logger.h
server.o:      server.h gpio.h ring_buffer.h trigger.h config.h \
              websocket.h protocol.h version.h logger.h
main.o:        config.h server.h version.h logger.h
