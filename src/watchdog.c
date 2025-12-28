#include "common.h"
#include <ncurses.h>

#define PROCESS_CHECK_INTERVAL 2 // Seconds between checks
#define BOX_WIDTH 62             
#define BOX_HEIGHT 20            

int main() {
    // 1. Setup Logging & UI
    register_process("Watchdog"); 
    log_message(WATCHDOG_LOG_FILE, "Watchdog", "Started monitoring...");

    initscr(); // Start ncurses mode
    cbreak(); // Disable line buffering
    noecho(); // Don't echo input
    curs_set(0); // Hide cursor
    timeout(PROCESS_CHECK_INTERVAL * 1000); // Set getch timeout


    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_YELLOW, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
    }

    int h, w;
    getmaxyx(stdscr, h, w); // Get window size

    while (1) {
        clear();
        getmaxyx(stdscr, h, w); // Update size in case of resize

        // Calculate Center Coordinates
        int start_y = (h - BOX_HEIGHT) / 2;
        int start_x = (w - BOX_WIDTH) / 2;
        
        // Safety check: if window is too small, stick to top-left (0,0)
        if (start_y < 0) start_y = 0;
        if (start_x < 0) start_x = 0;
        
        //  DRAW UI FRAME 
        attron(A_NORMAL); 
        
        // Horizontal lines
        mvhline(start_y, start_x, ACS_HLINE, BOX_WIDTH);                
        mvhline(start_y + BOX_HEIGHT - 1, start_x, ACS_HLINE, BOX_WIDTH); 
        // Vertical lines
        mvvline(start_y, start_x, ACS_VLINE, BOX_HEIGHT);               
        mvvline(start_y, start_x + BOX_WIDTH - 1, ACS_VLINE, BOX_HEIGHT); 
        // Corners
        mvaddch(start_y, start_x, ACS_ULCORNER);
        mvaddch(start_y, start_x + BOX_WIDTH - 1, ACS_URCORNER);
        mvaddch(start_y + BOX_HEIGHT - 1, start_x, ACS_LLCORNER);
        mvaddch(start_y + BOX_HEIGHT - 1, start_x + BOX_WIDTH - 1, ACS_LRCORNER);
        attroff(A_NORMAL);
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(start_y, start_x + (BOX_WIDTH - 25) / 2, " Watchdog Process Monitor "); 
        // Table Header
        mvprintw(start_y + 2, start_x + 2, " %-20s | %-10s | %-15s ", "PROCESS NAME", "PID", "STATUS");
        mvhline(start_y + 3, start_x + 2, ACS_HLINE, BOX_WIDTH - 4); // Inner separator
        attroff(A_BOLD | COLOR_PAIR(1));
        mvprintw(start_y + BOX_HEIGHT - 2, start_x + 2, "Press 'q' to Quit | Logs: ./watchdog.log");

        // READ process_list.txt and CHECK each process
        FILE *fp = fopen(PROCESS_LIST_FILE, "r");
        if (!fp) {
            attron(A_BLINK | A_BOLD | COLOR_PAIR(3)); // Red Blink
            mvprintw(start_y + 5, start_x + 2, "Error: process_list.txt not found!"); 
            attroff(A_BLINK | A_BOLD | COLOR_PAIR(3));
            refresh(); // Update screen
            sleep(1); // Wait before retrying
            continue;
        }

        int fd = fileno(fp);
        file_lock(fd, F_SETLKW, F_RDLCK);

        char line[64];
        int row_offset = 5; // Start printing 5 lines down from box top
        
        while (fgets(line, sizeof(line), fp)) {
            char name[32];
            pid_t pid;
            line[strcspn(line, "\n")] = 0;

            if (sscanf(line, "%s %d", name, &pid) == 2) {
                int current_row = start_y + row_offset;

                // Send Signal 0 to check health
                if (kill(pid, 0) == 0) {
                    mvprintw(current_row, start_x + 2, " %-20s | %-10d | ", name, pid);
                    // Green for alive
                    attron(A_BOLD | COLOR_PAIR(2));
                    printw("ALIVE");
                    attroff(A_BOLD | COLOR_PAIR(2));
                    
                    log_message(WATCHDOG_LOG_FILE, "Watchdog", "%s (PID %d) is running", name, pid);
                } else {
                    mvprintw(current_row, start_x + 2, " %-20s | %-10d | ", name, pid);
                     // Red Blink for unresponsive
                    attron(A_BOLD | A_BLINK | COLOR_PAIR(3));
                    printw("NOT RESPONDING!");
                    attroff(A_BOLD | A_BLINK | COLOR_PAIR(3));
                    
                    log_message(SYSTEM_LOG_FILE, "Watchdog", "ALERT: %s (PID %d) is not responding!", name, pid);
                }
                row_offset++;
                // Stop if we run out of box height
                if (row_offset >= BOX_HEIGHT - 2) break; 
            }
        }

        file_lock(fd, F_SETLKW, F_UNLCK); // Unlock
        fclose(fp);// Close file

        refresh();

        // Handle Input
        int ch = getch();
        if (ch == 'q' || ch == 'Q') { // 'q' or ESC to quit
            break;
        }
    }

    log_message(WATCHDOG_LOG_FILE, "Watchdog", "Terminating..."); // Log shutdown
    endwin(); // End ncurses mode
    return 0; // Exit program
}