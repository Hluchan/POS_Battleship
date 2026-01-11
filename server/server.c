//
// Created by Patrik on 15. 12. 2025.
//
#include "server.h"
#include "game_manager.h"
#include "game_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

static GameManager* g_game_manager = NULL;
static int g_server_socket = -1;
static volatile int g_running = 1;
static int g_next_player_id = 0;
static pthread_mutex_t g_player_id_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int socket_fd;
    int player_id;
    Player* player;
    Game* current_game;
    int game_id;
} ClientContext;

// POMOCNE METODY
int get_next_player_id() {
    pthread_mutex_lock(&g_player_id_mutex);
    int id = g_next_player_id++;
    pthread_mutex_unlock(&g_player_id_mutex);
    return id;
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

    // Prijmi header
    ssize_t received = recv(socket_fd, &msg->header, sizeof(MessageHeader), 0);
    if (received <= 0) return received;

    // Prijmi dáta ak sú
    if (msg->header.length > 0) {
        received = recv(socket_fd, &msg->data, msg->header.length, 0);
        if (received <= 0) return received;
    }

    return 1;
}

void send_error(int socket_fd, const char* error_msg) {
    ErrorMsg err;
    strncpy(err.error_message, error_msg, sizeof(err.error_message) - 1);
    err.error_message[sizeof(err.error_message) - 1] = '\0';
    send_message(socket_fd, MSG_ERROR, &err, sizeof(ErrorMsg));
}

void send_time_update_to_both(Game* game) {
    if (!game) return;

    Player* p0 = game_get_player(game, 0);
    Player* p1 = game_get_player(game, 1);

    if (!p0 || !p1) return;

    TimeUpdateMsg tum;
    tum.turn_time_left = game_get_remaining_turn_time(game);
    tum.game_time_left = game_get_remaining_game_time(game);

    send_message(player_get_socket(p0), MSG_TIME_UPDATE, &tum, sizeof(TimeUpdateMsg));
    send_message(player_get_socket(p1), MSG_TIME_UPDATE, &tum, sizeof(TimeUpdateMsg));
}

