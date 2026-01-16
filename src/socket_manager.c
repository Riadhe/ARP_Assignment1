#include "socket_manager.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>

#define BUFFER_SIZE 256

// Helper: Send string + MICRO Delay
// 1ms (1000us) is enough to split packets but won't freeze the physics engine.
void send_msg(int fd, const char *msg) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s\n", msg);
    if (write(fd, buffer, strlen(buffer)) < 0) perror("[Net] Write failed");
    
    // [OPTIMIZATION] Reduced from 10ms to 1ms
    // This keeps the loop running at ~500Hz instead of ~30Hz
    usleep(1000); 
}

// Helper: Smart Reader (Handles Friend's 256-byte null-padded messages)
int read_msg(int fd, char *buffer) {
    memset(buffer, 0, BUFFER_SIZE);
    char c;
    int i = 0;
    
    // Safety: Don't get stuck in an infinite loop of nulls
    int nulls_skipped = 0;
    const int MAX_NULLS = 1000; 

    while (i < BUFFER_SIZE - 1) {
        if (read(fd, &c, 1) <= 0) return -1; // Error or Disconnect
        
        // Skip leading nulls (Friend's padding)
        if (c == '\0' && i == 0) {
            nulls_skipped++;
            if (nulls_skipped > MAX_NULLS) return -1; // Protect against infinite junk
            continue; 
        }

        // Stop on Newline or Null (if we have data)
        if (c == '\n' || c == '\0') {
            buffer[i] = '\0';
            return i;
        }
        
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return i;
}

int init_network(int mode, int *port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(*port);

    if (mode == MODE_SERVER) {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        int opt = 1; setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) return -1;
        listen(sockfd, 1);
        printf("[Net] Waiting for player on port %d...\n", *port); fflush(stdout);
        
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) return -1;
        
        close(sockfd);
        return newsockfd; 
    } else { 
        char ip[32];
        printf("Enter Server IP (default 127.0.0.1): "); fflush(stdout);
        if (fgets(ip, sizeof(ip), stdin) && strlen(ip) > 1) {
             ip[strcspn(ip, "\n")] = 0; 
             if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
        } else {
             inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
        }

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) return -1;
        return sockfd;
    }
}

// HANDSHAKE (FRIEND COMPATIBLE)
int sync_handshake(int mode, int fd) {
    char buf[BUFFER_SIZE];
    if (mode == MODE_SERVER) {
        send_msg(fd, "ok");
        if (read_msg(fd, buf) <= 0 || strcmp(buf, "ook") != 0) return -1;
        
        // Friend expects JUST numbers "100,100" (No "size")
        char size_msg[64]; 
        sprintf(size_msg, "%d,%d", MAP_WIDTH, MAP_HEIGHT);
        send_msg(fd, size_msg);
        
        if (read_msg(fd, buf) <= 0) return -1;
        // Accept "sok" or "sok..."
        if (strncmp(buf, "sok", 3) != 0) return -1;
    } else {
        if (read_msg(fd, buf) <= 0 || strcmp(buf, "ok") != 0) return -1;
        send_msg(fd, "ook");
        
        if (read_msg(fd, buf) <= 0) return -1; // Rcv size
        send_msg(fd, "sok");
    }
    printf("[Net] Handshake OK.\n"); fflush(stdout);
    return 0;
}

// DATA EXCHANGE 
int network_exchange(int mode, int fd, DroneState *my_drone, Obstacle *opponent) {
    char buf[BUFFER_SIZE];
    char msg[BUFFER_SIZE];

    // Flip Y for Friend (Bottom-Left)
    float my_y_net = (float)MAP_HEIGHT - my_drone->position.y;

    if (mode == MODE_SERVER) {
        send_msg(fd, "drone");
        
        // Send Pos (Space separated for safety)
        sprintf(msg, "%.2f %.2f", my_drone->position.x, my_y_net);
        send_msg(fd, msg);
        
        if (read_msg(fd, buf) <= 0) return -1; // dok

        send_msg(fd, "obst");
        if (read_msg(fd, buf) <= 0) return -1; // coords
        
        // Clean commas if present
        for(int i=0; buf[i]; i++) if(buf[i]==',') buf[i]=' ';

        float op_x, op_y_net;
        if (sscanf(buf, "%f %f", &op_x, &op_y_net) == 2) {
             opponent->position.x = op_x;
             opponent->position.y = (float)MAP_HEIGHT - op_y_net;
        }
        opponent->id = 0; 

        send_msg(fd, "pok");

    } else { 
        if (read_msg(fd, buf) <= 0) return -1;

        if (strcmp(buf, "drone") == 0) {
            if (read_msg(fd, buf) <= 0) return -1;
            
            for(int i=0; buf[i]; i++) if(buf[i]==',') buf[i]=' ';
            
            float op_x, op_y_net;
            if (sscanf(buf, "%f %f", &op_x, &op_y_net) == 2) {
                opponent->position.x = op_x;
                opponent->position.y = (float)MAP_HEIGHT - op_y_net;
            }
            opponent->id = 0;
            send_msg(fd, "dok");
        } 
        else if (strcmp(buf, "obst") == 0) {
            sprintf(msg, "%.2f %.2f", my_drone->position.x, my_y_net);
            send_msg(fd, msg);
            if (read_msg(fd, buf) <= 0) return -1; 
        }
        else if (strcmp(buf, "q") == 0) {
            send_msg(fd, "qok");
            return -1; 
        }
    }
    return 0;
}

void close_network(int fd) { close(fd); }