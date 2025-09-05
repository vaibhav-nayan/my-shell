CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/main.c
OUT = my_shell

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)
