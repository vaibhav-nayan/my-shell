CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lreadline -lhistory -lncurses
SRC = src/main.c
OUT = my_shell

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)

docker-build:
	docker build -t my_shell .

docker-run:
	docker run -it my_shell
