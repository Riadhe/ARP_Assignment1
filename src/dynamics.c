#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h> 
#include <errno.h> 
#include "common.h"
#include "params.h"

// State Memory
static DroneState drone;
static Obstacle obstacles[MAX_OBSTACLES]; 
static Target targets[MAX_TARGETS]; 
static int obs_count = 0;
//Game logic: which target is next to collect
static int next_target_needed = 0;

//Pipes
static int fd_server_to_dyn;
static int fd_dyn_to_server;

// Physics Parameters(loaded from config/params.txt)
float M;
float K;
float T = DYNAMICS_RATE / 1000000.0; 

// Algorithm 1 : Repulsion field
// Khatib's Method: Obstacles exert a repulsive force 
// inversely proportional to distance (1/d^2).
Vec2 calculate_repulsion() {
    Vec2 f_rep = {0.0, 0.0};
    for (int i = 0; i < obs_count; i++) {
        float dx = drone.position.x - obstacles[i].position.x;
        float dy = drone.position.y - obstacles[i].position.y;
        float dist = sqrt(dx*dx + dy*dy);
        
        if (dist < REPULSION_RHO && dist > 0.1) { 
            float mag = REPULSION_ETA * (1.0/dist - 1.0/REPULSION_RHO) * (1.0/(dist*dist));
            f_rep.x += mag * (dx / dist);
            f_rep.y += mag * (dy / dist);
        }//Also avoid division by zero
    }
    return f_rep;
}

// Algorithm 2 : Attraction field
// Targets exert an attractive force pulling the drone towards them.
Vec2 calculate_attraction() {
    Vec2 f_att = {0.0, 0.0};
    // Pull towards the current needed target IF it is active
    if (next_target_needed < MAX_TARGETS && targets[next_target_needed].active == 1) {
        float dx = targets[next_target_needed].position.x - drone.position.x;
        float dy = targets[next_target_needed].position.y - drone.position.y;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist < ATTRACTION_RHO) {
            f_att.x = ATTRACTION_ETA * dx; 
            f_att.y = ATTRACTION_ETA * dy;
        }
    }
    return f_att;
}

void check_collisions() {
    for (int i = 0; i < MAX_TARGETS; i++) {
        // Only check active targets
        if (targets[i].active == 1) {
            float dx = drone.position.x - targets[i].position.x;
            float dy = drone.position.y - targets[i].position.y;
            float dist = sqrt(dx*dx + dy*dy);

            if (dist < 2.0) {
                // Check Sequence
                if (targets[i].id == next_target_needed) {
                    
                    // 1. DISAPPEAR (Set inactive)
                    targets[i].active = 0;
                    
                    // Notify Server (to hide it on Map)
                    Message msg;
                    msg.type = MSG_TARGET;
                    msg.target = targets[i]; 
                    msg.sender_pid = getpid();
                    write(fd_dyn_to_server, &msg, sizeof(Message));

                    // 2. Advance Sequence
                    next_target_needed++;

                    // 3. LEVEL CLEAR CHECK
                    // If we collected the last target (target 8, which is number 9)
                    if (next_target_needed >= MAX_TARGETS) {
                        
                        // Reset Sequence
                        next_target_needed = 0;
                        
                        // Respawn ALL targets
                        for (int j = 0; j < MAX_TARGETS; j++) {
                            targets[j].position.x = 5 + rand() % (MAP_WIDTH - 10);
                            targets[j].position.y = 5 + rand() % (MAP_HEIGHT - 10);
                            targets[j].active = 1; // Make visible again
                            
                            // Send update for EACH target
                            msg.target = targets[j];
                            write(fd_dyn_to_server, &msg, sizeof(Message));
                        }
                    }
                }
            }
        }
    }
}

// Update the drone's physics state
//// Solves: F = ma + kv
void update_physics() {
    Vec2 f_rep = calculate_repulsion();
    Vec2 f_att = calculate_attraction();
    //1.calculate Acceleration
    float total_fx = drone.force.x + f_rep.x + f_att.x;
    float total_fy = drone.force.y + f_rep.y + f_att.y;
    //Update Velocity and Position
    float ax = (total_fx - K * drone.velocity.x) / M;
    float ay = (total_fy - K * drone.velocity.y) / M;
    //Update velocity and position using Euler integration
    drone.velocity.x += ax * T;
    drone.velocity.y += ay * T;
    drone.position.x += drone.velocity.x * T;
    drone.position.y += drone.velocity.y * T;
    //Boundary conditions
    if (drone.position.x <= 1) { drone.position.x = 1; drone.velocity.x = -0.5*drone.velocity.x; }
    if (drone.position.x >= MAP_WIDTH-1) { drone.position.x = MAP_WIDTH-1; drone.velocity.x = -0.5*drone.velocity.x; }
    if (drone.position.y <= 1) { drone.position.y = 1; drone.velocity.y = -0.5*drone.velocity.y; }
    if (drone.position.y >= MAP_HEIGHT-1) { drone.position.y = MAP_HEIGHT-1; drone.velocity.y = -0.5*drone.velocity.y; }
}

void send_state() {
    Message msg;
    msg.type = MSG_DRONE_STATE;
    msg.sender_pid = getpid();
    msg.drone = drone;
    //Send updated state to server
    if (write(fd_dyn_to_server, &msg, sizeof(Message)) < 0) {}
}

void run_dynamics() {
    srand(time(NULL) + getpid());
    // Load parameters from config file
    M = load_param("config/params.txt", "M");
    K = load_param("config/params.txt", "K");
    //Wait for pipes to be available
    while ((fd_server_to_dyn = open(PIPE_SERVER_TO_DYN, O_RDONLY | O_NONBLOCK)) < 0) usleep(100000);
    while ((fd_dyn_to_server = open(PIPE_DYN_TO_SERVER, O_WRONLY | O_NONBLOCK)) < 0) usleep(100000);

    for(int i=0; i<MAX_TARGETS; i++) targets[i].active = 0;

    drone.position.x = MAP_WIDTH / 2;
    drone.position.y = MAP_HEIGHT / 2;
    drone.velocity.x = 0; drone.velocity.y = 0;
    drone.force.x = 0;    drone.force.y = 0;

    Message msg;
    while (1) {
        //Read all incoming commands
        while (read(fd_server_to_dyn, &msg, sizeof(Message)) > 0) {
            if (msg.type == MSG_FORCE_UPDATE) {
                drone.force = msg.drone.force;
            } 
            else if (msg.type == MSG_OBSTACLE) {
                if (obs_count < MAX_OBSTACLES) obstacles[obs_count++] = msg.obstacle;
            }
            else if (msg.type == MSG_TARGET) {
                if (msg.target.id < MAX_TARGETS) targets[msg.target.id] = msg.target;
            }
            else if (msg.type == MSG_STOP) exit(0);
        }
        //Run physics step
        update_physics();
        check_collisions();
        send_state();
        usleep(DYNAMICS_RATE);
    }
    close(fd_server_to_dyn); close(fd_dyn_to_server);
}