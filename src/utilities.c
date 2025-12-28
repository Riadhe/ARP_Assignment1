#include "common.h"

// THE LOCKING FUNCTION
int file_lock(int fd, int cmd, int type) {
    struct flock lock;
    // Setup the lock structure
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0; // From beginning
    lock.l_len = 0;  // Lock whole file
    lock.l_pid = getpid();

    if (fcntl(fd, cmd, &lock) == -1) {
    
        perror("fcntl lock failed");
        return -1;
    }
    return 0;
}

// THE LOGGING FUNCTION
void log_message(const char *filename, const char *process_name, const char *fmt, ...) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        perror("Failed to open log file");
        return;
    }

    // Lock (Wait)
    if (file_lock(fd, F_SETLKW, F_WRLCK) == -1) {
        close(fd);
        return;
    }

    // Prepare Message
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Write: [Time] [Process] Message
    dprintf(fd, "[%s] [%s] %s\n", time_str, process_name, buffer);

    // Unlock & Close
    file_lock(fd, F_SETLKW, F_UNLCK);
    close(fd);
}

// THE REGISTRATION FUNCTION
void register_process(const char *process_name) {
    int fd = open(PROCESS_LIST_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        return;
    }

    // Lock (Wait)
    if (file_lock(fd, F_SETLKW, F_WRLCK) == -1) {
        close(fd);
        return;
    }

    // Write PID
    dprintf(fd, "%s %d\n", process_name, getpid());

    // Unlock & Close
    file_lock(fd, F_SETLKW, F_UNLCK);
    close(fd);
}