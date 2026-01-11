//
// Created by Patrik on 6. 1. 2026.
//
#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#define MIN_BOARD_SIZE 10
#define MAX_BOARD_SIZE 15
#define DEFAULT_BOARD_SIZE 10

// Časové limity
#define MIN_TURN_TIME 20
#define MAX_TURN_TIME 60
#define MIN_GAME_TIME 300   // 5 minút v sekundách
#define MAX_GAME_TIME 1200  // 20 minút v sekundách

// Typy lodí a ich veľkosti
typedef enum {
    CARRIER = 5,
    BATTLESHIP = 4,
    DESTROYER = 3,
    SUBMARINE = 2
} ShipType;

// Počet lodí každého typu
#define NUM_CARRIERS 1
#define NUM_BATTLESHIPS 1
#define NUM_DESTROYERS 2
#define NUM_SUBMARINES 2
#define TOTAL_SHIPS 6

// Orientácia lode
typedef enum {
    HORIZONTAL = 0,
    VERTICAL = 1
} Orientation;

// Stavy políčka
typedef enum {
    WATER = 0,        // ~
    SHIP = 1,         // S
    HIT = 2,          // X
    MISS = 3          // O
} CellState;

// Stavy hry
typedef enum {
    WAITING_FOR_PLAYER,
    PLACEMENT_PHASE,
    BATTLE_PHASE,
    GAME_ENDED
} GameState;

// Výsledky ťahu
typedef enum {
    SHOT_MISS,
    SHOT_HIT,
    SHOT_SUNK
} ShotResult;

#endif // GAME_CONSTANTS_H
