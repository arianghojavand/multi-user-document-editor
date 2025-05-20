CC := gcc
CFLAGS := -Wall -Wextra -pthread -Ilibs -D_GNU_SOURCE


all: server client

server: source/server.c source/markdown.c
$(CC) $(CFLAGS) source/server.c source/markdown.c -o server

client: source/client.c source/markdown.c
$(CC) $(CFLAGS) source/client.c source/markdown.c -o client

markdown.o: source/markdown.c
$(CC) $(CFLAGS) -c source/markdown.c -o markdown.o

clean:
rm -f server client markdown.o