// Game timer thread - kontroluje timeouty a posiela updates
void* game_timer_thread(void* arg) {
    (void)arg;

    printf("[Timer] Game timer thread started\n");

    while (g_running) {
        sleep(1);  // Každú sekundu

        if (!g_game_manager) continue;

        int active_games = 0;

        // Prejdi všetky hry
        for (int i = 0; i < MAX_GAMES; i++) {
            Game* game = game_manager_get_game(g_game_manager, i);
            if (!game) continue;

            GameState state = game_get_state(game);
            if (state != BATTLE_PHASE) continue;

            active_games++;

            // Debug info
            int turn_left = game_get_remaining_turn_time(game);
            int game_left = game_get_remaining_game_time(game);
            int current = game_get_current_player(game);

            if (active_games == 1) {  // Log len pre prvú hru aby sme nespamovali
                printf("[Timer] Game %d: turn=%ds, game=%ds, player=%d\n",
                       game_get_id(game), turn_left, game_left, current);
            }

            // Pošli time update
            send_time_update_to_both(game);

            // Skontroluj turn timeout
            if (game_is_turn_timeout(game)) {
                printf("[Timer] Turn timeout in game %d - switching turn\n", game_get_id(game));

                // Získaj current pred switch
                int prev_player = game_get_current_player(game);

                // Switch turn (už má vlastný lock)
                game_switch_turn(game);

                // Pošli MSG_YOUR_TURN novému hráčovi
                Player* next = game_get_player(game, game_get_current_player(game));
                if (next) {
                    YourTurnMsg ytm = { .continue_turn = 0 };
                    send_message(player_get_socket(next), MSG_YOUR_TURN, &ytm, sizeof(YourTurnMsg));

                    // Informuj predchádzajúceho hráča
                    Player* prev = game_get_player(game, prev_player);
                    if (prev) {
                        send_error(player_get_socket(prev), "Turn timeout!");
                    }
                }

                send_time_update_to_both(game);
            }

            // Skontroluj game timeout
            if (game_is_game_timeout(game)) {
                printf("[Timer] Game timeout in game %d - ending game\n", game_get_id(game));

                // Ukončenie hry (už má vlastný lock)
                game_end(game);

                // Získaj hráčov a ich štatistiky
                Player* p0 = game_get_player(game, 0);
                Player* p1 = game_get_player(game, 1);

                // Pošli finálny time update s časmi 0 aby klienti videli, že čas vypršal
                TimeUpdateMsg final_time;
                final_time.turn_time_left = 0;
                final_time.game_time_left = 0;
                if (p0) send_message(player_get_socket(p0), MSG_TIME_UPDATE, &final_time, sizeof(TimeUpdateMsg));
                if (p1) send_message(player_get_socket(p1), MSG_TIME_UPDATE, &final_time, sizeof(TimeUpdateMsg));

                int p0_sunk = player_get_ships_sunk(p0);
                int p1_sunk = player_get_ships_sunk(p1);

                // Urči víťaza podľa potopených lodí
                int winner = -1;  // Draw
                if (p0_sunk > p1_sunk) {
                    winner = 0;
                } else if (p1_sunk > p0_sunk) {
                    winner = 1;
                } else {
                    // Rovnaký počet potopených - pozri hits
                    int p0_hits = player_get_hits(p0);
                    int p1_hits = player_get_hits(p1);
                    if (p0_hits > p1_hits) winner = 0;
                    else if (p1_hits > p0_hits) winner = 1;
                }

                // Pošli game over
                GameOverMsg gom;
                gom.winner = winner;
                gom.player0_ships_sunk = p0_sunk;
                gom.player1_ships_sunk = p1_sunk;
                gom.player0_hits = player_get_hits(p0);
                gom.player1_hits = player_get_hits(p1);
                gom.player0_misses = player_get_misses(p0);
                gom.player1_misses = player_get_misses(p1);
                gom.game_time = game_get_elapsed_time(game);
                strcpy(gom.reason, "Game time expired");

                send_message(player_get_socket(p0), MSG_GAME_OVER, &gom, sizeof(GameOverMsg));
                send_message(player_get_socket(p1), MSG_GAME_OVER, &gom, sizeof(GameOverMsg));
            }
        }
    }

    return NULL;
}

Game* find_player_game(Player* player) {
    if (!player) return NULL;

    for (int i = 0; i < MAX_GAMES; i++) {
        Game* game = game_manager_get_game(g_game_manager, i);
        if (!game) continue;

        // Kontroluj oboch hráčov
        Player* p0 = game_get_player(game, 0);
        Player* p1 = game_get_player(game, 1);

        if (p0 == player || p1 == player) {
            return game;
        }
    }

    return NULL;
}

// SPRAVY
// MSG_CREATE_GAME
void handle_create_game(ClientContext* ctx, Message* msg) {
    CreateGameMsg* cgm = &msg->data.create_game;

    // Vytvor konfiguráciu
    GameConfig config;
    config.board_size = cgm->board_size;
    config.turn_time = cgm->turn_time;
    config.game_time = cgm->game_time;

    // Vytvor hru
    int game_id = game_manager_create_game(g_game_manager, cgm->game_name, &config);

    if (game_id == -1) {
        send_error(ctx->socket_fd, "Failed to create game (server full)");
        return;
    }

    // Získaj hru
    Game* game = game_manager_get_game(g_game_manager, game_id);
    if (!game) {
        send_error(ctx->socket_fd, "Internal error");
        return;
    }

    // Pridaj hráča do hry
    if (!game_add_player(game, ctx->player)) {
        send_error(ctx->socket_fd, "Failed to join game");
        game_manager_remove_game(g_game_manager, game_id);
        return;
    }

    // Ulož do kontextu
    ctx->current_game = game;
    ctx->game_id = game_id;

    // Pošli potvrdenie
    GameCreatedMsg gcm;
    gcm.game_id = game_id;
    strncpy(gcm.game_name, cgm->game_name, MAX_GAME_NAME - 1);
    gcm.game_name[MAX_GAME_NAME - 1] = '\0';

    send_message(ctx->socket_fd, MSG_GAME_CREATED, &gcm, sizeof(GameCreatedMsg));

    // Pošli konfiguráciu
    GameConfig cfg;
    cfg.board_size = game_get_board_size(game);
    cfg.turn_time = game_get_turn_time(game);
    cfg.game_time = game_get_game_time(game);
    cfg.player_id = 0;  // Prvý hráč
    cfg.game_id = game_id;

    send_message(ctx->socket_fd, MSG_GAME_CONFIG, &cfg, sizeof(GameConfig));
}

