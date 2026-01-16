#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Initialize the connection (Server listens, Client connects)
// Returns the socket file descriptor, or -1 on error.
int init_network(int mode, int *port);

// Performs the initial Handshake (ok/ook, size/sok) 
int sync_handshake(int mode, int fd);

// Exchanges positions inside the main loop
// mode: SERVER or CLIENT
// fd: Socket file descriptor
// my_drone: Pointer to my local drone state (to send)
// opponent: Pointer to the opponent obstacle (to receive)
// Returns 0 on success, -1 on failure/quit
int network_exchange(int mode, int fd, DroneState *my_drone, Obstacle *opponent);

// Closes the connection
void close_network(int fd);

#endif