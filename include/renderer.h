#ifndef FLUENT2048_RENDERER_H
#define FLUENT2048_RENDERER_H

#include "game.h"

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_GAME,
    SCREEN_SETTINGS,
    SCREEN_CONTROLS,
    SCREEN_ABOUT,
    SCREEN_QUIT_CONFIRM
} ScreenId;

typedef struct {
    ScreenId screen;
    int home_selection;
    int settings_selection;
    int accent;
    int sound_enabled;
    int pause_selection;
    int pause_confirm_quit;
    int quit_selection;
} UiState;

void renderer_init(void);
void renderer_shutdown(void);
void renderer_draw(const GameState *game, const UiState *ui);

#endif
