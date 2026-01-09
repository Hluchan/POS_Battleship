//
// Created by Patrik on 6. 1. 2026.
//
#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

void ui_init() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // Skry kurzor

    // Farby ak sú podporované
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);    // Voda
        init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Loď
        init_pair(3, COLOR_RED, COLOR_BLACK);     // Zásah
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Mínenie
        init_pair(5, COLOR_WHITE, COLOR_BLACK);   // Text
    }
}

void ui_cleanup() {
    endwin();
}

// MENU
int ui_show_main_menu() {
    clear();

    int row, col;
    getmaxyx(stdscr, row, col);

    // Titul
    attron(A_BOLD);
    mvprintw(2, (col - 30) / 2, "================================");
    mvprintw(3, (col - 30) / 2, "  BATTLESHIP - MULTIPLAYER GAME");
    mvprintw(4, (col - 30) / 2, "================================");
    attroff(A_BOLD);

    // Menu
    mvprintw(8, (col - 30) / 2, "1. Create New Game");
    mvprintw(9, (col - 30) / 2, "2. Join Existing Game");
    mvprintw(10, (col - 30) / 2, "3. List Games");
    mvprintw(11, (col - 30) / 2, "4. Exit");

    mvprintw(14, (col - 30) / 2, "Select option: ");

    refresh();

    int choice = getch();
    return choice - '0';  // '1' -> 1, '2' -> 2, atď.
}

void ui_get_server_info(char* host, int* port) {
    clear();
    echo();
    curs_set(1);

    mvprintw(5, 5, "Enter server hostname/IP [127.0.0.1]: ");
    char input[256] = {0};
    getnstr(input, 255);

    if (strlen(input) == 0) {
        strcpy(host, "127.0.0.1");
    } else {
        strcpy(host, input);
    }

    mvprintw(7, 5, "Enter port [57341]: ");
    memset(input, 0, sizeof(input));
    getnstr(input, 255);

    if (strlen(input) == 0) {
        *port = 57341;
    } else {
        *port = atoi(input);
    }

    noecho();
    curs_set(0);
}

void ui_get_game_config(char* name, int* board_size, int* turn_time, int* game_time) {
    clear();
    echo();
    curs_set(1);

    mvprintw(3, 5, "=== CREATE NEW GAME ===");

    mvprintw(5, 5, "Game name: ");
    getnstr(name, 63);

    char input[32];

    mvprintw(7, 5, "Board size (10-15) [10]: ");
    memset(input, 0, sizeof(input));
    getnstr(input, 31);
    *board_size = (strlen(input) == 0) ? 10 : atoi(input);
    if (*board_size < 10) *board_size = 10;
    if (*board_size > 15) *board_size = 15;

    mvprintw(9, 5, "Turn time in seconds (20-60) [30]: ");
    memset(input, 0, sizeof(input));
    getnstr(input, 31);
    *turn_time = (strlen(input) == 0) ? 30 : atoi(input);

    mvprintw(11, 5, "Game time in seconds (300-1200) [600]: ");
    memset(input, 0, sizeof(input));
    getnstr(input, 31);
    *game_time = (strlen(input) == 0) ? 600 : atoi(input);

    noecho();
    curs_set(0);
}

void ui_get_game_id(int* game_id) {
    clear();
    echo();
    curs_set(1);

    mvprintw(5, 5, "Enter game ID to join: ");
    scanw("%d", game_id);

    noecho();
    curs_set(0);
}

// V HRE
void ui_draw_game_screen(ClientState* state) {
    clear();

    int board_size = state->config.board_size;

    // Nadpis
    attron(A_BOLD);
    mvprintw(0, 2, "BATTLESHIP - Game ID: %d", state->config.game_id);
    attroff(A_BOLD);

    // Status
    if (state->is_my_turn) {
        attron(COLOR_PAIR(3));
        mvprintw(1, 2, "[YOUR TURN]");
        attroff(COLOR_PAIR(3));
    } else {
        mvprintw(1, 2, "[Opponent's turn]");
    }

    mvprintw(1, 30, "Turn time: %ds", state->turn_time_left);
    mvprintw(1, 50, "Game time: %ds", state->game_time_left);

    // Moje pole
    mvprintw(3, 2, "YOUR BOARD:");
    ui_draw_board(4, 2, state, 1, board_size);

    // Súperovo pole
    mvprintw(3, 40, "OPPONENT'S BOARD:");
    ui_draw_board(4, 40, state, 0, board_size);

    // Štatistiky
    int stats_y = 4 + board_size + 3;
    ui_draw_ships_status(stats_y, 2, state);

    mvprintw(stats_y + 8, 2, "Your hits: %d  misses: %d", state->my_hits, state->my_misses);
    mvprintw(stats_y + 9, 2, "Opp hits:  %d  misses: %d", state->opp_hits, state->opp_misses);

    // Posledná akcia súpera
    if (state->last_opp_shot_row >= 0) {
        mvprintw(stats_y + 11, 2, "Opponent shot at: %c%d - %s",
                 'A' + state->last_opp_shot_col,
                 state->last_opp_shot_row + 1,
                 state->last_opp_result == SHOT_HIT ? "HIT!" :
                 state->last_opp_result == SHOT_SUNK ? "SUNK!" : "Miss");
    }

    // Inštrukcie
    mvprintw(stats_y + 13, 2, "Use arrow keys to move, ENTER to shoot, Q to quit");

    refresh();
}

