//
// Created by Patrik on 6. 1. 2026.
//
#include "game_manager.h"
#include <stdlib.h>
#include <string.h>

struct GameManager {
    Game* games[MAX_GAMES];
    int games_count;
    pthread_mutex_t mutex;
};

GameManager* game_manager_create() {
    GameManager* mgr = (GameManager*)malloc(sizeof(GameManager));
    if (!mgr) return NULL;

    for (int i = 0; i < MAX_GAMES; i++) {
        mgr->games[i] = NULL;
    }
    mgr->games_count = 0;

    pthread_mutex_init(&mgr->mutex, NULL);

    return mgr;
}

void game_manager_destroy(GameManager* mgr) {
    if (!mgr) return;

    game_manager_lock(mgr);

    // Zruš všetky aktívne hry
    for (int i = 0; i < MAX_GAMES; i++) {
        if (mgr->games[i] != NULL) {
            game_destroy(mgr->games[i]);
            mgr->games[i] = NULL;
        }
    }

    game_manager_unlock(mgr);

    pthread_mutex_destroy(&mgr->mutex);
    free(mgr);
}

// SPRAVA HIER
int game_manager_create_game(GameManager* mgr, const char* name, const GameConfig* config) {
    if (!mgr || !name || !config) return -1;

    game_manager_lock(mgr);

    // Nájdi voľný slot
    for (int i = 0; i < MAX_GAMES; i++) {
        if (mgr->games[i] == NULL) {
            // Vytvor hru
            mgr->games[i] = game_create(i, name, config);

            if (mgr->games[i] != NULL) {
                mgr->games_count++;
                game_manager_unlock(mgr);
                return i;  // game_id
            } else {
                // Chyba pri vytváraní hry
                game_manager_unlock(mgr);
                return -1;
            }
        }
    }

    // Plné - žiadne voľné sloty
    game_manager_unlock(mgr);
    return -1;
}

Game* game_manager_get_game(GameManager* mgr, int game_id) {
    if (!mgr) return NULL;
    if (game_id < 0 || game_id >= MAX_GAMES) return NULL;

    // Netreba mutex pre len čítanie pointera
    return mgr->games[game_id];
}

Game* game_manager_find_waiting_game(GameManager* mgr, int board_size) {
    if (!mgr) return NULL;

    game_manager_lock(mgr);

    // Prejdi všetky hry
    for (int i = 0; i < MAX_GAMES; i++) {
        Game* game = mgr->games[i];

        if (game != NULL &&
            game_is_active(game) &&
            game_get_state(game) == WAITING_FOR_PLAYER &&
            game_get_board_size(game) == board_size &&
            game_get_players_count(game) < 2) {

            game_manager_unlock(mgr);
            return game;
            }
    }

    game_manager_unlock(mgr);
    return NULL;  // Nenašli sme žiadnu vhodnú hru
}

int game_manager_list_games(GameManager* mgr, GameInfo* list, int max_count) {
    if (!mgr || !list || max_count <= 0) return 0;

    game_manager_lock(mgr);

    int count = 0;

    // Prejdi všetky hry
    for (int i = 0; i < MAX_GAMES && count < max_count; i++) {
        Game* game = mgr->games[i];

        if (game != NULL && game_is_active(game)) {
            // Naplň GameInfo štruktúru
            list[count].game_id = game_get_id(game);
            strncpy(list[count].game_name, game_get_name(game), MAX_GAME_NAME - 1);
            list[count].game_name[MAX_GAME_NAME - 1] = '\0';
            list[count].board_size = game_get_board_size(game);
            list[count].turn_time = game_get_turn_time(game);
            list[count].game_time = game_get_game_time(game);
            list[count].players_count = game_get_players_count(game);
            list[count].state = game_get_state(game);

            count++;
        }
    }

    game_manager_unlock(mgr);
    return count;
}

int game_manager_get_active_count(const GameManager* mgr) {
    if (!mgr) return 0;

    // Môžeme čítať games_count bez mutexu (atomic read)
    return mgr->games_count;
}

void game_manager_cleanup_ended_games(GameManager* mgr) {
    if (!mgr) return;

    game_manager_lock(mgr);

    // Prejdi všetky hry
    for (int i = 0; i < MAX_GAMES; i++) {
        Game* game = mgr->games[i];

        if (game != NULL &&
            (!game_is_active(game) || game_get_state(game) == GAME_ENDED)) {

            // Zruš hru
            game_destroy(game);
            mgr->games[i] = NULL;
            mgr->games_count--;
            }
    }

    game_manager_unlock(mgr);
}

int game_manager_remove_game(GameManager* mgr, int game_id) {
    if (!mgr) return 0;
    if (game_id < 0 || game_id >= MAX_GAMES) return 0;

    game_manager_lock(mgr);

    if (mgr->games[game_id] != NULL) {
        game_destroy(mgr->games[game_id]);
        mgr->games[game_id] = NULL;
        mgr->games_count--;

        game_manager_unlock(mgr);
        return 1;
    }

    game_manager_unlock(mgr);
    return 0;
}

int game_manager_count_games_in_state(GameManager* mgr, GameState state) {
    if (!mgr) return 0;

    game_manager_lock(mgr);

    int count = 0;

    for (int i = 0; i < MAX_GAMES; i++) {
        Game* game = mgr->games[i];
        if (game != NULL && game_get_state(game) == state) {
            count++;
        }
    }

    game_manager_unlock(mgr);
    return count;
}

int game_manager_game_exists(GameManager* mgr, int game_id) {
    if (!mgr) return 0;
    if (game_id < 0 || game_id >= MAX_GAMES) return 0;

    return mgr->games[game_id] != NULL;
}

// THREADY
void game_manager_lock(GameManager* mgr) {
    if (mgr) {
        pthread_mutex_lock(&mgr->mutex);
    }
}

void game_manager_unlock(GameManager* mgr) {
    if (mgr) {
        pthread_mutex_unlock(&mgr->mutex);
    }
}
