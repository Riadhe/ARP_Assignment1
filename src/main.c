#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "common.h"


int pipe_ui_to_server[2];
int pipe_dyn_to_server[2];
int pipe_obs_to_server[2];
int pipe_tar_to_server[2];
int pipe_server_to_ui[2];
int pipe_server_to_dyn[2];


// Spawn terminal : This function launches a NEW separate konsole's window 
// and runs a specific program inside it.
void spawn_terminal(const char* program_path) {
    pid_t pid = fork(); // Clone myself
    
    if (pid == 0) {
        // I AM THE CHILD PROCESS
        // Replace my code with a Terminal Emulator program.
        // We try 3 common Linux terminals: Konsole, Gnome, Xterm.
        execlp("konsole", "konsole", "-e", program_path, NULL);
        execlp("gnome-terminal", "gnome-terminal", "--", program_path, NULL);
        execlp("xterm", "xterm", "-e", program_path, NULL);
        
        // If we get here, no terminal was found!
        perror("[Main] Error: Could not launch a new terminal window");
        exit(1);
    }
    // I AM THE PARENT PROCESS
    // I return immediately to launch the next thing.
}

// Forward declarations tell the compiler these functions exist elsewhere
void run_blackboard(); 
void run_dynamics();   
void run_obstacles(); 
void run_targets();   

int main() {
    printf("[Main] Starting system...\n");

    // 1. Initialize Pipes (Create the resources)
    if (pipe(pipe_ui_to_server) == -1) perror("pipe");
 
    // IMPORTANT: Make pipes Non-Blocking.
    // This prevents the system from freezing if one pipe is empty.
    fcntl(pipe_ui_to_server[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_dyn_to_server[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_obs_to_server[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_tar_to_server[0], F_SETFL, O_NONBLOCK);

    printf("[Main] Pipes created.\n");

    // 2. Launch Background Processes 
    // fork() creates a copy of the program.
    if (fork() == 0) { run_blackboard(); exit(0); } // Server
    if (fork() == 0) { run_dynamics(); exit(0); }   // Physics
    if (fork() == 0) { run_obstacles(); exit(0); }  // Walls
    if (fork() == 0) { run_targets(); exit(0); }    // Goals

    // 3. Spawn External Windows (The Display & Input)
    // These open in new windows on  desktop.
    printf("[Main] Launching Map Window...\n");
    spawn_terminal("./map"); 
    
    printf("[Main] Launching Input Window...\n");
    spawn_terminal("./input");

    printf("[Main] System Running. Press Ctrl+C to stop.\n");

    // 4. Keep Parent Alive
    // If main() exits, the OS might kill all child processes.
    while (1) {
        sleep(10);
    }
    return 0;
}