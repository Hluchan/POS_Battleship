//
// Created by Patrik on 6. 1. 2026.
//
#ifndef UI_H
#define UI_H

#include "client_state.h"

void ui_init();
void ui_cleanup();

// MENU
int ui_show_main_menu();  // Vráti 1-4 (1=Create, 2=Join, 3=List, 4=Exit)
void ui_get_server_info(char* host, int* port);
void ui_get_game_config(char* name, int* board_size, int* turn_time, int* game_time);
void ui_get_game_id(int* game_id);

// V HRE
void ui_draw_game_screen(ClientState* state);
void ui_draw_placement_screen(ClientState* state, int cursor_row,
    int cursor_col, Orientation orientation);

// HRACIE DOSKY
void ui_draw_board(int start_y, int start_x, ClientState* state, int my_board, int board_size);
void ui_draw_ships_status(int start_y, int start_x, ClientState* state);

// UPOZORNENIA
void ui_show_message(const char* message);
void ui_show_error(const char* error);
void ui_show_game_over(ClientState* state);

// VSTUP
void ui_wait_for_key();

#endif //UI_H