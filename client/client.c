//
// Created by Patrik on 6. 1. 2026.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include "client_state.h"
#include "ui.h"
#include "../common/protocol.h"

#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT 57341

static ClientState* g_state = NULL;
void handle_server_message(Message* msg);

int connect_to_server(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sock);
        return -1;
    }

    return sock;
}

int send_message(int socket_fd, MessageType type, void* data, size_t data_size) {
    Message msg;
    msg.header.type = type;
    msg.header.length = data_size;

    if (data && data_size > 0) {
        memcpy(&msg.data, data, data_size);
    }

    ssize_t sent = send(socket_fd, &msg, sizeof(MessageHeader) + data_size, 0);
    return sent > 0 ? 1 : 0;
}

int receive_message(int socket_fd, Message* msg) {
    if (!msg) return -1;

    ssize_t received = recv(socket_fd, &msg->header, sizeof(MessageHeader), 0);
    if (received <= 0) return received;

    if (msg->header.length > 0) {
        received = recv(socket_fd, &msg->data, msg->header.length, 0);
        if (received <= 0) return received;
    }

    return 1;
}

// HERNE FUNKCIE
void handle_create_game() {
    char name[MAX_GAME_NAME];
    int board_size, turn_time, game_time;

    ui_get_game_config(name, &board_size, &turn_time, &game_time);

    CreateGameMsg cgm;
    strncpy(cgm.game_name, name, MAX_GAME_NAME - 1);
    cgm.game_name[MAX_GAME_NAME - 1] = '\0';
    cgm.board_size = board_size;
    cgm.turn_time = turn_time;
    cgm.game_time = game_time;

    send_message(g_state->socket_fd, MSG_CREATE_GAME, &cgm, sizeof(CreateGameMsg));

    // Čakaj na odpoveď
    Message msg;
    if (receive_message(g_state->socket_fd, &msg) > 0) {
        if (msg.header.type == MSG_GAME_CREATED) {
            ui_show_message("Game created! Waiting for opponent...");
            g_state->state = STATE_LOBBY;
        } else if (msg.header.type == MSG_ERROR) {
            ui_show_error(msg.data.error.error_message);
        }
    }
}

void handle_join_game() {
    // Najprv zobraz zoznam dostupných hier
    send_message(g_state->socket_fd, MSG_LIST_GAMES, NULL, 0);

    Message msg;
    int recv_result = receive_message(g_state->socket_fd, &msg);

    if (recv_result <= 0) {
        ui_show_error("Failed to receive game list from server");
        ui_wait_for_key();
        return;
    }

    if (msg.header.type != MSG_GAME_LIST) {
        if (msg.header.type == MSG_ERROR) {
            ui_show_error(msg.data.error.error_message);
        } else {
            ui_show_error("Unexpected response from server");
        }
        ui_wait_for_key();
        return;
    }

    GameListMsg* glm = &msg.data.game_list;

    clear();
    mvprintw(2, 5, "=== AVAILABLE GAMES ===");

    if (glm->games_count == 0) {
        mvprintw(4, 5, "No games available.");
        mvprintw(6, 5, "Press any key to return to menu...");
        refresh();
        getch();
        return;
    }

    for (int i = 0; i < glm->games_count; i++) {
        mvprintw(4 + i, 5, "[%d] %s - %dx%d - %d/2 players",
                 glm->games[i].game_id,
                 glm->games[i].game_name,
                 glm->games[i].board_size,
                 glm->games[i].board_size,
                 glm->games[i].players_count);
    }

    mvprintw(4 + glm->games_count + 2, 5, "Enter game ID to join: ");
    refresh();

    int game_id;
    ui_get_game_id(&game_id);

    JoinGameMsg jgm;
    jgm.game_id = game_id;

    send_message(g_state->socket_fd, MSG_JOIN_GAME, &jgm, sizeof(JoinGameMsg));

    if (receive_message(g_state->socket_fd, &msg) > 0) {
        if (msg.header.type == MSG_GAME_CONFIG) {
            client_state_set_config(g_state, &msg.data.game_config);
            g_state->state = STATE_PLACEMENT;
            ui_show_message("Joined game! Place your ships.");
        } else if (msg.header.type == MSG_ERROR) {
            ui_show_error(msg.data.error.error_message);
        }
    }
}

