//
// Created by Patrik on 6. 1. 2026.
//
#ifndef SERVER_H
#define SERVER_H

#include "../common/protocol.h"
#include "player.h"


#define DEFAULT_PORT 57341
#define MAX_PORT_ATTEMPTS 10  // Skúsi 57341-57350
#define MAX_CLIENTS 100

int init_server(int port);

void* handle_client(void* arg);

// POMOCNE METODY
int send_message(int socket_fd, MessageType type, void* data, size_t data_size);
int receive_message(int socket_fd, Message* msg);
void send_error(int socket_fd, const char* error_msg);

#endif // SERVER_H