# Compiler and Flags
CC = gcc
CFLAGS = -Wall -g -I src
LIBS = -lncurses -lm

# Targets
all: main map input

# 1. Main System
main: src/main.c src/blackboard.c src/dynamics.c src/obstacles.c src/targets.c src/params.c src/common.h
	$(CC) $(CFLAGS) src/main.c src/blackboard.c src/dynamics.c src/obstacles.c src/targets.c src/params.c -o main $(LIBS)

# 2. Map Window
map: src/ui_map.c src/common.h
	$(CC) $(CFLAGS) src/ui_map.c -o map $(LIBS)

# 3. Input Window (Needs params.c to read F_STEP)
input: src/ui_input.c src/params.c src/common.h
	$(CC) $(CFLAGS) src/ui_input.c src/params.c -o input $(LIBS)

# Clean up
clean:
	rm -f main map input *.log /tmp/fifo_*