void ui_draw_placement_screen(ClientState* state, int cursor_row, int cursor_col, Orientation orientation) {
    clear();

    int board_size = state->config.board_size;

    attron(A_BOLD);
    mvprintw(0, 2, "SHIP PLACEMENT - Game ID: %d", state->config.game_id);
    attroff(A_BOLD);

    mvprintw(1, 2, "Ships placed: %d / 6", state->ships_placed);

    // Board
    mvprintw(3, 2, "YOUR BOARD:");
    ui_draw_board(4, 2, state, 1, board_size);

    // Kurzor
    int draw_y = 4 + cursor_row;
    int draw_x = 2 + 4 + (cursor_col * 2);
    attron(A_REVERSE);
    mvaddch(draw_y, draw_x, orientation == HORIZONTAL ? '-' : '|');
    attroff(A_REVERSE);

    // Ships status
    ui_draw_ships_status(4 + board_size + 3, 2, state);

    // Inštrukcie
    int inst_y = 4 + board_size + 13;
    mvprintw(inst_y, 2, "Arrow keys: move | R: rotate | ENTER: place ship | C: confirm ready");
    mvprintw(inst_y + 1, 2, "Ships: Carrier(5) Battleship(4) Destroyer(3)x2 Submarine(2)x2");

    refresh();
}

// HRACIE DOSKY
void ui_draw_board(int start_y, int start_x, ClientState* state, int my_board, int board_size) {
    // Stĺpce (A B C ...)
    mvprintw(start_y, start_x + 4, " ");
    for (int i = 0; i < board_size; i++) {
        mvprintw(start_y, start_x + 4 + (i * 2), "%c ", 'A' + i);
    }

    // Riadky
    for (int i = 0; i < board_size; i++) {
        mvprintw(start_y + 1 + i, start_x, "%2d ", i + 1);

        for (int j = 0; j < board_size; j++) {
            CellState cell;
            if (my_board) {
                cell = client_state_get_my_cell(state, i, j);
            } else {
                cell = client_state_get_opp_cell(state, i, j);
            }

            char ch;
            int color = 1;

            switch (cell) {
                case WATER:
                    ch = '~';
                    color = 1;  // Cyan
                    break;
                case SHIP:
                    ch = my_board ? 'S' : '~';  // Na súperovom boarde nevidíš lode
                    color = 2;  // Green
                    break;
                case HIT:
                    ch = 'X';
                    color = 3;  // Red
                    break;
                case MISS:
                    ch = 'O';
                    color = 4;  // Yellow
                    break;
                default:
                    ch = '?';
                    color = 5;
                    break;
            }

            attron(COLOR_PAIR(color));
            mvaddch(start_y + 1 + i, start_x + 4 + (j * 2), ch);
            attroff(COLOR_PAIR(color));
            mvaddch(start_y + 1 + i, start_x + 4 + (j * 2) + 1, ' ');
        }
    }
}

void ui_draw_ships_status(int start_y, int start_x, ClientState* state) {
    mvprintw(start_y, start_x, "SHIPS STATUS:");

    // Zoznam lodí
    const char* ship_names[] = {"Carrier (5)", "Battleship (4)", "Destroyer (3)", "Destroyer (3)",
                                "Submarine (2)", "Submarine (2)"};

    for (int i = 0; i < 6; i++) {
        if (i < state->ships_placed) {
            attron(COLOR_PAIR(2));
            mvprintw(start_y + 1 + i, start_x, "[X] %s", ship_names[i]);
            attroff(COLOR_PAIR(2));
        } else {
            mvprintw(start_y + 1 + i, start_x, "[ ] %s", ship_names[i]);
        }
    }
}

// UPOZORNENIA
void ui_show_message(const char* message) {
    int row, col;
    getmaxyx(stdscr, row, col);

    mvprintw(row - 2, 2, "                                                  ");
    attron(COLOR_PAIR(2));
    mvprintw(row - 2, 2, "%s", message);
    attroff(COLOR_PAIR(2));
    refresh();
}

void ui_show_error(const char* error) {
    int row, col;
    getmaxyx(stdscr, row, col);

    mvprintw(row - 2, 2, "                                                  ");
    attron(COLOR_PAIR(3));
    mvprintw(row - 2, 2, "ERROR: %s", error);
    attroff(COLOR_PAIR(3));
    refresh();
}

void ui_show_game_over(ClientState* state) {
    clear();

    int row, col;
    getmaxyx(stdscr, row, col);

    attron(A_BOLD);
    mvprintw(row / 2 - 5, (col - 20) / 2, "======================");
    mvprintw(row / 2 - 4, (col - 20) / 2, "    GAME OVER!");
    mvprintw(row / 2 - 3, (col - 20) / 2, "======================");
    attroff(A_BOLD);

    if (state->winner == state->config.player_id) {
        attron(COLOR_PAIR(2));
        mvprintw(row / 2 - 1, (col - 20) / 2, "      YOU WON!");
        attroff(COLOR_PAIR(2));
    } else if (state->winner == -1) {
        mvprintw(row / 2 - 1, (col - 20) / 2, "      DRAW!");
    } else {
        attron(COLOR_PAIR(3));
        mvprintw(row / 2 - 1, (col - 20) / 2, "      YOU LOST!");
        attroff(COLOR_PAIR(3));
    }

    mvprintw(row / 2 + 1, (col - 40) / 2, "Reason: %s", state->game_over_reason);

    mvprintw(row / 2 + 3, (col - 40) / 2, "Your ships sunk: %d", state->my_ships_sunk);
    mvprintw(row / 2 + 4, (col - 40) / 2, "Opponent ships sunk: %d", state->opp_ships_sunk);
    mvprintw(row / 2 + 5, (col - 40) / 2, "Your accuracy: %.1f%%",
             (state->my_hits + state->my_misses) > 0 ?
             (100.0 * state->my_hits / (state->my_hits + state->my_misses)) : 0.0);

    mvprintw(row / 2 + 7, (col - 40) / 2, "Press any key to return to menu...");

    refresh();
}

//VSTUP
void ui_wait_for_key() {
    getch();
}
