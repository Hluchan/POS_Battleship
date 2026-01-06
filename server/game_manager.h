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
// Vytvorenie novej hry (vracia game_id alebo -1 pri chybe)
int game_manager_create_game(GameManager* mgr, const char* name,
                              const GameConfig* config);

// Získanie hry podľa ID
Game* game_manager_get_game(GameManager* mgr, int game_id);

// Nájdenie čakajúcej hry s danou veľkosťou dosky
Game* game_manager_find_waiting_game(GameManager* mgr, int board_size);

// Zoznam všetkých aktívnych hier
int game_manager_list_games(GameManager* mgr, GameInfo* list, int max_count);

// Počet aktívnych hier
int game_manager_get_active_count(const GameManager* mgr);

// Vyčistenie ukončených hier
void game_manager_cleanup_ended_games(GameManager* mgr);

// THREADY
void game_manager_lock(GameManager* mgr);
void game_manager_unlock(GameManager* mgr);

#endif //GAME_MANAGER_H