// MSG_LIST_GAMES
void handle_list_games(ClientContext* ctx, Message* msg) {
    (void)msg;  // Unused

    GameListMsg glm;
    glm.games_count = game_manager_list_games(g_game_manager, glm.games, 10);

    send_message(ctx->socket_fd, MSG_GAME_LIST, &glm, sizeof(GameListMsg));
}

// MSG_JOIN_GAME
void handle_join_game(ClientContext* ctx, Message* msg) {
    JoinGameMsg* jgm = &msg->data.join_game;

    // Získaj hru
    Game* game = game_manager_get_game(g_game_manager, jgm->game_id);
    if (!game) {
        send_error(ctx->socket_fd, "Game not found");
        return;
    }

    // Kontrola, či je hra dostupná
    if (game_get_state(game) != WAITING_FOR_PLAYER) {
        send_error(ctx->socket_fd, "Game already started or full");
        return;
    }

    // Pridaj hráča
    if (!game_add_player(game, ctx->player)) {
        send_error(ctx->socket_fd, "Failed to join game");
        return;
    }

    // Ulož do kontextu
    ctx->current_game = game;
    ctx->game_id = jgm->game_id;

    // Pošli konfiguráciu
    GameConfig cfg;
    cfg.board_size = game_get_board_size(game);
    cfg.turn_time = game_get_turn_time(game);
    cfg.game_time = game_get_game_time(game);
    cfg.player_id = 1;  // Druhý hráč
    cfg.game_id = jgm->game_id;

    send_message(ctx->socket_fd, MSG_GAME_CONFIG, &cfg, sizeof(GameConfig));

    // Oznám prvému hráčovi, že sa pripojil druhý
    Player* other = game_get_player(game, 0);
    if (other) {
        int other_socket = player_get_socket(other);
        send_message(other_socket, MSG_PLAYER_JOINED, NULL, 0);
    }
}

// MSG_PLACE_SHIP
void handle_place_ship(ClientContext* ctx, Message* msg) {
    if (!ctx->current_game) {
        send_error(ctx->socket_fd, "Not in game");
        return;
    }

    PlaceShipMsg* psm = &msg->data.place_ship;

    // Validuj a umiestni loď
    if (validate_and_place_ship(ctx->player, &psm->ship)) {
        send_message(ctx->socket_fd, MSG_PLACEMENT_OK, NULL, 0);
    } else {
        ErrorMsg err;
        strcpy(err.error_message, "Invalid ship placement");
        send_message(ctx->socket_fd, MSG_PLACEMENT_ERROR, &err, sizeof(ErrorMsg));
    }
}

// MSG_READY
void handle_ready(ClientContext* ctx, Message* msg) {
    (void)msg;

    if (!ctx->current_game) {
        send_error(ctx->socket_fd, "Not in game");
        return;
    }

    // Označ hráča ako pripraveného
    player_set_ready(ctx->player, 1);

    // Skontroluj, či sú obaja pripravení
    if (game_both_players_ready(ctx->current_game)) {
        // Začni hru
        game_start(ctx->current_game);

        // Oznám obom hráčom
        Player* p0 = game_get_player(ctx->current_game, 0);
        Player* p1 = game_get_player(ctx->current_game, 1);

        if (p0) send_message(player_get_socket(p0), MSG_GAME_START, NULL, 0);
        if (p1) send_message(player_get_socket(p1), MSG_GAME_START, NULL, 0);

        // Prvý hráč začína
        if (p0) {
            YourTurnMsg ytm = { .continue_turn = 0 };
            send_message(player_get_socket(p0), MSG_YOUR_TURN, &ytm, sizeof(YourTurnMsg));
        }

        // Pošli time update obom
        send_time_update_to_both(ctx->current_game);
    }
}

