//
// Created by Patrik on 6. 1. 2026.
//
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game_constants.h"
#include <stdint.h>

#define MAX_BUFFER_SIZE 4096
#define MAX_SHIPS (NUM_CARRIERS + NUM_BATTLESHIPS + NUM_DESTROYERS + NUM_SUBMARINES)
#define MAX_GAME_NAME 64

// Typy správ medzi klientom a serverom
typedef enum {
    // Client -> Server
    MSG_CREATE_GAME,        // Vytvorenie novej hry
    MSG_LIST_GAMES,         // Žiadosť o zoznam hier
    MSG_JOIN_GAME,          // Pripojenie k existujúcej hre
    MSG_PLACE_SHIP,         // Umiestnenie lode
    MSG_READY,              // Klient je pripravený
    MSG_SHOOT,              // Strela na súperovo pole
    MSG_DISCONNECT,         // Odpojenie

    // Server -> Client
    MSG_GAME_CREATED,       // Hra vytvorená
    MSG_GAME_LIST,          // Zoznam hier
    MSG_GAME_CONFIG,        // Konfigurácia hry
    MSG_PLAYER_JOINED,      // Druhý hráč sa pripojil
    MSG_PLACEMENT_OK,       // Loď úspešne umiestnená
    MSG_PLACEMENT_ERROR,    // Chyba pri umiestnení lode
    MSG_GAME_START,         // Hra začína
    MSG_YOUR_TURN,          // Tvoj ťah
    MSG_SHOT_RESULT,        // Výsledok strely
    MSG_OPPONENT_SHOT,      // Súper vystrelil
    MSG_GAME_OVER,          // Koniec hry
    MSG_TIME_UPDATE,        // Aktualizácia času
    MSG_ERROR               // Všeobecná chyba
} MessageType;

// Štruktúry

typedef struct {
    int row;
    int col;
    int length;
    Orientation orientation;
    ShipType type;
    int hits;           // Počet zásahov na tejto lodi
    int sunk;           // 1 ak je loď potopená, 0 inak
} Ship;

typedef struct {
    int row;
    int col;
} Coordinates;

typedef struct {
    MessageType type;
    uint32_t length;    // Dĺžka dát za hlavičkou
} MessageHeader;

// (MSG_GAME_LIST)
typedef struct {
    int game_id;
    char game_name[MAX_GAME_NAME];
    int board_size;
    int turn_time;
    int game_time;
    int players_count;      // 1 alebo 2
    GameState state;
} GameInfo;

// (MSG_CREATE_GAME)
typedef struct {
    char game_name[MAX_GAME_NAME];
    int board_size;
    int turn_time;
    int game_time;
} CreateGameMsg;

// (MSG_JOIN_GAME)
typedef struct {
    int game_id;
} JoinGameMsg;

// (MSG_GAME_CREATED)
typedef struct {
    int game_id;
    char game_name[MAX_GAME_NAME];
} GameCreatedMsg;

// (MSG_GAME_LIST)
typedef struct {
    int games_count;
    GameInfo games[10];     // Max 10 hier v zozname
} GameListMsg;

// (MSG_GAME_CONFIG)
typedef struct {
    int board_size;
    int turn_time;      // Sekundy na ťah
    int game_time;      // Celkový čas hry v sekundách
    int player_id;      // 0 alebo 1
    int game_id;
} GameConfig;

// (MSG_PLACE_SHIP)
typedef struct {
    Ship ship;
} PlaceShipMsg;

// (MSG_SHOOT)
typedef struct {
    Coordinates target;
} ShootMsg;

// (MSG_SHOT_RESULT, MSG_OPPONENT_SHOT)
typedef struct {
    Coordinates target;
    ShotResult result;
    ShipType ship_type;  // Ak bola loď potopená
} ShotResultMsg;

// (MSG_GAME_OVER)
typedef struct {
    int winner;              // 0 albo 1, -1 pre remízu
    int player0_ships_sunk;
    int player1_ships_sunk;
    int player0_hits;
    int player1_hits;
    int player0_misses;
    int player1_misses;
    int game_time;           // Celkový čas hry
    char reason[256];        // Dôvod ukončenia
} GameOverMsg;

// (MSG_TIME_UPDATE)
typedef struct {
    int turn_time_left;      // Zostávajúci čas na ťah
    int game_time_left;      // Zostávajúci celkový čas
} TimeUpdateMsg;

// (MSG_ERROR, MSG_PLACEMENT_ERROR)
typedef struct {
    char error_message[256];
} ErrorMsg;

// (MSG_YOUR_TURN)
typedef struct {
    int continue_turn;       // 1 ak môže strieľať znova (po zásahu)
} YourTurnMsg;

// Union pre všetky typy dát
typedef union {
    CreateGameMsg create_game;
    JoinGameMsg join_game;
    GameCreatedMsg game_created;
    GameListMsg game_list;
    GameConfig game_config;
    PlaceShipMsg place_ship;
    ShootMsg shoot;
    ShotResultMsg shot_result;
    GameOverMsg game_over;
    TimeUpdateMsg time_update;
    ErrorMsg error;
    YourTurnMsg your_turn;
} MessageData;

// Kompletná správa
typedef struct {
    MessageHeader header;
    MessageData data;
} Message;

#endif // PROTOCOL_H
