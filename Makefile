CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

.PHONY: all clean directories

all: directories server client

directories:
	@mkdir -p files

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client
	rm -rf files/
