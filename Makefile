# Compiler and flags
CC := gcc
CFLAGS := -Wvla -g -fsanitize=address # -Werror -Wall -Wextra 

# Directories
SRC_DIR := source
LIB_DIR := libs

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Source files
SOURCES := $(SRC_DIR)/client.c $(SRC_DIR)/server.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c

# Object files (optional)
OBJECTS := $(SOURCES:.c=.o)

# Executables
EXECUTABLES := client server

all: $(EXECUTABLES)

client: $(SRC_DIR)/client.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c
	$(CC) $(CFLAGS) $(SRC_DIR)/client.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c -o client

server: $(SRC_DIR)/server.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c
	$(CC) $(CFLAGS) $(SRC_DIR)/server.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c -o server

clean:
	rm -f $(EXECUTABLES) $(OBJECTS)