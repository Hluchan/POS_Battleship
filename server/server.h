//
// Created by Patrik on 6. 1. 2026.
//
#ifndef SERVER_H
#define SERVER_H

#include "../common/protocol.h"
#include "../common/game_constants.h"
#include <pthread.h>
#include <time.h>

#define MAX_GAMES 10
#define MAX_CLIENTS (MAX_GAMES * 2)

// Štruktúry
typedef struct {
    int socket_fd;
    int player_id;          // 0 alebo 1
    int ready;              // 1 ak je hráč pripravený
    int connected;          // 1 ak je hráč pripojený
    Ship ships[MAX_SHIPS];
    int board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    int shots[MAX_BOARD_SIZE][MAX_BOARD_SIZE];  // Pole kde hráč strieľal
    int ships_placed;       // Počet umiestnených lodí
    int ships_sunk;         // Počet potopených lodí súpera
    int hits;
    int misses;
    pthread_mutex_t mutex;
} Player;

typedef struct {
    int game_id;
    GameState state;
    GameConfig config;
    Player players[2];
    int current_player;     // 0 alebo 1
    time_t game_start_time;
    time_t turn_start_time;
    int paused_by;          // -1 ak nie je pozastavená, inak ID hráča
    pthread_t game_thread;
    pthread_mutex_t game_mutex;
    int active;             // 1 ak je hra aktívna
} Game;

typedef struct {
    int server_socket;
    int port;
    Game games[MAX_GAMES];
    pthread_mutex_t games_mutex;
    int running;
} ServerState;

// Funkcie
int init_server(int port);
void* handle_client(void* arg);
void* game_loop(void* arg);
Game* create_game(int client_socket, GameConfig config);
Game* find_waiting_game(int board_size);
int send_message(int socket_fd, Message* msg);
int receive_message(int socket_fd, Message* msg);
void cleanup_game(Game* game);
void broadcast_to_game(Game* game, Message* msg);

#endif // SERVER_H