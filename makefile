all:
	mkdir -p ./bin
	gcc -Wall -Wextra -Wshadow --std=c11 -lm -lX11 -g -O0 -o ./bin/program main.c
