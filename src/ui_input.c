#define _XOPEN_SOURCE_EXTENDED
#include <locale.h>
#include <ncurses.h>
#include <fcntl.h>      
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "params.h" 

const char *keys[3][3] = {{"Z", "E", "R"}, {"S", "D", "F"}, {"X", "C", "V"}};
// Commanded force values
float cmd_x = 0.0f;
float cmd_y = 0.0f;
float force_step;
DroneState drone_display;

//  User interface drawing functions 
void draw_input_win(WINDOW *win) {
    werase(win);
    box(win, 0, 0);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 0, 2, " Input Display "); 
    wattroff(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 2, 2, "Press Keys to Accelerate Drone.");
   

    int box_w = 7; int box_h = 5;  
    int h, w;
    getmaxyx(win, h, w);
    int start_y = (h / 2) - ((3 * box_h) / 2); 
    int start_x = (w / 2) - ((3 * box_w) / 2);
    
    // Safety check for small windows
    if (start_y < 2) start_y = 2; 
    if (start_x < 2) start_x = 2;

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int y = start_y + (r * box_h); 
            int x = start_x + (c * box_w);
            if (y + box_h < h && x + box_w < w) {
                mvwprintw(win, y,     x, "+-----+"); 
                mvwprintw(win, y + 1, x, "|     |"); 
                mvwprintw(win, y + 2, x, "|  %s  |", keys[r][c]);
                mvwprintw(win, y + 3, x, "|     |"); 
                mvwprintw(win, y + 4, x, "+-----+"); 
            }
        }
    }
    mvwprintw(win, h - 4, 2, "Tap ESC to close windows.");
    wnoutrefresh(win);
}
// Display the current drone state and last command
void draw_output_win(WINDOW *win, char *last_cmd) {
    werase(win);
    box(win, 0, 0);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 0, 2, " Dynamics Display ");
    wattroff(win, COLOR_PAIR(1) | A_BOLD);

    int y = 2;
    mvwprintw(win, y++, 2, "Command: %s", last_cmd);
    y++; 
    wattron(win, A_BOLD);
    mvwprintw(win, y++, 2, "*** Drone State ***");
    wattroff(win, A_BOLD);
    mvwprintw(win, y++, 2, "Pos X:   %.2f", drone_display.position.x);
    mvwprintw(win, y++, 2, "Pos Y:   %.2f", drone_display.position.y);
    y++;
    mvwprintw(win, y++, 2, "Vel X:   %.2f", drone_display.velocity.x);
    mvwprintw(win, y++, 2, "Vel Y:   %.2f", drone_display.velocity.y);
    y++;
    mvwprintw(win, y++, 2, "Force X: %.2f", drone_display.force.x);
    mvwprintw(win, y++, 2, "Force Y: %.2f", drone_display.force.y);
    wnoutrefresh(win);
}

// Manages the split-screen layout
void update_layout(WINDOW *left, WINDOW *right, float ratio, char *log_msg) {
    int H, W;
    getmaxyx(stdscr, H, W);
    int gap = 1;
    int w_total = W - 2; 
    int w_left  = (int)(w_total * ratio);
    int w_right = w_total - w_left - gap;

    erase(); wnoutrefresh(stdscr);

    int win_h = (H > 2) ? H - 2 : 1;
    wresize(left,  win_h, w_left);
    mvwin(left,    1,     1);
    wresize(right, win_h, w_right);
    mvwin(right,   1,     1 + w_left + gap);

    draw_input_win(left);
    draw_output_win(right, log_msg);
    doupdate(); 
}

int main(void) {
    setlocale(LC_ALL, ""); 
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    
    // Set timeout(0) for Non-Blocking input reading.
    // This allows the loop to run fast even if no key is pressed.
    timeout(0); 
    
    if (has_colors()) {
        start_color(); use_default_colors();
        init_pair(1, COLOR_YELLOW, -1); 
    }

    force_step = load_param("config/params.txt", "F_STEP");
    if (force_step <= 0) force_step = 2.0f;

    // Wait for pipes to be available
    int fd_out;
    while ((fd_out = open(PIPE_UI_TO_SERVER, O_WRONLY)) < 0) usleep(100000);
    int fd_in;
    while ((fd_in = open(PIPE_SERVER_TO_UI_INPUT, O_RDONLY | O_NONBLOCK)) < 0) usleep(100000);

    WINDOW *left_win = newwin(1, 1, 0, 0);
    WINDOW *right_win = newwin(1, 1, 0, 0);
    float split_ratio = 0.55f; 
    char log_msg[32] = "Ready";
    // Initial layout
    update_layout(left_win, right_win, split_ratio, log_msg);
    
    int ch;
    Message msg_out; msg_out.sender_pid = getpid();
    Message msg_in;
    int running = 1;
    // Main Loop
    while (running) {
        int input_processed = 0;
        msg_out.type = MSG_FORCE_UPDATE;

        // 1. READ Telemetry 
        while (read(fd_in, &msg_in, sizeof(Message)) > 0) {
            if (msg_in.type == MSG_DRONE_STATE) drone_display = msg_in.drone;
            else if (msg_in.type == MSG_STOP) running = 0;
        }

        // 2. READ Keys 
        // We read ALL keys waiting in the buffer to prevent lag
        while ((ch = getch()) != ERR) {
            if (ch == KEY_RESIZE) {
                resize_term(0, 0); 
                update_layout(left_win, right_win, split_ratio, log_msg);
            }
            else if (ch == 27) { 
                msg_out.type = MSG_STOP; 
                input_processed = 1;
                running = 0;
            } 
            else {
                input_processed = 1;
                switch(ch) {
                    case 'e': case 'E': cmd_y -= force_step; strcpy(log_msg, " UP"); break;
                    case 'c': case 'C': cmd_y += force_step; strcpy(log_msg, " DOWN"); break;
                    case 's': case 'S': cmd_x -= force_step; strcpy(log_msg, " LEFT"); break;
                    case 'f': case 'F': cmd_x += force_step; strcpy(log_msg, " RIGHT"); break;
                    case 'd': case 'D': cmd_x = 0.0; cmd_y = 0.0; strcpy(log_msg, "BRAKE"); break;
                    
                    case 'r': case 'R': cmd_y -= force_step; cmd_x += force_step; strcpy(log_msg, " UpRight"); break;
                    case 'z': case 'Z': cmd_y -= force_step; cmd_x -= force_step; strcpy(log_msg, " UpLeft"); break;
                    case 'x': case 'X': cmd_y += force_step; cmd_x -= force_step; strcpy(log_msg, " DownLleft"); break;
                    case 'v': case 'V': cmd_y += force_step; cmd_x += force_step; strcpy(log_msg, " DownRight"); break;
                }
            }
        }

        // 3. SEND Update
        if (input_processed) {
            msg_out.drone.force.x = cmd_x;
            msg_out.drone.force.y = cmd_y;
            write(fd_out, &msg_out, sizeof(Message));
            if (msg_out.type == MSG_STOP) break;
        }
        
        draw_output_win(right_win, log_msg);
        doupdate();
        
        // Smart Sleep: Don't sleep if user is active (low latency)
        if (!input_processed) usleep(UI_REFRESH_RATE); 
        else usleep(1000); 
    }

    close(fd_out); close(fd_in);
    delwin(left_win); delwin(right_win);
    endwin();
    return 0;
}