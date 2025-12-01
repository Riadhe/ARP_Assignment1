#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"

void run_obstacles() {
    printf("[Obstacles] Starting...\n");
    
    // Seed random generator
    srand(time(NULL) + getpid());
    
    // Wait for pipe to be available
    int fd_obs_to_server;
    while ((fd_obs_to_server = open(PIPE_OBS_TO_SERVER, O_WRONLY)) < 0) {
        usleep(100000);
    }

    Message msg;
    msg.type = MSG_OBSTACLE;
    msg.sender_pid = getpid();
    
    int count = 0;
    while (1) {
        if (count < MAX_OBSTACLES) {
            msg.obstacle.id = count;
            msg.obstacle.position.x = 2 + rand() % (MAP_WIDTH - 4);
            msg.obstacle.position.y = 2 + rand() % (MAP_HEIGHT - 4);
            
            write(fd_obs_to_server, &msg, sizeof(Message));
            
            count++;
        }
        
        // Spawn interval: 0.5 seconds
        usleep(500000);
    }
    
    close(fd_obs_to_server);
}