//
// Created by Patrik on 6. 1. 2026.
//

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

// Funkcie hernej logiky
void handle_game_message(Game* game, int player_id, Message* msg);
int validate_ship_placement(Game* game, int player_id, Ship* ship);
int place_ship(Game* game, int player_id, Ship* ship);
void random_ship_placement(Game* game, int player_id);
ShotResult process_shot(Game* game, int shooter_id, Coordinates target);
int check_ship_sunk(Game* game, int target_player_id, int row, int col);
int check_game_over(Game* game);
void calculate_winner_by_time(Game* game, GameOverMsg* result);

// Pomocné funkcie
int is_valid_coordinate(int board_size, int row, int col);
int can_place_ship(int board[MAX_BOARD_SIZE][MAX_BOARD_SIZE],
                   int board_size, Ship* ship);

#endif // GAME_LOGIC_H