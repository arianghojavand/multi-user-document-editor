CC := gcc
CFLAGS := -Wall -Wextra 

SRC_DIR := source
LIB_DIR := libs

S_OBJECTS = source/server.o
C_OBJECTS = source/client.o
M_OBJECTS = source/markdown.o libs/document.o

CLIENT_OBJS := $(SRC_DIR)/client.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
SERVER_OBJS := $(SRC_DIR)/server.o $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o
MARKDOWN_OBJS := $(SRC_DIR)/markdown.o $(LIB_DIR)/document.o

# Targets
all: client server markdown

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@


markdown: $(MARKDOWN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

markdown.o: $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c -o markdown
	$(CC) $(CFLAGS) -c $(LIB_DIR)/document.c -o document_part.o
	ar rcs markdown.o markdown_part.o document_part.o
	rm -f markdown_part.o document_part.o

document.o: $(LIB_DIR)/document.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

clean:
	rm -f server client markdown markdown.o document.o
	rm -f $(SRC_DIR)/*.o $(LIB_DIR)/*.o