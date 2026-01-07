//
// Created by Patrik on 6. 1. 2026.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

// TODO: Toto je len minimalna verzia na testovanie

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT 57341

int connect_to_server(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return -1;
    }

    printf("[Client] Connected to %s:%d\n", host, port);
    return sock;
}

int main(int argc, char* argv[]) {
    const char* host = DEFAULT_SERVER;
    int port = DEFAULT_PORT;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }

    printf("===========================================\n");
    printf("   BATTLESHIP CLIENT - UNFINISHED\n");
    printf("===========================================\n");
    printf("Connecting to %s:%d...\n", host, port);

    // Pripoj sa na server
    int sock = connect_to_server(host, port);
    if (sock == -1) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }

    printf("\n CONNECTION SUCCESSFUL!\n\n");
    printf("Testing basic communication...\n");

    // Test: Požiadaj o zoznam hier
    Message msg;
    msg.header.type = MSG_LIST_GAMES;
    msg.header.length = 0;

    if (send(sock, &msg, sizeof(MessageHeader), 0) <= 0) {
        perror("send");
        close(sock);
        return 1;
    }

    printf("[Client] Sent MSG_LIST_GAMES request\n");

    // Prijmi odpoveď
    Message response;
    ssize_t received = recv(sock, &response, sizeof(MessageHeader), 0);
    if (received <= 0) {
        perror("recv");
        close(sock);
        return 1;
    }

    printf("[Client] Received response type: %d\n", response.header.type);

    if (response.header.type == MSG_GAME_LIST) {
        // Prijmi data ak sú
        if (response.header.length > 0) {
            recv(sock, &response.data, response.header.length, 0);
        }

        GameListMsg* glm = &response.data.game_list;
        printf("\n SERVER HAS %d ACTIVE GAMES:\n", glm->games_count);

        for (int i = 0; i < glm->games_count; i++) {
            printf("  [%d] %s - %dx%d - %d/2 players - State: %d\n",
                   glm->games[i].game_id,
                   glm->games[i].game_name,
                   glm->games[i].board_size,
                   glm->games[i].board_size,
                   glm->games[i].players_count,
                   glm->games[i].state);
        }
    }

    printf("\nTEST SUCCESSFUL\n");
    printf("\nPress Enter to disconnect...");
    getchar();

    // Odpoj sa
    msg.header.type = MSG_DISCONNECT;
    msg.header.length = 0;
    send(sock, &msg, sizeof(MessageHeader), 0);

    close(sock);
    printf("\n[Client] Disconnected\n");

    return 0;
}


