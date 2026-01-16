#include "common.h"
#include <time.h>

// This function matches the Assignment 2 behavior:
// 30 Obstacles that stay mostly still, but occasionally refresh.
void run_obstacles() {
    register_process("Obstacles");
    log_message(SYSTEM_LOG_FILE, "Obstacles", "Generator started (Assignment 2 Mode).");

    int fd;
    // Wait for Blackboard pipe
    while ((fd = open(PIPE_OBS_TO_SERVER, O_WRONLY)) < 0) {
        usleep(100000); 
    }

    srand(time(NULL) + getpid());

    Obstacle obstacles[MAX_OBSTACLES];
    Message msg;
    msg.type = MSG_OBSTACLE;
    msg.sender_pid = getpid();

    // 1. INITIALIZATION: Fill the map with 30 obstacles
    // This makes sure Repulsion works immediately.
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].id = i; // Assign IDs 0 to 29
        obstacles[i].position.x = 5 + rand() % (MAP_WIDTH - 10);
        obstacles[i].position.y = 5 + rand() % (MAP_HEIGHT - 10);
        
        msg.obstacle = obstacles[i];
        write(fd, &msg, sizeof(Message));
        
        // Tiny sleep to ensure Blackboard reads them all safely
        usleep(2000); 
    }

    log_message(SYSTEM_LOG_FILE, "Obstacles", "Initialized %d obstacles.", MAX_OBSTACLES);

    // 2. MAIN LOOP: Slow Refresh
    while (1) {
        // Sleep for 4 seconds (Adjust this if you want faster/slower updates)
        // In Assignment 2, obstacles shouldn't flicker like a disco light.
        sleep(4); 

        // Pick ONE random obstacle ID (0-29) to move
        int id = rand() % MAX_OBSTACLES;

        obstacles[id].position.x = 5 + rand() % (MAP_WIDTH - 10);
        obstacles[id].position.y = 5 + rand() % (MAP_HEIGHT - 10);

        msg.obstacle = obstacles[id];
        write(fd, &msg, sizeof(Message));
        
        // Log it (Optional debug)
        // log_message(SYSTEM_LOG_FILE, "Obstacles", "Moved obstacle %d", id);
    }

    close(fd);
}