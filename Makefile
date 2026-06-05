CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -I./src
TARGET = c-mcp-server
SRCS = src/main.c src/api.c src/http_server.c src/cJSON.c src/tools.c src/resources.c
OBJS = build/main.o build/api.o build/http_server.o build/cJSON.o build/tools.o build/resources.o
BUILD_DIR = build

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(TARGET) $(OBJS) $(BUILD_DIR)
