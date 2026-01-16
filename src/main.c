#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h> 
#include "common.h"

// Signal Handler
void handle_sigint(int sig) {
    log_message(SYSTEM_LOG_FILE, "Main", "Received SIGINT. Shutting down system...");
    printf("\n[Main] Shutdown complete. See system.log for details.\n");
    // Clean up pipes on exit
    unlink(PIPE_UI_TO_SERVER);
    unlink(PIPE_SERVER_TO_UI_INPUT);
    unlink(PIPE_SERVER_TO_MAP);
    unlink(PIPE_SERVER_TO_DYN);
    unlink(PIPE_DYN_TO_SERVER);
    unlink(PIPE_OBS_TO_SERVER);
    unlink(PIPE_TAR_TO_SERVER);
    exit(0);
}

// Spawn terminal helper
void spawn_terminal(const char* program_path) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("konsole", "konsole", "-e", program_path, NULL);
        execlp("gnome-terminal", "gnome-terminal", "--", program_path, NULL);
        execlp("xterm", "xterm", "-e", program_path, NULL);
        perror("[Main] Error: Could not launch a new terminal window");
        exit(1);
    }
}

// Helper to create all named pipes
void create_named_pipes() {
    mode_t mode = 0666;
    // Unlink old pipes (in case of crash) and Create new ones
    unlink(PIPE_UI_TO_SERVER);       if (mkfifo(PIPE_UI_TO_SERVER, mode) == -1) perror("mkfifo UI->Server");
    unlink(PIPE_SERVER_TO_UI_INPUT); if (mkfifo(PIPE_SERVER_TO_UI_INPUT, mode) == -1) perror("mkfifo Server->Input");
    unlink(PIPE_SERVER_TO_MAP);      if (mkfifo(PIPE_SERVER_TO_MAP, mode) == -1) perror("mkfifo Server->Map");
    unlink(PIPE_SERVER_TO_DYN);      if (mkfifo(PIPE_SERVER_TO_DYN, mode) == -1) perror("mkfifo Server->Dyn");
    unlink(PIPE_DYN_TO_SERVER);      if (mkfifo(PIPE_DYN_TO_SERVER, mode) == -1) perror("mkfifo Dyn->Server");
    unlink(PIPE_OBS_TO_SERVER);      if (mkfifo(PIPE_OBS_TO_SERVER, mode) == -1) perror("mkfifo Obs->Server");
    unlink(PIPE_TAR_TO_SERVER);      if (mkfifo(PIPE_TAR_TO_SERVER, mode) == -1) perror("mkfifo Tar->Server");
    printf("[Main] All Named Pipes (FIFOs) created in /tmp/.\n");
}

void run_blackboard(int mode); 
void run_dynamics();   
void run_obstacles(); 
void run_targets();   

int main() {
    signal(SIGINT, handle_sigint);

    signal(SIGPIPE, SIG_IGN);

    // STEP 1: SELECT MODE 
    int mode = 0; 
    printf("\n=== DRONE SIMULATOR - ASSIGNMENT 3 ===\n");
    printf("Select Operation Mode:\n");
    printf("1. Standalone (Assignment 2)\n");
    printf("2. Server (Host Multiplayer)\n");
    printf("3. Client (Join Multiplayer)\n");
    printf("Enter choice (1-3): ");
    
    int choice;
    if (scanf("%d", &choice) == 1) {
        if (choice == 1) mode = MODE_STANDALONE;
        else if (choice == 2) mode = MODE_SERVER;
        else if (choice == 3) mode = MODE_CLIENT;
    }
    // This prevents the "Enter Server IP" step from being skipped!
    while (getchar() != '\n'); 

    // Cleanup
    remove(PROCESS_LIST_FILE);
    remove(WATCHDOG_LOG_FILE);
    remove(SYSTEM_LOG_FILE);

    printf("[Main] Starting system in mode: %d...\n", mode);
    register_process("Main_Manager");
    log_message(SYSTEM_LOG_FILE, "Main", "System starting in mode %d...", mode);

    // STEP 2: CREATE PIPES  
    create_named_pipes();

    // LAUNCH PROCESSES 
    
    // 1. Blackboard Server
    if (fork() == 0) { 
        signal(SIGINT, SIG_DFL); 
        run_blackboard(mode); 
        exit(0); 
    } 

    // 2. Dynamics
    if (fork() == 0) { signal(SIGINT, SIG_DFL); run_dynamics(); exit(0); }

    // 3. Generators & Watchdog (Only Standalone)
    if (mode == MODE_STANDALONE) {
        if (fork() == 0) { signal(SIGINT, SIG_DFL); run_obstacles(); exit(0); }
        if (fork() == 0) { signal(SIGINT, SIG_DFL); run_targets(); exit(0); }
        printf("[Main] Launching Watchdog...\n");
        spawn_terminal("./watchdog");
    } else {
        printf("[Main] Multiplayer Mode: Generators and Watchdog DISABLED.\n");
    }

    // 4. UI WINDOWS
    printf("[Main] Launching Map Window...\n");
    spawn_terminal("./map"); 
    
    printf("[Main] Launching Input Window...\n");
    spawn_terminal("./input");

    printf("[Main] System Running. Press Ctrl+C to stop.\n");

    while (1) {
        sleep(10);
    }
    return 0;
}