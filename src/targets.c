#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"

void run_targets() {
    printf("[Targets] Starting...\n");
    srand(time(NULL) + getpid() + 100);
    // Wait for pipe to be available
    int fd_tar_to_server;
    while ((fd_tar_to_server = open(PIPE_TAR_TO_SERVER, O_WRONLY)) < 0) {
        usleep(100000);
    }

    Message msg;
    msg.type = MSG_TARGET;
    msg.sender_pid = getpid();

    // Spawn 20 targets initially
    for (int i = 0; i < MAX_TARGETS; i++) {
        msg.target.id = i;
        msg.target.position.x = 5 + rand() % (MAP_WIDTH - 10);
        msg.target.position.y = 5 + rand() % (MAP_HEIGHT - 10);
        msg.target.active = 1; 
        
        write(fd_tar_to_server, &msg, sizeof(Message));
        usleep(100000); 
    }
    
    // Idle loop
    while (1) sleep(10);
    
    close(fd_tar_to_server);
}