// MSG_SHOOT
void handle_shoot(ClientContext* ctx, Message* msg) {
    if (!ctx->current_game) {
        send_error(ctx->socket_fd, "Not in game");
        return;
    }

    // Kontrola stavu hry
    if (game_get_state(ctx->current_game) != BATTLE_PHASE) {
        send_error(ctx->socket_fd, "Game not in battle phase");
        return;
    }

    // Kontrola, či je hráč na ťahu
    int current = game_get_current_player(ctx->current_game);
    Player* current_player = game_get_player(ctx->current_game, current);

    if (current_player != ctx->player) {
        send_error(ctx->socket_fd, "Not your turn");
        return;
    }

    ShootMsg* sm = &msg->data.shoot;

    // Získaj súpera
    Player* opponent = game_get_player(ctx->current_game, 1 - current);

    // Kontrola, či už nebolo strielané na toto políčko
    CellState target_cell = player_get_board_cell(opponent, sm->target.row, sm->target.col);
    if (target_cell == HIT || target_cell == MISS) {
        send_error(ctx->socket_fd, "Already shot at this position");
        return;
    }

    // Spracuj strelu - shooter je current_player, target je opponent
    ShotResult result = process_shot(current_player, opponent, sm->target.row, sm->target.col);

    // Priprav výsledok
    ShotResultMsg srm;
    srm.target = sm->target;
    srm.result = result;

    // Získaj typ lode ak bola zasiahnutá alebo potopená
    if (result == SHOT_HIT || result == SHOT_SUNK) {
        Ship* hit_ship = player_find_ship_at(opponent, sm->target.row, sm->target.col);
        srm.ship_type = hit_ship ? hit_ship->type : CARRIER;  // Fallback na CARRIER ak sa nenájde
    } else {
        srm.ship_type = CARRIER;  // Pre MISS nemá význam, ale musíme poslať nejakú hodnotu
    }

    // Pošli výsledok obom
    send_message(ctx->socket_fd, MSG_SHOT_RESULT, &srm, sizeof(ShotResultMsg));
    send_message(player_get_socket(opponent), MSG_OPPONENT_SHOT, &srm, sizeof(ShotResultMsg));

    // Kontrola výhry
    if (check_game_over(ctx->current_game)) {
        // Ukonči hru
        game_end(ctx->current_game);

        // Pošli game over
        GameOverMsg gom;
        gom.winner = current;
        gom.player0_ships_sunk = game_get_total_ships_sunk(ctx->current_game, 0);
        gom.player1_ships_sunk = game_get_total_ships_sunk(ctx->current_game, 1);
        gom.player0_hits = game_get_total_hits(ctx->current_game, 0);
        gom.player1_hits = game_get_total_hits(ctx->current_game, 1);
        gom.player0_misses = game_get_total_misses(ctx->current_game, 0);
        gom.player1_misses = game_get_total_misses(ctx->current_game, 1);
        gom.game_time = game_get_elapsed_time(ctx->current_game);
        strcpy(gom.reason, "All ships sunk");

        Player* p0 = game_get_player(ctx->current_game, 0);
        Player* p1 = game_get_player(ctx->current_game, 1);

        if (p0) send_message(player_get_socket(p0), MSG_GAME_OVER, &gom, sizeof(GameOverMsg));
        if (p1) send_message(player_get_socket(p1), MSG_GAME_OVER, &gom, sizeof(GameOverMsg));

        return;
    }

    // Ak bol zásah, hráč môže strieľať znova
    if (result == SHOT_HIT || result == SHOT_SUNK) {
        // Reset turn timer aby hráč dostal plný čas na ďalší ťah!
        game_reset_turn_timer(ctx->current_game);

        YourTurnMsg ytm = { .continue_turn = 1 };
        send_message(ctx->socket_fd, MSG_YOUR_TURN, &ytm, sizeof(YourTurnMsg));
        send_time_update_to_both(ctx->current_game);
    } else {
        // Prepni ťah
        game_switch_turn(ctx->current_game);

        Player* next = game_get_player(ctx->current_game, game_get_current_player(ctx->current_game));
        YourTurnMsg ytm = { .continue_turn = 0 };
        send_message(player_get_socket(next), MSG_YOUR_TURN, &ytm, sizeof(YourTurnMsg));
        send_time_update_to_both(ctx->current_game);
    }
}

