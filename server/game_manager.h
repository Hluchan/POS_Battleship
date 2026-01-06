//
// Created by Patrik on 6. 1. 2026.
//
#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "../common/protocol.h"
#include "game.h"

#define MAX_GAMES 10

typedef struct GameManager GameManager;

GameManager* game_manager_create();
void game_manager_destroy(GameManager* mgr);

// SPRAVA HIER
int game_manager_create_game(GameManager* mgr, const char* name, const GameConfig* config);
Game* game_manager_get_game(GameManager* mgr, int game_id);
Game* game_manager_find_waiting_game(GameManager* mgr, int board_size);
int game_manager_list_games(GameManager* mgr, GameInfo* list, int max_count);
int game_manager_get_active_count(const GameManager* mgr);
void game_manager_cleanup_ended_games(GameManager* mgr);
int game_manager_remove_game(GameManager* mgr, int game_id);
int game_manager_count_games_in_state(GameManager* mgr, GameState state);
int game_manager_game_exists(GameManager* mgr, int game_id);


// THREADY
void game_manager_lock(GameManager* mgr);
void game_manager_unlock(GameManager* mgr);

#endif //GAME_MANAGER_H