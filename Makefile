CXX ?= g++
CPPFLAGS ?= -Isrc -Iclient
CXXFLAGS ?= -std=c++20 -Wall -Wextra -MMD -MP
LDFLAGS ?=
LDLIBS ?= -pthread

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/qdbf-server
CLIENT_BIN := $(BUILD_DIR)/qdbf-client

SERVER_SOURCES := $(wildcard src/*.cpp)
COMMON_SOURCES := $(filter-out src/qdbf.cpp,$(SERVER_SOURCES))
CLIENT_SOURCES := $(COMMON_SOURCES) client/qdbf_cli.cpp

SERVER_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/server/%.o,$(SERVER_SOURCES))
CLIENT_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/client/%.o,$(CLIENT_SOURCES))

.PHONY: all debug release clean run-server run-client

all: debug

debug: CXXFLAGS += -g -O0

debug: $(SERVER_BIN) $(CLIENT_BIN)

release: CXXFLAGS += -O2 -DNDEBUG

release: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CLIENT_BIN): $(CLIENT_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/server/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/client/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -rf $(BUILD_DIR)

-include $(SERVER_OBJECTS:.o=.d) $(CLIENT_OBJECTS:.o=.d)
