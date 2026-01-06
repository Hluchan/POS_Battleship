//
// Created by Patrik on 6. 1. 2026.
//
#ifndef PLAYER_H
#define PLAYER_H

#include "../common/protocol.h"
#include "../common/game_constants.h"
#include <pthread.h>

typedef struct Player Player;

Player* player_create(int socket_fd, int player_id);
void player_destroy(Player* player);
void player_reset(Player* player);

// GETTERY
int player_get_socket(const Player* player);
int player_get_id(const Player* player);
int player_is_ready(const Player* player);
int player_is_connected(const Player* player);
int player_get_ships_placed(const Player* player);
int player_get_ships_sunk(const Player* player);
int player_get_hits(const Player* player);
int player_get_misses(const Player* player);

// SETTERY
void player_set_socket(Player* player, int socket_fd);
void player_set_ready(Player* player, int ready);
void player_set_connected(Player* player, int connected);

// HERNE POLE
CellState player_get_board_cell(const Player* player, int row, int col);
void player_set_board_cell(Player* player, int row, int col, CellState state);
CellState player_get_shot_cell(const Player* player, int row, int col);
void player_set_shot_cell(Player* player, int row, int col, CellState state);
void player_initialize_boards(Player* player);

// LODE
int player_add_ship(Player* player, const Ship* ship);
const Ship* player_get_ship(const Player* player, int index);
int player_get_total_ships(const Player* player);

// STATISTIKY
void player_increment_hits(Player* player);
void player_increment_misses(Player* player);
void player_increment_ships_sunk(Player* player);
float player_get_accuracy(const Player* player);

// THREADY
void player_lock(Player* player);
void player_unlock(Player* player);

#endif //PLAYER_H