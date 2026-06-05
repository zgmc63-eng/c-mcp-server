CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
CPPFLAGS = -I./src
LDFLAGS =
TARGET = c-mcp-server
BUILD_DIR = build
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(TARGET) $(BUILD_DIR)