void handle_list_games() {
    send_message(g_state->socket_fd, MSG_LIST_GAMES, NULL, 0);

    Message msg;
    if (receive_message(g_state->socket_fd, &msg) > 0) {
        if (msg.header.type == MSG_GAME_LIST) {
            GameListMsg* glm = &msg.data.game_list;

            clear();
            mvprintw(2, 5, "=== AVAILABLE GAMES ===");

            if (glm->games_count == 0) {
                mvprintw(4, 5, "No games available.");
            } else {
                for (int i = 0; i < glm->games_count; i++) {
                    mvprintw(4 + i, 5, "[%d] %s - %dx%d - %d/2 players",
                             glm->games[i].game_id,
                             glm->games[i].game_name,
                             glm->games[i].board_size,
                             glm->games[i].board_size,
                             glm->games[i].players_count);
                }
            }

            mvprintw(4 + glm->games_count + 2, 5, "Press any key to continue...");
            refresh();
            getch();
        }
    }
}

void handle_placement_phase() {
    int cursor_row = 0;
    int cursor_col = 0;
    Orientation orientation = HORIZONTAL;

    // Ship lengths v poradí
    int ship_lengths[] = {5, 4, 3, 3, 2, 2};  // Carrier, Battleship, 2x Destroyer, 2x Submarine
    ShipType ship_types[] = {CARRIER, BATTLESHIP, DESTROYER, DESTROYER, SUBMARINE, SUBMARINE};

    while (g_state->state == STATE_PLACEMENT) {
        ui_draw_placement_screen(g_state, cursor_row, cursor_col, orientation);

        int ch = getch();

        switch (ch) {
            case KEY_UP:
                if (cursor_row > 0) cursor_row--;
                break;
            case KEY_DOWN:
                if (cursor_row < g_state->config.board_size - 1) cursor_row++;
                break;
            case KEY_LEFT:
                if (cursor_col > 0) cursor_col--;
                break;
            case KEY_RIGHT:
                if (cursor_col < g_state->config.board_size - 1) cursor_col++;
                break;
            case 'r':
            case 'R':
                orientation = (orientation == HORIZONTAL) ? VERTICAL : HORIZONTAL;
                break;
            case '\n':
            case KEY_ENTER:
                // Umiestni loď
                if (g_state->ships_placed < 6) {
                    Ship ship;
                    ship.row = cursor_row;
                    ship.col = cursor_col;
                    ship.length = ship_lengths[g_state->ships_placed];
                    ship.orientation = orientation;
                    ship.type = ship_types[g_state->ships_placed];
                    ship.hits = 0;
                    ship.sunk = 0;

                    if (client_state_add_ship(g_state, &ship)) {
                        // Pošli serveru
                        PlaceShipMsg psm;
                        psm.ship = ship;
                        send_message(g_state->socket_fd, MSG_PLACE_SHIP, &psm, sizeof(PlaceShipMsg));

                        ui_show_message("Ship placed!");
                    } else {
                        ui_show_error("Cannot place ship here!");
                    }
                }
                break;
            case 'c':
            case 'C':
                // Potvrď ready
                if (g_state->ships_ready) {
                    send_message(g_state->socket_fd, MSG_READY, NULL, 0);
                    g_state->state = STATE_WAITING_START;
                    ui_show_message("Waiting for opponent...");
                } else {
                    ui_show_error("Place all ships first!");
                }
                break;
            case 'q':
            case 'Q':
                return;
        }
    }
}

void handle_battle_phase() {
    int cursor_row = 0;
    int cursor_col = 0;

    while (g_state->state == STATE_BATTLE) {
        ui_draw_game_screen(g_state);

        // Highlight kurzor na súperovom poli
        int draw_y = 4 + cursor_row;
        int draw_x = 40 + 4 + (cursor_col * 2);
        attron(A_REVERSE);
        mvaddch(draw_y, draw_x, '^');
        attroff(A_REVERSE);
        refresh();

        timeout(1000);  // 1 sekunda timeout pre non-blocking input
        int ch = getch();

        if (ch != ERR) {
            switch (ch) {
                case KEY_UP:
                    if (cursor_row > 0) cursor_row--;
                    break;
                case KEY_DOWN:
                    if (cursor_row < g_state->config.board_size - 1) cursor_row++;
                    break;
                case KEY_LEFT:
                    if (cursor_col > 0) cursor_col--;
                    break;
                case KEY_RIGHT:
                    if (cursor_col < g_state->config.board_size - 1) cursor_col++;
                    break;
                case '\n':
                case KEY_ENTER:
                    if (g_state->is_my_turn) {
                        // Vystrel
                        ShootMsg sm;
                        sm.target.row = cursor_row;
                        sm.target.col = cursor_col;

                        send_message(g_state->socket_fd, MSG_SHOOT, &sm, sizeof(ShootMsg));
                        g_state->is_my_turn = 0;
                    }
                    break;
                case 'q':
                case 'Q':
                    send_message(g_state->socket_fd, MSG_DISCONNECT, NULL, 0);
                    return;
            }
        }

        // Check for server messages
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_state->socket_fd, &readfds);
        struct timeval tv = {0, 0};

        if (select(g_state->socket_fd + 1, &readfds, NULL, NULL, &tv) > 0) {
            Message msg;
            int recv_result = receive_message(g_state->socket_fd, &msg);
            if (recv_result > 0) {
                handle_server_message(&msg);
            } else if (recv_result <= 0) {
                // Server disconnected
                ui_show_error("Server disconnected!");
                sleep(2);
                client_state_reset(g_state);
                g_state->state = STATE_MENU;
                return;
            }
        }
    }
}

