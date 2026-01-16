#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <locale.h> 
#include "common.h"

// State
DroneState drone;
Obstacle obstacles[MAX_OBSTACLES];
Target targets[MAX_TARGETS];
int score = 0; 

// Track screen size for scaling
int inner_h, inner_w;
int screen_h = 0, screen_w = 0; 

// LAYOUT MANAGER 
void layout_and_draw(WINDOW *win) {
    int H, W;
    getmaxyx(stdscr, H, W);
    screen_h = H; screen_w = W;

    // Margins logic
    int wh = (H > 6) ? H - 6 : H;
    int ww = (W > 10) ? W - 10 : W;
    
    // Safety check
    if (wh < 3) wh = 3; 
    if (ww < 3) ww = 3;

    wresize(win, wh, ww);
    mvwin(win, (H - wh) / 2, (W - ww) / 2);
    werase(stdscr); refresh(); 
    inner_h = wh - 2; inner_w = ww - 2;
}

// DRAWING LOGIC 
void draw_game_entities(WINDOW *win) {
    werase(win); 

    // Draw Box & Header
    wattron(win, COLOR_PAIR(4)); box(win, 0, 0); wattroff(win, COLOR_PAIR(4));
    wattron(win, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(win, 0, 2, " MAP DISPLAY | Score = %d ", score);
    wattroff(win, COLOR_PAIR(3) | A_BOLD);

    // Calculate Scale: Screen Pixels per Meter
    float scale_y = (float)inner_h / MAP_HEIGHT;
    float scale_x = (float)inner_w / MAP_WIDTH;

    // 1. Draw Targets (Green Numbers)
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    for(int i=0; i<MAX_TARGETS; i++) {
        // [HYBRID FIX] Check ID validity + active flag
        if (targets[i].id != -1 && targets[i].active == 1) {
            int r = 1 + (int)(targets[i].position.y * scale_y);
            int c = 1 + (int)(targets[i].position.x * scale_x);
            if (r > 0 && r <= inner_h && c > 0 && c <= inner_w)
                mvwaddch(win, r, c, '1' + i); // '1' + ID
        }
    }
    wattroff(win, COLOR_PAIR(2) | A_BOLD);

    // 2. Draw Obstacles (Yellow)
    wattron(win, COLOR_PAIR(3));
    // [HYBRID FIX] Iterate ALL slots instead of relying on obs_count
    // This works for both Mode 2 (ID 0 only) and Mode 1 (IDs 0-29)
    for(int i=0; i<MAX_OBSTACLES; i++) {
        if (obstacles[i].id != -1) {
            int r = 1 + (int)(obstacles[i].position.y * scale_y);
            int c = 1 + (int)(obstacles[i].position.x * scale_x);
            if (r > 0 && r <= inner_h && c > 0 && c <= inner_w)
                mvwaddch(win, r, c, 'O'); // Using 'O' for visibility
        }
    }
    wattroff(win, COLOR_PAIR(3));

    // 3. Draw Drone (Blue +)
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    int dr = 1 + (int)(drone.position.y * scale_y);
    int dc = 1 + (int)(drone.position.x * scale_x);
    
    // Clamp to window
    if (dr < 1) dr = 1; 
    if (dr > inner_h) dr = inner_h;
    if (dc < 1) dc = 1; 
    if (dc > inner_w) dc = inner_w;
    
    mvwaddch(win, dr, dc, '+');
    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    wrefresh(win);
}

int main(int argc, char *argv[]) {
    register_process("UI_Map"); 
    log_message(SYSTEM_LOG_FILE, "UI_Map", "Map UI process started."); 

    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0); 
    nodelay(stdscr, TRUE);
    
    start_color(); 
    use_default_colors();
    init_pair(1, COLOR_BLUE, -1);   
    init_pair(2, COLOR_GREEN, -1);  
    init_pair(3, COLOR_YELLOW, -1); 
    init_pair(4, COLOR_WHITE, -1);  
    
    int fd_in;
    while ((fd_in = open(PIPE_SERVER_TO_MAP, O_RDONLY | O_NONBLOCK)) < 0) usleep(100000);

    WINDOW *field = newwin(3, 3, 0, 0); 
    layout_and_draw(field); 
    
    // Initialize state arrays to "Empty" (-1)
    // This allows us to detect valid updates in ANY mode
    for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].id = -1;
    for(int i=0; i<MAX_TARGETS; i++) { targets[i].id = -1; targets[i].active = 0; }

    Message msg;
    int running = 1;

    // Main Loop
    while (running) {
        int ch = getch();
        
        // Auto-Resize check
        int cur_h, cur_w;
        getmaxyx(stdscr, cur_h, cur_w);
        if (ch == KEY_RESIZE || cur_h != screen_h || cur_w != screen_w) {
            resize_term(0, 0); layout_and_draw(field); 
        }

        // Drain pipe buffer
        while (read(fd_in, &msg, sizeof(Message)) > 0) {
            switch(msg.type) {
                case MSG_DRONE_STATE: 
                    drone = msg.drone; 
                    break;
                case MSG_OBSTACLE:
                    // [CRITICAL FIX] Update specific slot by ID
                    // This works for Multiplayer (ID 0 updates repeatedly)
                    // AND Standalone (IDs 0-29 update independently)
                    if (msg.obstacle.id >= 0 && msg.obstacle.id < MAX_OBSTACLES) {
                        obstacles[msg.obstacle.id] = msg.obstacle;
                    }
                    break;
                case MSG_TARGET:
                    if (msg.target.id >= 0 && msg.target.id < MAX_TARGETS) {
                        if (targets[msg.target.id].active == 1 && msg.target.active == 0) score++;
                        targets[msg.target.id] = msg.target;
                    }
                    break;
                case MSG_STOP: 
                    running = 0; 
                    break;
                default: break;
            }
        }
        
        draw_game_entities(field);
        usleep(UI_REFRESH_RATE);
    }
    
    close(fd_in); delwin(field); endwin();
    return 0;
}