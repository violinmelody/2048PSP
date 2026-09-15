#include "game.h"

#include <stdlib.h>
#include <string.h>

#define MOVE_FRAMES 7
#define SPAWN_FRAMES 7

typedef struct {
    int value;
    int source_index;
} LineTile;

static void map_cell(int direction, int line, int index, int *row, int *column) {
    if (direction == 0) { *row = index; *column = line; }
    else if (direction == 1) { *row = BOARD_SIZE - 1 - index; *column = line; }
    else if (direction == 2) { *row = line; *column = index; }
    else { *row = line; *column = BOARD_SIZE - 1 - index; }
}

static void add_random_tile(GameState *game, bool animate) {
    int empty[BOARD_SIZE * BOARD_SIZE][2];
    int count = 0;
    int row, column;

    for (row = 0; row < BOARD_SIZE; ++row) {
        for (column = 0; column < BOARD_SIZE; ++column) {
            if (game->cells[row][column] == 0) {
                empty[count][0] = row;
                empty[count][1] = column;
                ++count;
            }
        }
    }
    if (count == 0) return;

    {
        const int selected = rand() % count;
        row = empty[selected][0];
        column = empty[selected][1];
        game->cells[row][column] = (rand() % 10 == 0) ? 4 : 2;
        if (animate) {
            game->spawn_row = row;
            game->spawn_column = column;
            game->spawn_frame = SPAWN_FRAMES;
        }
    }
}

static void add_motion(GameState *game, int value, int direction, int line,
                       int from_index, int to_index, bool merged) {
    TileMotion *motion;
    if (game->motion_count >= MAX_MOVE_ANIMATIONS) return;
    motion = &game->motions[game->motion_count++];
    motion->value = value;
    map_cell(direction, line, from_index, &motion->from_row, &motion->from_column);
    map_cell(direction, line, to_index, &motion->to_row, &motion->to_column);
    motion->merged = merged;
}

static bool process_line(GameState *game, int direction, int line_index) {
    LineTile input[BOARD_SIZE];
    int output[BOARD_SIZE] = {0};
    int input_count = 0;
    int out = 0;
    int i;
    bool changed = false;

    for (i = 0; i < BOARD_SIZE; ++i) {
        int row, column;
        map_cell(direction, line_index, i, &row, &column);
        if (game->cells[row][column] != 0) {
            input[input_count].value = game->cells[row][column];
            input[input_count].source_index = i;
            ++input_count;
        }
    }

    i = 0;
    while (i < input_count) {
        if (i + 1 < input_count && input[i].value == input[i + 1].value) {
            const int merged_value = input[i].value * 2;
            output[out] = merged_value;
            add_motion(game, input[i].value, direction, line_index, input[i].source_index, out, true);
            add_motion(game, input[i + 1].value, direction, line_index, input[i + 1].source_index, out, true);
            if (input[i].source_index != out || input[i + 1].source_index != out) changed = true;
            game->score += merged_value;
            if (merged_value >= 2048) game->won = true;
            i += 2;
        } else {
            output[out] = input[i].value;
            add_motion(game, input[i].value, direction, line_index, input[i].source_index, out, false);
            if (input[i].source_index != out) changed = true;
            ++i;
        }
        ++out;
    }

    for (i = 0; i < BOARD_SIZE; ++i) {
        int row, column;
        map_cell(direction, line_index, i, &row, &column);
        if (game->cells[row][column] != output[i]) changed = true;
        game->cells[row][column] = output[i];
    }
    return changed;
}

void game_init(GameState *game) {
    memset(game, 0, sizeof(*game));
    game_reset(game);
}

void game_reset(GameState *game) {
    const int best_score = game->best_score;
    memset(game, 0, sizeof(*game));
    game->best_score = best_score;
    game->spawn_row = game->spawn_column = -1;
    add_random_tile(game, false);
    add_random_tile(game, false);
}

bool game_move(GameState *game, int direction) {
    int line;
    bool changed = false;

    memcpy(game->previous_cells, game->cells, sizeof(game->cells));
    game->previous_score = game->score;
    game->motion_count = 0;
    game->spawn_frame = 0;

    for (line = 0; line < BOARD_SIZE; ++line) changed |= process_line(game, direction, line);

    if (!changed) {
        memcpy(game->cells, game->previous_cells, sizeof(game->cells));
        game->motion_count = 0;
        return false;
    }

    add_random_tile(game, true);
    game->can_undo = true;
    game->animation_duration = MOVE_FRAMES;
    game->animation_frame = MOVE_FRAMES;
    if (game->score > game->best_score) game->best_score = game->score;
    return true;
}

void game_undo(GameState *game) {
    if (!game->can_undo || game_is_animating(game)) return;
    memcpy(game->cells, game->previous_cells, sizeof(game->cells));
    game->score = game->previous_score;
    game->can_undo = false;
    game->won = false;
    game->motion_count = 0;
    game->spawn_frame = 0;
}

bool game_is_over(const GameState *game) {
    int row, column;
    for (row = 0; row < BOARD_SIZE; ++row) {
        for (column = 0; column < BOARD_SIZE; ++column) {
            const int value = game->cells[row][column];
            if (value == 0) return false;
            if (column + 1 < BOARD_SIZE && value == game->cells[row][column + 1]) return false;
            if (row + 1 < BOARD_SIZE && value == game->cells[row + 1][column]) return false;
        }
    }
    return true;
}

void game_tick_animation(GameState *game) {
    if (game->animation_frame > 0) --game->animation_frame;
    else if (game->spawn_frame > 0) --game->spawn_frame;
}

bool game_is_animating(const GameState *game) {
    return game->animation_frame > 0;
}
