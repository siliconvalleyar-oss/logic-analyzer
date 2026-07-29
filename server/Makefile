# Logic Analyzer Server - Makefile
# Organizado por capas: core/, hardware/, net/, analysis/
# Los objetos compilados (.o) y dependencias (.d) se alojan en obj/
# Licencia: MIT

CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pthread -I. -Wno-deprecated-declarations
LDFLAGS  := -lpthread -lcrypto

OBJ_DIR  := obj

# Source files by layer (ruta relativa a este Makefile)
SRCS := main.cpp \
        core/server.cpp core/config.cpp core/logger.cpp core/protocol.cpp \
        hardware/gpio.cpp hardware/ring_buffer.cpp \
        net/websocket.cpp net/sha1.cpp \
        analysis/trigger.cpp

# Object files → obj/main.o obj/core/server.o obj/hardware/gpio.o …
OBJS := $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))
# Dependency files generados automaticamente por -MMD -MP
DEPS := $(OBJS:.o=.d)

# Detect architecture (ARM = Raspberry Pi)
UNAME_M := $(shell uname -m)
IS_ARM  := $(or $(findstring arm,$(UNAME_M)),$(findstring aarch64,$(UNAME_M)))

# On ARM, enable GPIO mmap and link libatomic (needed for 8-byte atomics on 32-bit)
ifneq (,$(IS_ARM))
    CXXFLAGS += -DGPIO_MMAP=1
    LDFLAGS  += -latomic
endif

TARGET := logic_server

.PHONY: all clean run test test-build docs

all: $(TARGET)

# Link: los OBJS ya incluyen prefijo obj/
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "=== Build complete: $(TARGET) ==="
	@echo "Architecture: $(UNAME_M)"
	@echo "GPIO mmap: $(if $(filter arm%,$(UNAME_M)),ENABLED,$(if $(filter aarch64,$(UNAME_M)),ENABLED,DISABLED (simulation)))"

# Compilar .cpp → .o en obj/ manteniendo subdirectorios
# -MMD -MP genera automaticamente los .d con dependencias de headers
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Incluir dependencias generadas (silenciosamente si no existen aun)
-include $(wildcard $(DEPS))

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
	rm -rf $(OBJ_DIR) $(TARGET)
