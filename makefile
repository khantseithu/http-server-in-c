CC = clang
CFLAGS += -Wall -Wextra -pedantic
# For Apple Silicon (M1/M2) Macs:
INCLUDES = -I/opt/homebrew/opt/openssl/include
LDFLAGS = -L/opt/homebrew/opt/openssl/lib -lssl -lcrypto

-include makefile.local

build:
	$(CC) $(CFLAGS) $(INCLUDES) src/server.c src/debug.c src/utils.c -o server $(LDFLAGS)
