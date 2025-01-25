CC = clang  # or gcc-13
CFLAGS = -Wall -Wextra -pedantic

build:
	$(CC) $(CFLAGS) -o server src/server.c

run:
	./server

clean:
	rm -f server