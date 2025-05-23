CC := gcc
CFLAGS := -Wall -Wextra -fsanitize=address -g -pthread

SRC_DIR := source
LIB_DIR := libs

all: server client

markdown.o: $(SRC_DIR)/markdown.c $(LIB_DIR)/document.c
	$(CC) $(CFLAGS) -c $(SRC_DIR)/markdown.c -o markdown_part.o
	$(CC) $(CFLAGS) -c $(LIB_DIR)/document.c -o document_part.o
	ld -r markdown_part.o document_part.o -o markdown.o
	rm -f markdown_part.o document_part.o

server: $(SRC_DIR)/server.c markdown.o
	$(CC) $(CFLAGS) -o $@ $^

# === Rule: client binary ===
client: $(SRC_DIR)/client.c markdown.o
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: all clean

clean:
	rm -f server client markdown.o
	rm -f $(SRC_DIR)/*.o $(LIB_DIR)/*.o
	rm -f *.out $(SRC_DIR)/*.out $(LIB_DIR)/*.out
	rm -f FIFO_C2S_* FIFO_S2C_*
	rm -f *.md