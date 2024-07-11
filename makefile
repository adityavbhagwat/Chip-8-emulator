CFLAGS=-std=c17 -Wall -Wextra -Werror
all:
	gcc -I src/include -L src/lib  chip8.c -o chip8 $(CFLAGS) `sdl2-config --cflags --libs`