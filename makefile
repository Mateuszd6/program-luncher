all:
	mkdir -p ./bin
	gcc -Wall -Wextra -Wshadow --std=c11 -lX11 -g -o ./bin/program main.c
