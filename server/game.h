//
// Created by Patrik on 6. 1. 2026.
//
#ifndef GAME_H
#define GAME_H

#include "player.h"
#include "../common/protocol.h"
#include <time.h>

typedef struct Game Game;

Game* game_create(int game_id, const char* name, const GameConfig* config);
void game_destroy(Game* game);

// GETTERY
int game_get_id(const Game* game);
GameState game_get_state(const Game* game);
Player* game_get_player(Game* game, int player_id);
const char* game_get_name(const Game* game);
int game_get_players_count(const Game* game);
int game_get_current_player(const Game* game);
int game_get_board_size(const Game* game);
int game_get_turn_time(const Game* game);
int game_get_game_time(const Game* game);
const GameConfig* game_get_config(const Game* game);
int game_is_active(const Game* game);
time_t game_get_start_time(const Game* game);
time_t game_get_turn_start_time(const Game* game);

// SETTERY
void game_set_state(Game* game, GameState state);
void game_set_current_player(Game* game, int player_id);
void game_set_active(Game* game, int active);

// METODY HRY
int game_add_player(Game* game, Player* player);
int game_both_players_ready(const Game* game);
void game_start(Game* game);
void game_switch_turn(Game* game);
void game_pause(Game* game);
void game_resume(Game* game);
void game_end(Game* game);

// METODY CASU
int game_get_elapsed_time(const Game* game);
int game_get_remaining_game_time(const Game* game);
int game_get_elapsed_turn_time(const Game* game);
int game_get_remaining_turn_time(const Game* game);
int game_is_turn_timeout(const Game* game);
int game_is_game_timeout(const Game* game);
void game_reset_turn_timer(Game* game);

// STATISTIKY
int game_get_total_ships_sunk(const Game* game, int player_id);
int game_get_total_hits(const Game* game, int player_id);
int game_get_total_misses(const Game* game, int player_id);
float game_get_player_accuracy(const Game* game, int player_id);

// THREADY
void game_lock(Game* game);
void game_unlock(Game* game);

#endif //GAME_H