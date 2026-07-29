# Logic Analyzer Server - Makefile
# Organizado por capas: core/, hardware/, net/, analysis/
# Licencia: MIT

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pthread -I. -Wno-deprecated-declarations
LDFLAGS  := -lpthread -lcrypto

# Source files by layer
SRCS := main.cpp \
        core/server.cpp core/config.cpp core/logger.cpp core/protocol.cpp \
        hardware/gpio.cpp hardware/ring_buffer.cpp \
        net/websocket.cpp net/sha1.cpp \
        analysis/trigger.cpp

OBJS := $(SRCS:.cpp=.o)

# Detect architecture
UNAME_M := $(shell uname -m)

# On ARM (Raspberry Pi), enable GPIO mmap
ifneq (,$(findstring arm,$(UNAME_M)))
    CXXFLAGS += -DGPIO_MMAP=1
endif
ifneq (,$(findstring aarch64,$(UNAME_M)))
    CXXFLAGS += -DGPIO_MMAP=1
endif

TARGET := logic_server

.PHONY: all clean run test test-build docs

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="
	@echo "Architecture: $(UNAME_M)"
	@echo "GPIO mmap: $(if $(filter arm%,$(UNAME_M)),ENABLED,$(if $(filter aarch64,$(UNAME_M)),ENABLED,DISABLED (simulation)))"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: $(TARGET)
	@echo "=== Starting Logic Analyzer Server on port 8080 ==="
	@echo "Open http://localhost:8080 in your browser"
	./$(TARGET) 8080

test-build: $(TARGET)
	@echo "=== Test build: OK ==="

test: $(TARGET)
	@echo "=== Running smoke test ==="
	@timeout 2 ./$(TARGET) --port=8080 2>&1 || true
	@echo "=== Test complete ==="

docs:
	@echo "=== Generating Doxygen documentation ==="
	@if which doxygen > /dev/null 2>&1; then \
	    mkdir -p ../docs/doxygen/server && \
	    doxygen Doxyfile 2>&1; \
	    echo "Docs: docs/doxygen/server/html/index.html"; \
	else \
	    echo "Warning: doxygen not installed. Install: sudo apt install doxygen"; \
	fi

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf core/*.o hardware/*.o net/*.o analysis/*.o

# Dependencies
core/config.o:      core/config.h
core/logger.o:      core/logger.h
core/protocol.o:    core/protocol.h hardware/ring_buffer.h analysis/trigger.h
core/server.o:      core/server.h hardware/gpio.h hardware/ring_buffer.h \
                    analysis/trigger.h core/config.h net/websocket.h \
                    core/protocol.h core/version.h core/logger.h
hardware/gpio.o:        hardware/gpio.h
hardware/ring_buffer.o: hardware/ring_buffer.h
net/websocket.o:    net/websocket.h net/sha1.h
net/sha1.o:         net/sha1.h
analysis/trigger.o: analysis/trigger.h hardware/ring_buffer.h
main.o:             core/config.h core/server.h core/version.h core/logger.h