// KLIENT THREAD
void* handle_client(void* arg) {
    ClientContext* ctx = (ClientContext*)arg;

    printf("[Server] Client %d connected (socket %d)\n", ctx->player_id, ctx->socket_fd);

    Message msg;
    while (g_running) {
        // Prijmi správu
        int result = receive_message(ctx->socket_fd, &msg);

        if (result <= 0) {
            // Klient sa odpojil
            printf("[Server] Client %d disconnected\n", ctx->player_id);
            break;
        }

        // Spracuj správu
        switch (msg.header.type) {
            case MSG_CREATE_GAME:
                handle_create_game(ctx, &msg);
                break;

            case MSG_LIST_GAMES:
                handle_list_games(ctx, &msg);
                break;

            case MSG_JOIN_GAME:
                handle_join_game(ctx, &msg);
                break;

            case MSG_PLACE_SHIP:
                handle_place_ship(ctx, &msg);
                break;

            case MSG_READY:
                handle_ready(ctx, &msg);
                break;

            case MSG_SHOOT:
                handle_shoot(ctx, &msg);
                break;

            case MSG_DISCONNECT:
                printf("[Server] Client %d requested disconnect\n", ctx->player_id);
                goto cleanup;

            default:
                printf("[Server] Unknown message type: %d\n", msg.header.type);
                send_error(ctx->socket_fd, "Unknown message type");
                break;
        }
    }

cleanup:
    // Ak je hráč v hre, ukonči ju
    if (ctx->current_game) {
        GameState current_state = game_get_state(ctx->current_game);

        // Pošli "Opponent disconnected" ak hra este neskoncila
        if (current_state != GAME_ENDED) {
            game_end(ctx->current_game);

            // Oznám druhému hráčovi
            int opponent_id = (game_get_player(ctx->current_game, 0) == ctx->player) ? 1 : 0;
            Player* opponent = game_get_player(ctx->current_game, opponent_id);

            if (opponent) {
                GameOverMsg gom;
                gom.winner = opponent_id;
                strcpy(gom.reason, "Opponent disconnected");
                send_message(player_get_socket(opponent), MSG_GAME_OVER, &gom, sizeof(GameOverMsg));
            }
        }
        // Ak hra už skončila (GAME_ENDED), nepošli nič - hráč už dostal MSG_GAME_OVER

        ctx->current_game = NULL;
    }

    // Zruš player (server ho vlastní!)
    player_destroy(ctx->player);
    close(ctx->socket_fd);
    free(ctx);

    printf("[Server] Client cleanup done\n");
    return NULL;
}

// SIGNAL HANDLER
static void handle_signal(int sig) {
    (void)sig;
    printf("\n[Server] Shutting down...\n");
    g_running = 0;

    // Zavretie socketu aby sme sa nedostali do accept loopu
    if (g_server_socket != -1) {
        close(g_server_socket);
        g_server_socket = -1;
    }
}

