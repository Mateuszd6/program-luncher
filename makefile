all:
	mkdir -p ./bin/
	gcc -Wall -Wextra -std=gnu11 -g -O0 -DDEBUG -lX11 -lpthread main.c -o ./bin/program
