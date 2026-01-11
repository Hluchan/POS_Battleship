//
// Created by Patrik on 6. 1. 2026.
//
#include "client_state.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ClientState* client_state_create() {
    ClientState* state = malloc(sizeof(ClientState));
    if (!state) return NULL;

    memset(state, 0, sizeof(ClientState));

    state->socket_fd = -1;
    state->state = STATE_MENU;
    state->in_game = 0;
    state->last_opp_shot_row = -1;
    state->last_opp_shot_col = -1;

    client_state_reset_boards(state);

    return state;
}

void client_state_destroy(ClientState* state) {
    if (!state) return;

    if (state->socket_fd != -1) {
        close(state->socket_fd);
    }

    free(state);
}

void client_state_reset(ClientState* state) {
    if (!state) return;

    // Zatvor socket ak je otvorený
    if (state->socket_fd != -1) {
        close(state->socket_fd);
        state->socket_fd = -1;
    }

    state->state = STATE_MENU;
    state->in_game = 0;
    state->ships_placed = 0;
    state->ships_ready = 0;

    // Reset ship status
    for (int i = 0; i < MAX_SHIPS; i++) {
        state->my_ships_status[i] = 0;
        state->opp_ships_status[i] = 0;
    }

    state->my_ships_sunk = 0;
    state->opp_ships_sunk = 0;
    state->my_hits = 0;
    state->my_misses = 0;
    state->opp_hits = 0;
    state->opp_misses = 0;

    state->is_my_turn = 0;
    state->can_shoot_again = 0;

    state->turn_time_left = 0;
    state->game_time_left = 0;
    state->last_time_update = 0;

    state->last_opp_shot_row = -1;
    state->last_opp_shot_col = -1;

    client_state_reset_boards(state);
}

void client_state_set_config(ClientState* state, const GameConfig* config) {
    if (!state || !config) return;

    state->config = *config;
    state->in_game = 1;
    state->turn_time_left = config->turn_time;
    state->game_time_left = config->game_time;
}

void client_state_set_socket(ClientState* state, int socket_fd) {
    if (!state) return;
    state->socket_fd = socket_fd;
}

// HERNE POLE
void client_state_reset_boards(ClientState* state) {
    if (!state) return;

    for (int i = 0; i < MAX_BOARD_SIZE; i++) {
        for (int j = 0; j < MAX_BOARD_SIZE; j++) {
            state->my_board[i][j] = WATER;
            state->opp_board[i][j] = WATER;
        }
    }
}

CellState client_state_get_my_cell(ClientState* state, int row, int col) {
    if (!state) return WATER;
    if (row < 0 || row >= MAX_BOARD_SIZE) return WATER;
    if (col < 0 || col >= MAX_BOARD_SIZE) return WATER;

    return state->my_board[row][col];
}

CellState client_state_get_opp_cell(ClientState* state, int row, int col) {
    if (!state) return WATER;
    if (row < 0 || row >= MAX_BOARD_SIZE) return WATER;
    if (col < 0 || col >= MAX_BOARD_SIZE) return WATER;

    return state->opp_board[row][col];
}

void client_state_set_my_cell(ClientState* state, int row, int col, CellState cell) {
    if (!state) return;
    if (row < 0 || row >= MAX_BOARD_SIZE) return;
    if (col < 0 || col >= MAX_BOARD_SIZE) return;

    state->my_board[row][col] = cell;
}

void client_state_set_opp_cell(ClientState* state, int row, int col, CellState cell) {
    if (!state) return;
    if (row < 0 || row >= MAX_BOARD_SIZE) return;
    if (col < 0 || col >= MAX_BOARD_SIZE) return;

    state->opp_board[row][col] = cell;
}

// LODE
int client_state_add_ship(ClientState* state, const Ship* ship) {
    if (!state || !ship) return 0;

    if (state->ships_placed >= MAX_SHIPS) return 0;

    // Skontroluj či sa dá umiestniť
    if (!client_state_can_place_ship(state, ship)) return 0;

    // Umiestni loď na board
    for (int i = 0; i < ship->length; i++) {
        int r = ship->row + (ship->orientation == VERTICAL ? i : 0);
        int c = ship->col + (ship->orientation == HORIZONTAL ? i : 0);

        state->my_board[r][c] = SHIP;
    }

    // Pridaj do zoznamu
    state->my_ships[state->ships_placed] = *ship;
    state->my_ships_status[state->ships_placed] = 1;  // Loď umiestnená
    state->ships_placed++;

    // Skontroluj či sú všetky lode umiestnené
    int total_needed = NUM_CARRIERS + NUM_BATTLESHIPS + NUM_DESTROYERS + NUM_SUBMARINES;
    if (state->ships_placed >= total_needed) {
        state->ships_ready = 1;
    }

    return 1;
}

int client_state_can_place_ship(ClientState* state, const Ship* ship) {
    if (!state || !ship) return 0;

    int board_size = state->config.board_size;
    if (board_size == 0) board_size = 10;  // Default

    // Kontrola hraníc
    if (ship->row < 0 || ship->col < 0) return 0;
    if (ship->length < 2 || ship->length > 5) return 0;

    if (ship->orientation == HORIZONTAL) {
        if (ship->col + ship->length > board_size) return 0;
    } else {
        if (ship->row + ship->length > board_size) return 0;
    }

    // Kontrola kolízií
    for (int i = 0; i < ship->length; i++) {
        int r = ship->row + (ship->orientation == VERTICAL ? i : 0);
        int c = ship->col + (ship->orientation == HORIZONTAL ? i : 0);

        if (state->my_board[r][c] != WATER) {
            return 0;  // Kolízia
        }
    }

    return 1;
}

void client_state_mark_ready(ClientState* state) {
    if (!state) return;
    state->ships_ready = 1;
}

// STATISTIKY
void client_state_update_stats(ClientState* state, const GameOverMsg* msg) {
    if (!state || !msg) return;

    state->winner = msg->winner;

    if (state->config.player_id == 0) {
        state->my_ships_sunk = msg->player0_ships_sunk;
        state->opp_ships_sunk = msg->player1_ships_sunk;
        state->my_hits = msg->player0_hits;
        state->my_misses = msg->player0_misses;
        state->opp_hits = msg->player1_hits;
        state->opp_misses = msg->player1_misses;
    } else {
        state->my_ships_sunk = msg->player1_ships_sunk;
        state->opp_ships_sunk = msg->player0_ships_sunk;
        state->my_hits = msg->player1_hits;
        state->my_misses = msg->player1_misses;
        state->opp_hits = msg->player0_hits;
        state->opp_misses = msg->player0_misses;
    }

    strncpy(state->game_over_reason, msg->reason, sizeof(state->game_over_reason) - 1);
    state->game_over_reason[sizeof(state->game_over_reason) - 1] = '\0';
}