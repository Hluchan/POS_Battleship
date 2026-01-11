//
// Created by Patrik on 6. 1. 2026.
//
#include "game_logic.h"
#include <stdlib.h>
#include <string.h>

// VALIDACIA A UMIESTNENIE LODI
int validate_and_place_ship(Player* player, const Ship* ship) {
    if (!player || !ship) return 0;

int board_size = MAX_BOARD_SIZE;

// Validácia rozmerov
if (ship->row < 0 || ship->col < 0) return 0;
if (ship->length < 2 || ship->length > 5) return 0;

// Kontrola, či loď nepresahuje hranice
if (ship->orientation == HORIZONTAL) {
    if (ship->col + ship->length > board_size) return 0;
} else {
    if (ship->row + ship->length > board_size) return 0;
}

// Kontrola kolízií s inými loďami
for (int i = 0; i < ship->length; i++) {
    int r = ship->row + (ship->orientation == VERTICAL ? i : 0);
    int c = ship->col + (ship->orientation == HORIZONTAL ? i : 0);

    if (player_get_board_cell(player, r, c) != WATER) {
        return 0;  // Kolízia
    }
}

// Umiestni loď
for (int i = 0; i < ship->length; i++) {
    int r = ship->row + (ship->orientation == VERTICAL ? i : 0);
    int c = ship->col + (ship->orientation == HORIZONTAL ? i : 0);

    player_set_board_cell(player, r, c, SHIP);
}

// Pridaj loď do zoznamu
player_add_ship(player, ship);

return 1;
}

// STRELBA
ShotResult process_shot(Player* shooter, Player* target, int row, int col) {
    if (!shooter || !target) return SHOT_MISS;

    CellState cell = player_get_board_cell(target, row, col);

    if (cell == SHIP) {
        // Zásah na target board
        player_set_board_cell(target, row, col, HIT);

        // INCREMENT stats na SHOOTER (ten čo strieľal)!
        player_increment_hits(shooter);

        // Kontrola potopenia
        Ship* hit_ship = player_find_ship_at(target, row, col);
        if (hit_ship) {
            player_increment_ship_hits(target, hit_ship);

            if (player_is_ship_sunk(hit_ship)) {
                // Loď bola potopená
                // ships_sunk sa incrementuje na TARGET (komu potopili loď)
                player_increment_ships_sunk(target);

                // Ale SHOOTER dostáva credit za potopenie (v jeho hits už je)
                return SHOT_SUNK;
            }
        }

        return SHOT_HIT;
    } else if (cell == WATER) {
        // Minutie
        player_set_board_cell(target, row, col, MISS);

        // INCREMENT stats na SHOOTER (ten čo strieľal)!
        player_increment_misses(shooter);
        return SHOT_MISS;
    } else {
        // Už bolo zasiahnuté
        return SHOT_MISS;
    }
}

// UKONCENIE HRY
int check_game_over(Game* game) {
    if (!game) return 0;

    // Kontroluj, či má niekto všetky lode potopené
    Player* p0 = game_get_player(game, 0);
    Player* p1 = game_get_player(game, 1);

    if (!p0 || !p1) return 0;

    int p0_ships = player_get_total_ships(p0);
    int p1_ships = player_get_total_ships(p1);

    int p0_sunk = player_get_ships_sunk(p0);
    int p1_sunk = player_get_ships_sunk(p1);

    // Ak má niekto všetky lode potopené
    if (p0_sunk >= p0_ships || p1_sunk >= p1_ships) {
        return 1;
    }

    // Kontrola timeoutu
    if (game_is_game_timeout(game)) {
        return 1;
    }

    return 0;
}

int determine_winner(Game* game) {
    if (!game) return -1;

    Player* p0 = game_get_player(game, 0);
    Player* p1 = game_get_player(game, 1);

    if (!p0 || !p1) return -1;

    int p0_sunk = player_get_ships_sunk(p0);
    int p1_sunk = player_get_ships_sunk(p1);

    // Viac potopených lodí = výhra
    if (p1_sunk > p0_sunk) return 0;  // p0 vyhral (potopil viac lodí súpera)
    if (p0_sunk > p1_sunk) return 1;  // p1 vyhral

    // Remíza - porovnaj zásahy
    int p0_hits = player_get_hits(p0);
    int p1_hits = player_get_hits(p1);

    if (p0_hits > p1_hits) return 0;
    if (p1_hits > p0_hits) return 1;

    return -1;  // Remíza
}
