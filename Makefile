# Makefile for SHAM Protocol Implementation

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lssl -lcrypto

# Source files
COMMON_SOURCES = sham.c
SERVER_SOURCES = server.c $(COMMON_SOURCES)
CLIENT_SOURCES = client.c $(COMMON_SOURCES)

# Output executables
SERVER_TARGET = server
CLIENT_TARGET = client

# Object files
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Server target
$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CC) $(SERVER_OBJECTS) -o $(SERVER_TARGET) $(LDFLAGS)

# Client target
$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CLIENT_OBJECTS) -o $(CLIENT_TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f *.o core

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libssl-dev build-essential


.PHONY: all clean install-deps test-file test-chat create-test-file help
