#ifndef FLUENT2048_GAME_H
#define FLUENT2048_GAME_H

#include <stdbool.h>

#define BOARD_SIZE 4
#define MAX_MOVE_ANIMATIONS 16

typedef struct {
    int value;
    int from_row;
    int from_column;
    int to_row;
    int to_column;
    bool merged;
} TileMotion;

typedef struct {
    int cells[BOARD_SIZE][BOARD_SIZE];
    int previous_cells[BOARD_SIZE][BOARD_SIZE];
    int score;
    int previous_score;
    int best_score;
    bool won;
    bool paused;
    bool can_undo;

    TileMotion motions[MAX_MOVE_ANIMATIONS];
    int motion_count;
    int animation_frame;
    int animation_duration;
    int spawn_row;
    int spawn_column;
    int spawn_frame;
} GameState;

void game_init(GameState *game);
void game_reset(GameState *game);
bool game_move(GameState *game, int direction);
void game_undo(GameState *game);
bool game_is_over(const GameState *game);
void game_tick_animation(GameState *game);
bool game_is_animating(const GameState *game);

#endif