void handle_server_message(Message* msg) {
    switch (msg->header.type) {
        case MSG_GAME_CONFIG:
            client_state_set_config(g_state, &msg->data.game_config);
            g_state->state = STATE_PLACEMENT;
            break;

        case MSG_PLAYER_JOINED:
            ui_show_message("Opponent joined! Place your ships.");
            g_state->state = STATE_PLACEMENT;
            break;

        case MSG_PLACEMENT_OK:
            // OK
            break;

        case MSG_PLACEMENT_ERROR:
            ui_show_error(msg->data.error.error_message);
            break;

        case MSG_GAME_START:
            g_state->state = STATE_BATTLE;
            ui_show_message("Game started!");
            break;

        case MSG_YOUR_TURN:
            g_state->is_my_turn = 1;
            g_state->can_shoot_again = msg->data.your_turn.continue_turn;
            ui_show_message("Your turn!");
            break;

        case MSG_SHOT_RESULT:
            {
                ShotResultMsg* srm = &msg->data.shot_result;
                client_state_set_opp_cell(g_state, srm->target.row, srm->target.col,
                                         srm->result == SHOT_HIT || srm->result == SHOT_SUNK ? HIT : MISS);

                if (srm->result == SHOT_HIT) {
                    g_state->my_hits++;
                    ui_show_message("HIT!");
                } else if (srm->result == SHOT_SUNK) {
                    g_state->my_hits++;
                    g_state->opp_ships_sunk++;

                    // Označ loď ako potopenú na základe typu
                    // ShipType: CARRIER=0, BATTLESHIP=1, DESTROYER=2, SUBMARINE=3
                    // Pozícia v poli: Carrier(0), Battleship(1), Destroyer(2,3), Submarine(4,5)
                    int ship_index = -1;
                    if (srm->ship_type == CARRIER) ship_index = 0;
                    else if (srm->ship_type == BATTLESHIP) ship_index = 1;
                    else if (srm->ship_type == DESTROYER) {
                        // Nájdi prvého destroyer ktorý nie je potopený
                        ship_index = (g_state->opp_ships_status[2] == 0) ? 2 : 3;
                    }
                    else if (srm->ship_type == SUBMARINE) {
                        // Nájdi prvú submarine ktorá nie je potopená
                        ship_index = (g_state->opp_ships_status[4] == 0) ? 4 : 5;
                    }

                    if (ship_index >= 0 && ship_index < MAX_SHIPS) {
                        g_state->opp_ships_status[ship_index] = 1;  // Potopená
                    }

                    ui_show_message("SUNK!");
                } else {
                    g_state->my_misses++;
                    ui_show_message("Miss.");
                }
            }
            break;

        case MSG_OPPONENT_SHOT:
            {
                ShotResultMsg* srm = &msg->data.shot_result;
                g_state->last_opp_shot_row = srm->target.row;
                g_state->last_opp_shot_col = srm->target.col;
                g_state->last_opp_result = srm->result;

                if (srm->result == SHOT_HIT || srm->result == SHOT_SUNK) {
                    client_state_set_my_cell(g_state, srm->target.row, srm->target.col, HIT);
                    g_state->opp_hits++;

                    // Ak je loď potopená, označ ju
                    if (srm->result == SHOT_SUNK) {
                        g_state->my_ships_sunk++;

                        int ship_index = -1;
                        if (srm->ship_type == CARRIER) ship_index = 0;
                        else if (srm->ship_type == BATTLESHIP) ship_index = 1;
                        else if (srm->ship_type == DESTROYER) {
                            ship_index = (g_state->my_ships_status[2] != 2) ? 2 : 3;
                        }
                        else if (srm->ship_type == SUBMARINE) {
                            ship_index = (g_state->my_ships_status[4] != 2) ? 4 : 5;
                        }

                        if (ship_index >= 0 && ship_index < MAX_SHIPS) {
                            g_state->my_ships_status[ship_index] = 2;  // Potopená
                        }
                    }
                } else {
                    client_state_set_my_cell(g_state, srm->target.row, srm->target.col, MISS);
                    g_state->opp_misses++;
                }
            }
            break;

        case MSG_GAME_OVER:
            client_state_update_stats(g_state, &msg->data.game_over);
            g_state->state = STATE_GAME_OVER;
            ui_show_game_over(g_state);
            ui_wait_for_key();
            // Reset a návrat do menu
            client_state_reset(g_state);
            g_state->state = STATE_MENU;
            break;

        case MSG_TIME_UPDATE:
            g_state->turn_time_left = msg->data.time_update.turn_time_left;
            g_state->game_time_left = msg->data.time_update.game_time_left;
            break;

        case MSG_ERROR:
            ui_show_error(msg->data.error.error_message);
            break;

        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Vytvor stav
    g_state = client_state_create();
    if (!g_state) {
        fprintf(stderr, "Failed to create client state\n");
        return 1;
    }

    // Inicializuj UI
    ui_init();

    // Hlavný loop
    int running = 1;
    while (running) {
        if (g_state->state == STATE_MENU) {
            int choice = ui_show_main_menu();

            switch (choice) {
                case 1:  // Create game
                case 2:  // Join game
                case 3:  // List games
                    // Pripoj sa na server
                    if (g_state->socket_fd == -1) {
                        char host[256];
                        int port;
                        ui_get_server_info(host, &port);

                        int sock = connect_to_server(host, port);
                        if (sock == -1) {
                            ui_show_error("Failed to connect to server!");
                            ui_wait_for_key();
                            break;
                        }

                        client_state_set_socket(g_state, sock);
                        ui_show_message("Connected to server!");
                    }

                    // Vykonaj akciu
                    if (choice == 1) {
                        handle_create_game();
                    } else if (choice == 2) {
                        handle_join_game();
                    } else if (choice == 3) {
                        handle_list_games();
                    }
                    break;

                case 4:  // Exit
                    running = 0;
                    break;

                default:
                    ui_show_error("Invalid option!");
                    ui_wait_for_key();
                    break;
            }
        } else if (g_state->state == STATE_LOBBY) {
            // Prečítaj čakajúce správy
            while (1) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(g_state->socket_fd, &readfds);
                struct timeval tv = {0, 100000};  // 100ms timeout

                if (select(g_state->socket_fd + 1, &readfds, NULL, NULL, &tv) <= 0) {
                    break;  // Žiadne správy alebo timeout
                }

                Message msg;
                int recv_result = receive_message(g_state->socket_fd, &msg);
                if (recv_result > 0) {
                    handle_server_message(&msg);
                } else if (recv_result <= 0) {
                    ui_show_error("Server disconnected!");
                    sleep(2);
                    client_state_reset(g_state);
                    g_state->state = STATE_MENU;
                    break;
                }
            }
        } else if (g_state->state == STATE_PLACEMENT) {
            handle_placement_phase();
        } else if (g_state->state == STATE_WAITING_START) {
            ui_show_message("Waiting for game to start...");

            // Prečítaj čakajúce správy
            while (1) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(g_state->socket_fd, &readfds);
                struct timeval tv = {0, 100000};  // 100ms timeout

                if (select(g_state->socket_fd + 1, &readfds, NULL, NULL, &tv) <= 0) {
                    break;  // Žiadne správy alebo timeout
                }

                Message msg;
                int recv_result = receive_message(g_state->socket_fd, &msg);
                if (recv_result > 0) {
                    handle_server_message(&msg);
                } else if (recv_result <= 0) {
                    ui_show_error("Server disconnected!");
                    sleep(2);
                    client_state_reset(g_state);
                    g_state->state = STATE_MENU;
                    break;
                }
            }
        } else if (g_state->state == STATE_BATTLE) {
            handle_battle_phase();

            // Po skončení battle, späť do menu
            if (g_state->state == STATE_GAME_OVER || g_state->state == STATE_MENU) {
                client_state_reset(g_state);
                g_state->state = STATE_MENU;
            }
        } else if (g_state->state == STATE_GAME_OVER) {
            ui_show_game_over(g_state);
            ui_wait_for_key();
            client_state_reset(g_state);
            g_state->state = STATE_MENU;
        }
    }

    // Cleanup
    ui_cleanup();
    client_state_destroy(g_state);

    return 0;
}