// INICIALIZACIA A HLAVNY LOOP
int init_server(int port) {
    // Vytvor socket
    g_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_socket == -1) {
        perror("socket");
        return -1;
    }

    // Povoľ reuse address
    int opt = 1;
    setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Skúšaj bind na port s fallback mechanizmom
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    int attempts = 0;
    int current_port = port;
    int bind_success = 0;

    while (attempts < MAX_PORT_ATTEMPTS && !bind_success) {
        addr.sin_port = htons(current_port);

        if (bind(g_server_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            // Bind úspešný!
            bind_success = 1;
            printf("[Server] Successfully bound to port %d\n", current_port);
            break;
        } else {
            // Bind zlyhal, skús ďalší port
            if (errno == EADDRINUSE) {
                printf("[Server] Port %d is in use, trying %d...\n",
                       current_port, current_port + 1);
                current_port++;
                attempts++;
            } else {
                // Iná chyba
                perror("bind");
                close(g_server_socket);
                return -1;
            }
        }
    }

    if (!bind_success) {
        fprintf(stderr, "[Server] Failed to bind after %d attempts (ports %d-%d)\n",
                MAX_PORT_ATTEMPTS, port, port + MAX_PORT_ATTEMPTS - 1);
        close(g_server_socket);
        return -1;
    }

    // Listen
    if (listen(g_server_socket, 10) == -1) {
        perror("listen");
        close(g_server_socket);
        return -1;
    }

    printf("[Server] Listening on 0.0.0.0:%d\n", current_port);
    printf("[Server] Clients can connect using: <server_ip>:%d\n", current_port);
    return current_port;  // Vráť skutočný port
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            fprintf(stderr, "Port must be between 1-65535\n");
            return 1;
        }
    }

    // Signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Vytvor GameManager
    g_game_manager = game_manager_create();
    if (!g_game_manager) {
        fprintf(stderr, "Failed to create GameManager\n");
        return 1;
    }

    // Inicializuj server (vráti skutočný port)
    int actual_port = init_server(port);
    if (actual_port == -1) {
        game_manager_destroy(g_game_manager);
        return 1;
    }

    printf("[Server] Battleship server started successfully\n");
    printf("[Server] Waiting for connections...\n");
    printf("[Server] Press Ctrl+C to stop\n\n");

    // Spusti game timer thread
    pthread_t timer_thread;
    if (pthread_create(&timer_thread, NULL, game_timer_thread, NULL) != 0) {
        perror("pthread_create timer");
        game_manager_destroy(g_game_manager);
        close(g_server_socket);
        return 1;
    }
    pthread_detach(timer_thread);
    printf("[Server] Game timer thread started\n");

    // Hlavný accept loop
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_socket = accept(g_server_socket, (struct sockaddr*)&client_addr, &addr_len);

        if (client_socket == -1) {
            if (g_running) {
                perror("accept");
            }
            continue;
        }

        // Vytvor kontext pre klienta
        ClientContext* ctx = malloc(sizeof(ClientContext));
        if (!ctx) {
            close(client_socket);
            continue;
        }

        ctx->socket_fd = client_socket;
        ctx->player_id = get_next_player_id();
        ctx->player = player_create(client_socket, ctx->player_id);
        ctx->current_game = NULL;
        ctx->game_id = -1;

        if (!ctx->player) {
            free(ctx);
            close(client_socket);
            continue;
        }

        // Vytvor vlákno pre klienta
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, ctx) != 0) {
            player_destroy(ctx->player);
            free(ctx);
            close(client_socket);
            continue;
        }

        pthread_detach(thread);  // Automatický cleanup

        printf("[Server] New client connected: %s:%d (ID: %d)\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               ctx->player_id);
    }

    // Cleanup
    printf("\n[Server] Cleaning up...\n");
    // Kontrola ci uz nieje socket zatvoreny
    if (g_server_socket != -1) {
        close(g_server_socket);
        g_server_socket = -1;
    }

    // Vyčisti všetky hry
    game_manager_cleanup_ended_games(g_game_manager);
    game_manager_destroy(g_game_manager);

    printf("[Server] Server stopped\n");
    return 0;
}