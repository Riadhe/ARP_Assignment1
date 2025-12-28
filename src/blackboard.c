#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

// File Descriptors 
static int fd_ui_to_server;
static int fd_dyn_to_server;
static int fd_obs_to_server;
static int fd_tar_to_server;

static int fd_server_to_map;
static int fd_server_to_dyn;
static int fd_server_to_ui_input;

// The "World State" storage
static DroneState drone;
static Obstacle obstacles[MAX_OBSTACLES];
static Target targets[MAX_TARGETS];
static int obs_count = 0;
static int tar_count = 0;

// Handle incoming messages from different sources
// This function decides where to send data based on what type it is.
void handle_message(Message *msg) {
    switch (msg->type) {
        case MSG_DRONE_STATE:
            drone = msg->drone;
            // Dynamics sent position -> Send to Map (to draw) and Input (to show numbers)
            write(fd_server_to_map, msg, sizeof(Message));
            write(fd_server_to_ui_input, msg, sizeof(Message));
            break;

        case MSG_FORCE_UPDATE:
            // Input sent keys -> Send to Dynamics (to calculate physics)
            write(fd_server_to_dyn, msg, sizeof(Message));
            break;

        case MSG_OBSTACLE:
            // Store obstacle locally
            if (obs_count < MAX_OBSTACLES) {
                obstacles[obs_count++] = msg->obstacle;
                // Tell Map (to draw) and Dynamics (to bounce off)
                write(fd_server_to_map, msg, sizeof(Message));
                write(fd_server_to_dyn, msg, sizeof(Message));
            }
            break;

        case MSG_TARGET:
            if (msg->target.id < MAX_TARGETS) {
                targets[msg->target.id] = msg->target;
                
                // Track how many targets exist to avoid unused variable warning
                if (msg->target.id >= tar_count) {
                    tar_count = msg->target.id + 1;
                }
                
                // Forward update to Map and Dynamics
                write(fd_server_to_map, msg, sizeof(Message));
                write(fd_server_to_dyn, msg, sizeof(Message));
            }
            break;
            
        case MSG_STOP:
            // Broadcast STOP to everyone so they close cleanly
            write(fd_server_to_map, msg, sizeof(Message));
            write(fd_server_to_dyn, msg, sizeof(Message));
            write(fd_server_to_ui_input, msg, sizeof(Message));
            printf("[Blackboard] Stop signal. Exiting.\n");
            exit(0);
            break;
    }
}

void run_blackboard() {
    // Additionnal Line of Assignment 2: Register process and log startup
    register_process("Blackboard"); 
    log_message(SYSTEM_LOG_FILE, "Blackboard", "Server started."); // 

    
    printf("[Blackboard] Active.\n");
    fflush(stdout); // Force print immediately

    // 1. Create Named Pipes (FIFOs) on the hard drive
    mkfifo(PIPE_UI_TO_SERVER, 0666);
    mkfifo(PIPE_DYN_TO_SERVER, 0666);
    mkfifo(PIPE_OBS_TO_SERVER, 0666);
    mkfifo(PIPE_TAR_TO_SERVER, 0666);
    mkfifo(PIPE_SERVER_TO_MAP, 0666);
    mkfifo(PIPE_SERVER_TO_DYN, 0666);
    mkfifo(PIPE_SERVER_TO_UI_INPUT, 0666);

    // 2. Open INPUT Pipes (Non-Blocking)
    // O_RDWR trick: We open Read pipes with Write permission too.
    // This stops the pipe from sending "End of File" if a client disconnects.
    fd_ui_to_server  = open(PIPE_UI_TO_SERVER,  O_RDWR | O_NONBLOCK);
    fd_dyn_to_server = open(PIPE_DYN_TO_SERVER, O_RDWR | O_NONBLOCK);
    fd_obs_to_server = open(PIPE_OBS_TO_SERVER, O_RDWR | O_NONBLOCK);
    fd_tar_to_server = open(PIPE_TAR_TO_SERVER, O_RDWR | O_NONBLOCK);

    // 3. Open OUTPUT Pipes
    fd_server_to_map      = open(PIPE_SERVER_TO_MAP, O_RDWR); 
    fd_server_to_dyn      = open(PIPE_SERVER_TO_DYN, O_RDWR);
    fd_server_to_ui_input = open(PIPE_SERVER_TO_UI_INPUT, O_RDWR);

    Message msg;
    while (1) {
        // Check every pipe. If read > 0, we handle it.
        if (read(fd_ui_to_server, &msg, sizeof(Message)) > 0) handle_message(&msg);
        if (read(fd_dyn_to_server, &msg, sizeof(Message)) > 0) handle_message(&msg);
        if (read(fd_obs_to_server, &msg, sizeof(Message)) > 0) handle_message(&msg);
        if (read(fd_tar_to_server, &msg, sizeof(Message)) > 0) handle_message(&msg);
        
        // Sleep 1ms to save CPU
        usleep(1000); 
    }
}