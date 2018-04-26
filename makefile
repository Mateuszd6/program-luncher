all:
	mkdir -p ./bin/
	gcc -Wall -Wextra -std=c11 -g -O0 -DDEBUG -lX11 -lm main.c -o ./bin/program
