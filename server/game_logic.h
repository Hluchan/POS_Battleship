//
// Created by Patrik on 6. 1. 2026.
//
#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "player.h"
#include "game.h"
#include "../common/protocol.h"

// VALIDACIA A UMIESTNENIE LODI
int validate_and_place_ship(Player* player, const Ship* ship);

// STRELBA
ShotResult process_shot(Player* shooter, Player* target, int row, int col);

// UKONCENIE HRY
int check_game_over(Game* game);
int determine_winner(Game* game);

#endif // GAME_LOGIC_H