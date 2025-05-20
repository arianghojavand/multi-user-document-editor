# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Werror -Wvla -g -fsanitize=address

# Directories
SRC_DIR := source
LIB_DIR := libs

# Source and object files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
LIB_FILES := $(wildcard $(LIB_DIR)/*.c)

SRC_OBJECTS := $(SRC_FILES:.c=.o)
LIB_OBJECTS := $(LIB_FILES:.c=.o)

OBJECTS := $(SRC_OBJECTS) $(LIB_OBJECTS)

# Executables
EXECUTABLES := client server

# Default rule
all: $(EXECUTABLES)

# Object file compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Specific executables
client: $(SRC_DIR)/client.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
	$(CC) $(CFLAGS) $^ -o $@

server: $(SRC_DIR)/server.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
	$(CC) $(CFLAGS) $^ -o $@

# Aliases for automarker
markdown.o: $(SRC_DIR)/markdown.o
	cp $< $@

document.o: $(LIB_DIR)/document.o
	cp $< $@

clean:
	rm -f $(EXECUTABLES) $(OBJECTS) markdown.o document.o