# Compiler and Flags
CC = gcc
CFLAGS = -Wall -g -I src
LIBS = -lncurses -lm

# Targets
all: main map input watchdog

# 1. Main System (Updated for Network Mode)
main: src/main.c src/blackboard.c src/socket_manager.c src/dynamics.c src/obstacles.c src/targets.c src/params.c src/utilities.c src/common.h
	$(CC) $(CFLAGS) src/main.c src/blackboard.c src/socket_manager.c src/dynamics.c src/obstacles.c src/targets.c src/params.c src/utilities.c -o main $(LIBS)

# 2. Map Window
map: src/ui_map.c src/utilities.c src/common.h
	$(CC) $(CFLAGS) src/ui_map.c src/utilities.c -o map $(LIBS)

# 3. Input Window
input: src/ui_input.c src/params.c src/utilities.c src/common.h
	$(CC) $(CFLAGS) src/ui_input.c src/params.c src/utilities.c -o input $(LIBS)

# 4. Watchdog
watchdog: src/watchdog.c src/utilities.c src/common.h
	$(CC) $(CFLAGS) src/watchdog.c src/utilities.c -o watchdog $(LIBS)

# Clean up
clean:
	rm -f main map input watchdog *.log process_list.txt /tmp/fifo_*