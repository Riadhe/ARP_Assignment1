#include "common.h"
#include "socket_manager.h" 
#include <locale.h>

// Global State
DroneState drone;
Obstacle obstacles[MAX_OBSTACLES];
Target targets[MAX_TARGETS];
int obs_count = 0;

void run_blackboard(int mode) {
    setlocale(LC_NUMERIC, "C");
    register_process("Blackboard");
    log_message(SYSTEM_LOG_FILE, "Blackboard", "Started in mode %d", mode);

    for (int i = 0; i < MAX_OBSTACLES; i++) obstacles[i].id = -1;
    for (int i = 0; i < MAX_TARGETS; i++) { targets[i].id = -1; targets[i].active = 0; }

    // Pipe Setup
    int fd_ui_in, fd_dyn_in, fd_obs_in, fd_tar_in;
    int fd_ui_out, fd_ui_input_out, fd_dyn_out;

    while ((fd_ui_in = open(PIPE_UI_TO_SERVER, O_RDONLY | O_NONBLOCK)) < 0) usleep(1000);
    while ((fd_dyn_in = open(PIPE_DYN_TO_SERVER, O_RDONLY | O_NONBLOCK)) < 0) usleep(1000);
    
    if (mode == MODE_STANDALONE) {
        while ((fd_obs_in = open(PIPE_OBS_TO_SERVER, O_RDONLY | O_NONBLOCK)) < 0) usleep(1000);
        while ((fd_tar_in = open(PIPE_TAR_TO_SERVER, O_RDONLY | O_NONBLOCK)) < 0) usleep(1000);
    }

    mkfifo(PIPE_SERVER_TO_UI_INPUT, 0666);
    mkfifo(PIPE_SERVER_TO_MAP, 0666);
    mkfifo(PIPE_SERVER_TO_DYN, 0666);
    
    fd_ui_out = open(PIPE_SERVER_TO_MAP, O_WRONLY);
    fd_ui_input_out = open(PIPE_SERVER_TO_UI_INPUT, O_WRONLY);
    fd_dyn_out = open(PIPE_SERVER_TO_DYN, O_WRONLY);

    // Network Setup
    int sockfd = -1;
    if (mode != MODE_STANDALONE) {
        int port = SERVER_PORT;
        sockfd = init_network(mode, &port);
        if (sockfd < 0) {
            log_message(SYSTEM_LOG_FILE, "Blackboard", "Network Init Failed!");
            exit(1);
        }
        if (sync_handshake(mode, sockfd) < 0) {
            log_message(SYSTEM_LOG_FILE, "Blackboard", "Handshake Failed!");
            close(sockfd);
            exit(1);
        }
    }

    Message msg_in, msg_out;
    int running = 1;
    Obstacle opponent = {0};
    opponent.id = 0; 

    // Rate Limiting
    int net_tick = 0;
    const int NET_RATE = 10; 

    // MAIN LOOP
    while (running) {
        // A. Read Local Inputs
        while (read(fd_ui_in, &msg_in, sizeof(Message)) > 0) {
            if (msg_in.type == MSG_STOP) running = 0;
            else if (msg_in.type == MSG_FORCE_UPDATE) write(fd_dyn_out, &msg_in, sizeof(Message));
        }
        while (read(fd_dyn_in, &msg_in, sizeof(Message)) > 0) {
            if (msg_in.type == MSG_DRONE_STATE) drone = msg_in.drone;
            else if (msg_in.type == MSG_TARGET && mode == MODE_STANDALONE) {
                if (msg_in.target.id < MAX_TARGETS) targets[msg_in.target.id] = msg_in.target;
            }
        }

        // B. Handle Environment
        if (mode == MODE_STANDALONE) {
            while (read(fd_obs_in, &msg_in, sizeof(Message)) > 0) {
                int id = msg_in.obstacle.id;
                if (id >= 0 && id < MAX_OBSTACLES) {
                    obstacles[id] = msg_in.obstacle;
                    if (id >= obs_count) obs_count = id + 1;
                }
            }
            while (read(fd_tar_in, &msg_in, sizeof(Message)) > 0) {
                 if (msg_in.target.id < MAX_TARGETS) targets[msg_in.target.id] = msg_in.target;
            }
        } 
        else {
            // NETWORK LOGIC
            net_tick++;
            if (net_tick >= NET_RATE) {
                net_tick = 0;
                if (network_exchange(mode, sockfd, &drone, &opponent) == 0) {
                    obs_count = 1;
                    obstacles[0] = opponent; 
                    obstacles[0].id = 0;
                } else {
                    // [FIX] IF NETWORK FAILS, STOP THE LOOP.
                    // This stops the "Broken pipe" spam.
                    log_message(SYSTEM_LOG_FILE, "Blackboard", "Connection lost.");
                    running = 0; 
                }
            }
        }

        // C. Broadcast State
        msg_out.type = MSG_DRONE_STATE;
        msg_out.drone = drone;
        write(fd_ui_input_out, &msg_out, sizeof(Message));
        write(fd_ui_out, &msg_out, sizeof(Message));

        msg_out.type = MSG_OBSTACLE;
        int scan_limit = (mode == MODE_STANDALONE) ? MAX_OBSTACLES : 1;
        
        for (int i = 0; i < scan_limit; i++) {
            if (obstacles[i].id != -1) {
                msg_out.obstacle = obstacles[i];
                write(fd_ui_out, &msg_out, sizeof(Message));
                write(fd_dyn_out, &msg_out, sizeof(Message));
            }
        }

        if (mode == MODE_STANDALONE) {
            msg_out.type = MSG_TARGET;
            for (int i = 0; i < MAX_TARGETS; i++) {
                msg_out.target = targets[i];
                write(fd_ui_out, &msg_out, sizeof(Message));  
                write(fd_dyn_out, &msg_out, sizeof(Message)); 
            }
            usleep(10000); 
        } else {
            // [FIX] Sleep in Multiplayer too (100Hz)
            // This prevents CPU 100% usage while keeping physics smooth
            usleep(10000);
        }
    }

    if (sockfd != -1) close_network(sockfd);
    close(fd_ui_in); close(fd_dyn_in);
    if (mode == MODE_STANDALONE) { close(fd_obs_in); close(fd_tar_in); }
    close(fd_ui_out); close(fd_ui_input_out); close(fd_dyn_out);
    
    log_message(SYSTEM_LOG_FILE, "Blackboard", "Terminating...");
}