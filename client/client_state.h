//
// Created by Patrik on 6. 1. 2026.
//
#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include <time.h>
#include "../common/protocol.h"
#include "../common/game_constants.h"

// Stavy klienta
typedef enum {
    STATE_MENU,              // Hlavné menu
    STATE_CONNECTING,        // Pripájanie na server
    STATE_LOBBY,             // V lobby (čaká na druhého hráča)
    STATE_PLACEMENT,         // Rozmiestnenie lodí
    STATE_WAITING_START,     // Čaká na štart hry
    STATE_BATTLE,            // Hra beží
    STATE_GAME_OVER          // Hra skončila
} ClientStateEnum;

// Štruktúra pre stav klienta
typedef struct {
    // Sieťové
    int socket_fd;
    char server_host[256];
    int server_port;

    // Stav
    ClientStateEnum state;

    // Konfigurácia hry
    GameConfig config;
    int in_game;             // 1 ak je v hre

    // Herné polia
    CellState my_board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    CellState opp_board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];

    // Lode
    Ship my_ships[MAX_SHIPS];
    int ships_placed;
    int ships_ready;         // 1 ak všetky lode umiestnené
    int my_ships_status[MAX_SHIPS];    // 0=not placed, 1=placed, 2=sunk
    int opp_ships_status[MAX_SHIPS];   // 0=not sunk, 1=sunk

    // Štatistiky
    int my_ships_sunk;
    int opp_ships_sunk;
    int my_hits;
    int my_misses;
    int opp_hits;
    int opp_misses;

    // Ťah
    int is_my_turn;
    int can_shoot_again;     // 1 ak môžem strieľať znova (po zásahu)

    // Čas
    int turn_time_left;
    int game_time_left;
    time_t last_time_update;     // Kedy prišiel posledný MSG_TIME_UPDATE

    // Posledná akcia súpera
    int last_opp_shot_row;
    int last_opp_shot_col;
    ShotResult last_opp_result;

    // Game over info
    int winner;
    char game_over_reason[256];
} ClientState;

ClientState* client_state_create();
void client_state_destroy(ClientState* state);
void client_state_reset(ClientState* state);

void client_state_set_config(ClientState* state, const GameConfig* config);
void client_state_set_socket(ClientState* state, int socket_fd);

// HERNE POLE
void client_state_reset_boards(ClientState* state);
CellState client_state_get_my_cell(ClientState* state, int row, int col);
CellState client_state_get_opp_cell(ClientState* state, int row, int col);
void client_state_set_my_cell(ClientState* state, int row, int col, CellState cell);
void client_state_set_opp_cell(ClientState* state, int row, int col, CellState cell);

// LODE
int client_state_add_ship(ClientState* state, const Ship* ship);
int client_state_can_place_ship(ClientState* state, const Ship* ship);
void client_state_mark_ready(ClientState* state);

// STATISTIKY
void client_state_update_stats(ClientState* state, const GameOverMsg* msg);

#endif //CLIENT_STATE_H