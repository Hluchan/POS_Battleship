//
// Created by Patrik on 6. 1. 2026.
//
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Game Game;

struct Game {
    int game_id;
    char name[MAX_GAME_NAME];
    GameState state;
    GameConfig config;
    Player* players[2];
    int players_count;
    int current_player;
    time_t game_start_time;
    time_t turn_start_time;
    int active;
    pthread_mutex_t mutex;
};

Game* game_create(int game_id, const char* name, const GameConfig* config) {
    if (!name || !config) return NULL;

    Game* game = (Game*)malloc(sizeof(Game));
    if (!game) return NULL;

    game->game_id = game_id;
    strncpy(game->name, name, MAX_GAME_NAME - 1);
    game->name[MAX_GAME_NAME - 1] = '\0';

    game->state = WAITING_FOR_PLAYER;
    game->config = *config;

    game->players[0] = NULL;
    game->players[1] = NULL;
    game->players_count = 0;

    game->current_player = 0;
    game->game_start_time = 0;
    game->turn_start_time = 0;
    game->active = 1;

    pthread_mutex_init(&game->mutex, NULL);

    return game;
}

void game_destroy(Game* game) {
    if (!game) return;

    game_lock(game);
    game->players[0] = NULL;
    game->players[1] = NULL;
    game_unlock(game);

    pthread_mutex_destroy(&game->mutex);
    free(game);
}

// GETTERY
int game_get_id(const Game* game) {
    return game ? game->game_id : -1;
}

GameState game_get_state(const Game* game) {
    return game ? game->state : WAITING_FOR_PLAYER;
}

Player* game_get_player(Game* game, int player_id) {
    if (!game) return NULL;
    if (player_id < 0 || player_id > 1) return NULL;

    return game->players[player_id];
}

const char* game_get_name(const Game* game) {
    return game ? game->name : "";
}

int game_get_players_count(const Game* game) {
    return game ? game->players_count : 0;
}

int game_get_current_player(const Game* game) {
    return game ? game->current_player : 0;
}

int game_get_board_size(const Game* game) {
    return game ? game->config.board_size : DEFAULT_BOARD_SIZE;
}

int game_get_turn_time(const Game* game) {
    return game ? game->config.turn_time : MIN_TURN_TIME;
}

int game_get_game_time(const Game* game) {
    return game ? game->config.game_time : MIN_GAME_TIME;
}

const GameConfig* game_get_config(const Game* game) {
    return game ? &game->config : NULL;
}

int game_is_active(const Game* game) {
    return game ? game->active : 0;
}

time_t game_get_start_time(const Game* game) {
    return game ? game->game_start_time : 0;
}

time_t game_get_turn_start_time(const Game* game) {
    return game ? game->turn_start_time : 0;
}

// SETTERY
void game_set_state(Game* game, GameState state) {
    if (!game) return;

    game_lock(game);
    game->state = state;
    game_unlock(game);
}

void game_set_current_player(Game* game, int player_id) {
    if (!game) return;
    if (player_id < 0 || player_id > 1) return;

    game_lock(game);
    game->current_player = player_id;
    game_unlock(game);
}

void game_set_active(Game* game, int active) {
    if (!game) return;

    game_lock(game);
    game->active = active;
    game_unlock(game);
}

// METODY HRY
int game_add_player(Game* game, Player* player) {
    if (!game || !player) return 0;

    game_lock(game);

    // Kontrola stavu hry
    if (game->state != WAITING_FOR_PLAYER) {
        game_unlock(game);
        return 0;
    }

    // Kontrola, či máme voľné miesto
    if (game->players_count >= 2) {
        game_unlock(game);
        return 0;
    }

    // Pridaj hráča
    if (game->players[0] == NULL) {
        game->players[0] = player;
        game->players_count = 1;
    } else if (game->players[1] == NULL) {
        game->players[1] = player;
        game->players_count = 2;
        // Keď sa pripojí druhý hráč, prejdi do fázy rozmiestnenia
        game->state = PLACEMENT_PHASE;
    }

    game_unlock(game);
    return 1;
}

int game_both_players_ready(const Game* game) {
    if (!game) return 0;
    if (game->players_count != 2) return 0;
    if (!game->players[0] || !game->players[1]) return 0;

    return player_is_ready(game->players[0]) &&
           player_is_ready(game->players[1]);
}

void game_start(Game* game) {
    if (!game) return;
    if (!game_both_players_ready(game)) return;

    game_lock(game);
    game->state = BATTLE_PHASE;
    game->game_start_time = time(NULL);
    game->turn_start_time = time(NULL);
    game->current_player = 0;  // Prvý hráč začína
    game_unlock(game);
}

void game_switch_turn(Game* game) {
    if (!game) return;

    game_lock(game);
    game->current_player = 1 - game->current_player;  // 0 -> 1, 1 -> 0
    game->turn_start_time = time(NULL);
    game_unlock(game);
}

void game_end(Game* game) {
    if (!game) return;

    game_lock(game);
    game->state = GAME_ENDED;
    game->active = 0;
    game_unlock(game);
}

// METODY CASU
int game_get_elapsed_time(const Game* game) {
    if (!game) return 0;
    if (game->game_start_time == 0) return 0;

    return (int)difftime(time(NULL), game->game_start_time);
}

int game_get_remaining_game_time(const Game* game) {
    if (!game) return 0;

    int elapsed = game_get_elapsed_time(game);
    int remaining = game->config.game_time - elapsed;

    return remaining > 0 ? remaining : 0;
}

int game_get_elapsed_turn_time(const Game* game) {
    if (!game) return 0;
    if (game->turn_start_time == 0) return 0;

    return (int)difftime(time(NULL), game->turn_start_time);
}

int game_get_remaining_turn_time(const Game* game) {
    if (!game) return 0;

    int elapsed = game_get_elapsed_turn_time(game);
    int remaining = game->config.turn_time - elapsed;

    return remaining > 0 ? remaining : 0;
}

int game_is_turn_timeout(const Game* game) {
    if (!game) return 0;
    return game_get_remaining_turn_time(game) == 0;
}

int game_is_game_timeout(const Game* game) {
    if (!game) return 0;
    return game_get_remaining_game_time(game) == 0;
}

void game_reset_turn_timer(Game* game) {
    if (!game) return;

    game_lock(game);
    game->turn_start_time = time(NULL);
    game_unlock(game);
}

// STATISTIKY
int game_get_total_ships_sunk(const Game* game, int player_id) {
    if (!game) return 0;

    Player* player = game_get_player((Game*)game, player_id);
    if (!player) return 0;

    return player_get_ships_sunk(player);
}

int game_get_total_hits(const Game* game, int player_id) {
    if (!game) return 0;

    Player* player = game_get_player((Game*)game, player_id);
    if (!player) return 0;

    return player_get_hits(player);
}

int game_get_total_misses(const Game* game, int player_id) {
    if (!game) return 0;

    Player* player = game_get_player((Game*)game, player_id);
    if (!player) return 0;

    return player_get_misses(player);
}

float game_get_player_accuracy(const Game* game, int player_id) {
    if (!game) return 0.0f;

    Player* player = game_get_player((Game*)game, player_id);
    if (!player) return 0.0f;

    return player_get_accuracy(player);
}

// THREADY
void game_lock(Game* game) {
    if (game) {
        pthread_mutex_lock(&game->mutex);
    }
}

void game_unlock(Game* game) {
    if (game) {
        pthread_mutex_unlock(&game->mutex);
    }
}
