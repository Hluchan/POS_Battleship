//
// Created by Patrik on 6. 1. 2026.
//
#include "player.h"
#include <stdlib.h>
#include <string.h>

struct Player {
    int socket_fd;
    int player_id;
    int connected;
    int ready;
    // HERNE POLE
    int board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    int shots[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    // LODE
    Ship ships[MAX_SHIPS];
    int ships_placed;
    int ships_sunk;
    // STATISTIKY
    int hits;
    int misses;
    // THREADY
    pthread_mutex_t mutex;
};

Player* player_create(int socket_fd, int player_id) {
    Player* player = (Player*)malloc(sizeof(Player));
    if (!player) {
        return NULL;
    }

    // Inicializácia
    player->socket_fd = socket_fd;
    player->player_id = player_id;
    player->connected = 1;
    player->ready = 0;
    player->ships_placed = 0;
    player->ships_sunk = 0;
    player->hits = 0;
    player->misses = 0;

    // Mutex
    pthread_mutex_init(&player->mutex, NULL);

    // Dosky
    player_initialize_boards(player);

    return player;
}

void player_destroy(Player* player) {
    if (player) {
        pthread_mutex_destroy(&player->mutex);
        free(player);
    }
}

void player_reset(Player* player) {
    if (!player) return;

    player_lock(player);
    player->ready = 0;
    player->ships_placed = 0;
    player->ships_sunk = 0;
    player->hits = 0;
    player->misses = 0;
    player_initialize_boards(player);
    player_unlock(player);
}

// GETTERY
int player_get_socket(const Player* player) {
    return player ? player->socket_fd : -1;
}

int player_get_id(const Player* player) {
    return player ? player->player_id : -1;
}

int player_is_ready(const Player* player) {
    return player ? player->ready : 0;
}

int player_is_connected(const Player* player) {
    return player ? player->connected : 0;
}

int player_get_ships_placed(const Player* player) {
    return player ? player->ships_placed : 0;
}

int player_get_ships_sunk(const Player* player) {
    return player ? player->ships_sunk : 0;
}

int player_get_hits(const Player* player) {
    return player ? player->hits : 0;
}

int player_get_misses(const Player* player) {
    return player ? player->misses : 0;
}

// SETTERY
void player_set_socket(Player* player, int socket_fd) {
    if (player) {
        player_lock(player);
        player->socket_fd = socket_fd;
        player_unlock(player);
    }
}

void player_set_ready(Player* player, int ready) {
    if (player) {
        player_lock(player);
        player->ready = ready;
        player_unlock(player);
    }
}

void player_set_connected(Player* player, int connected) {
    if (player) {
        player_lock(player);
        player->connected = connected;
        player_unlock(player);
    }
}

// HERNE POLE
CellState player_get_board_cell(const Player* player, int row, int col) {
    if (!player) return WATER;
    if (row < 0 || row >= MAX_BOARD_SIZE) return WATER;
    if (col < 0 || col >= MAX_BOARD_SIZE) return WATER;

    return player->board[row][col];
}

void player_set_board_cell(Player* player, int row, int col, CellState state) {
    if (!player) return;
    if (row < 0 || row >= MAX_BOARD_SIZE) return;
    if (col < 0 || col >= MAX_BOARD_SIZE) return;

    player_lock(player);
    player->board[row][col] = state;
    player_unlock(player);
}

CellState player_get_shot_cell(const Player* player, int row, int col) {
    if (!player) return WATER;
    if (row < 0 || row >= MAX_BOARD_SIZE) return WATER;
    if (col < 0 || col >= MAX_BOARD_SIZE) return WATER;

    return player->shots[row][col];
}

void player_set_shot_cell(Player* player, int row, int col, CellState state) {
    if (!player) return;
    if (row < 0 || row >= MAX_BOARD_SIZE) return;
    if (col < 0 || col >= MAX_BOARD_SIZE) return;

    player_lock(player);
    player->shots[row][col] = state;
    player_unlock(player);
}

void player_initialize_boards(Player* player) {
    if (!player) return;

    player_lock(player);
    memset(player->board, WATER, sizeof(player->board));
    memset(player->shots, WATER, sizeof(player->shots));
    player_unlock(player);
}

// LODE
int player_add_ship(Player* player, const Ship* ship) {
    if (!player || !ship) return 0;
    if (player->ships_placed >= MAX_SHIPS) return 0;

    player_lock(player);
    player->ships[player->ships_placed] = *ship;
    player->ships_placed++;
    player_unlock(player);

    return 1;
}

const Ship* player_get_ship(const Player* player, int index) {
    if (!player) return NULL;
    if (index < 0 || index >= player->ships_placed) return NULL;

    return &player->ships[index];
}

int player_get_total_ships(const Player* player) {
    return player ? player->ships_placed : 0;
}

Ship* player_find_ship_at(Player* player, int row, int col) {
    if (!player) return NULL;
    for (int i = 0; i < player->ships_placed; i++) {
        Ship* ship = &player->ships[i];

        if (ship->orientation == HORIZONTAL) {
            // Horizontálna loď - rovnaký riadok, stĺpce od col po col+length-1
            if (ship->row == row &&
                col >= ship->col &&
                col < ship->col + ship->length) {
                return ship;
                }
        } else { // VERTICAL
            // Vertikálna loď - rovnaký stĺpec, riadky od row po row+length-1
            if (ship->col == col &&
                row >= ship->row &&
                row < ship->row + ship->length) {
                return ship;
                }
        }
    }

    return NULL;
}

void player_increment_ship_hits(Player* player, Ship* ship) {
    if (!player || !ship) return;

    player_lock(player);
    ship->hits++;
    player_unlock(player);
}

int player_is_ship_sunk(const Ship* ship) {
    if (!ship) return 0;
    return ship->hits >= ship->length;
}

// STATISTIKY
void player_increment_hits(Player* player) {
    if (player) {
        player_lock(player);
        player->hits++;
        player_unlock(player);
    }
}

void player_increment_misses(Player* player) {
    if (player) {
        player_lock(player);
        player->misses++;
        player_unlock(player);
    }
}

void player_increment_ships_sunk(Player* player) {
    if (player) {
        player_lock(player);
        player->ships_sunk++;
        player_unlock(player);
    }
}

float player_get_accuracy(const Player* player) {
    if (!player) return 0.0f;

    int total = player->hits + player->misses;
    if (total == 0) return 0.0f;

    return (float)player->hits / (float)total * 100.0f;
}

// THREADY
void player_lock(Player* player) {
    if (player) {
        pthread_mutex_lock(&player->mutex);
    }
}

void player_unlock(Player* player) {
    if (player) {
        pthread_mutex_unlock(&player->mutex);
    }
}
