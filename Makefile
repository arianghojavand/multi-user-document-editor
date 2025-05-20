CFLAGS := -Wall -Wextra -Werror -Wvla -g -fsanitize=address

# Directories
SRC_DIR := source
LIB_DIR := libs

# Source files
SOURCES := $(SRC_DIR)/client.c $(SRC_DIR)/server.c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c

# Object files
OBJECTS := $(SOURCES:.c=.o)

# Executables
EXECUTABLES := client server

# Pattern rule to compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Default rule
all: $(EXECUTABLES)

client: $(SRC_DIR)/client.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
	$(CC) $(CFLAGS) $^ -o $@

server: $(SRC_DIR)/server.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(EXECUTABLES) $(OBJECTS)