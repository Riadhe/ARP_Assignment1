#ifndef COMMON_H
#define COMMON_H

// 1. Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
// NEW: Required for Locking and Logging
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
// 2. PIPES 
#define PIPE_DIR "/tmp/"

// Input Window -> Server
#define PIPE_UI_TO_SERVER       "/tmp/fifo_ui_to_server"

// Server : Input Window (To show speed/position data)
#define PIPE_SERVER_TO_UI_INPUT "/tmp/fifo_server_to_ui_input"

// Server : Map Window (To draw the drone)
#define PIPE_SERVER_TO_MAP      "/tmp/fifo_server_to_map"

// Server : Dynamics (To send key presses)
#define PIPE_SERVER_TO_DYN      "/tmp/fifo_server_to_dyn"

// Dynamics : Server (To send calculated position)
#define PIPE_DYN_TO_SERVER      "/tmp/fifo_dyn_to_server"

// Obstacles : Server (To send new walls)
#define PIPE_OBS_TO_SERVER      "/tmp/fifo_obs_to_server"

// Targets : Server (To send new goals)
#define PIPE_TAR_TO_SERVER      "/tmp/fifo_tar_to_server"

// 2. CONSTANTS (simulation parameters)

#define MAP_WIDTH   100  // World size in meters
#define MAP_HEIGHT  100

// Physics Constants (Defaults, can be overridden by params.txt)
#define F_STEP      2.0f    // Force applied per key press (Newtons)
#define REPULSION_RHO   10.0f   // Radius of influence for Obstacles (meters)
#define REPULSION_ETA   500.0f  // Strength of repulsive push
#define ATTRACTION_RHO  20.0f   // Radius of influence for Targets
#define ATTRACTION_ETA  2.0f    // Strength of attractive pull

// Timing (Microseconds)
// 20000us = 20ms = 50 FPS 
#define UI_REFRESH_RATE 20000  
// 2000us = 2ms = 500 Physics Steps Per Second (High precision math)
#define DYNAMICS_RATE   2000   
// NEW: Update rate for dynamic obstacles/targets (e.g., 50ms = 20Hz)
#define GENERATOR_RATE  50000 


// Game Limits
#define MAX_OBSTACLES 30
#define MAX_TARGETS   9


// 2. Assignment 2 Constants (NEW)
#define PROCESS_LIST_FILE "process_list.txt"
#define SYSTEM_LOG_FILE   "system.log"
#define WATCHDOG_LOG_FILE "watchdog.log"

// 3. MESSAGE TYPES (The Topics)
typedef enum {
    MSG_DRONE_STATE,    // "I am at position X,Y"
    MSG_FORCE_UPDATE,   // "Player pressed a key"
    MSG_OBSTACLE,       // "A new obstacle appeared"
    MSG_TARGET,         // "A new target appeared"
    MSG_STOP,           // "Emergency Stop / Quit Game"
} MessageType;

// 4. DATA STRUCTURES (The Objects)

// 2D Vector for Position, Velocity, Force
typedef struct { 
    float x; 
    float y; 
} Vec2;

// The complete status of the drone
typedef struct {
    Vec2 position;      
    Vec2 velocity;      
    Vec2 force;         
} DroneState;

// A static wall/dot
typedef struct { 
    int id; 
    Vec2 position; 
} Obstacle;

// A goal to collect
typedef struct { 
    int id; 
    Vec2 position; 
    int active;         // 1 = Visible, 0 = Collected/Invisible
} Target;



// 5. THE MESSAGE ENVELOPE
// This struct is what actually travels through the pipes.
typedef struct {
    MessageType type;   // Tells the receiver what data to look at
    int sender_pid;     // Process ID of who sent it

    // The Payload (Only one is used at a time)
    DroneState drone;
    Obstacle obstacle;
    Target target;
    
    char info[64];      // For debug text messages
} Message;


// 6. Function Prototypes (NEW) 
// These allow all your processes to use the tools in utilities.c

/**
 * Handles file locking (The "Key" logic)
 */
int file_lock(int fd, int cmd, int type);

/**
 * Writes to a log file safely (Open -> Lock -> Write -> Unlock -> Close)
 */
void log_message(const char *filename, const char *process_name, const char *fmt, ...);

/**
 * Saves the process PID to process_list.txt so the Watchdog can find it
 */
void register_process(const char *process_name);
#endif