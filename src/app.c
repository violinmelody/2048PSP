#include "app.h"

#include <pspctrl.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <time.h>

#include "audio.h"
#include "game.h"
#include "renderer.h"
#include "storage.h"

static int exit_requested = 0;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    exit_requested = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    int callback_id;
    (void)args; (void)argp;
    callback_id = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(callback_id);
    sceKernelSleepThreadCB();
    return 0;
}

static void register_callbacks(void) {
    const int thread_id = sceKernelCreateThread("Callback Thread", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (thread_id >= 0) sceKernelStartThread(thread_id, 0, NULL);
}

static void persist_score(const GameState *game) {
    storage_save_score(game->best_score);
}

static void persist_settings(const UiState *ui) {
    const Settings settings = {ui->accent, ui->sound_enabled};
    storage_save_settings(&settings);
}

static void go_home(GameState *game, UiState *ui) {
    game->paused = false;
    ui->pause_confirm_quit = 0;
    ui->pause_selection = 0;
    ui->screen = SCREEN_HOME;
}

void app_run(void) {
    GameState game;
    UiState ui = {SCREEN_HOME, 0, 0, 0, 1, 0, 0, 0};
    Settings settings;
    SceCtrlData pad;
    SceCtrlData previous = {0};
    int analog_latched = 0;

    register_callbacks();
    srand((unsigned)time(NULL));
    storage_load_settings(&settings);
    game_init(&game);
    game.best_score = storage_load_score();
    ui.accent = settings.accent;
    ui.sound_enabled = settings.sound_enabled;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    renderer_init();
    audio_init();
    audio_set_enabled(ui.sound_enabled);

    while (!exit_requested) {
        unsigned int pressed;
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~previous.Buttons;

        if (ui.screen == SCREEN_HOME) {
            if (pressed & PSP_CTRL_UP) { ui.home_selection = (ui.home_selection + 4) % 5; audio_play_move(); }
            if (pressed & PSP_CTRL_DOWN) { ui.home_selection = (ui.home_selection + 1) % 5; audio_play_move(); }
            if (pressed & PSP_CTRL_CROSS) {
                audio_play_select();
                if (ui.home_selection == 0) { game_reset(&game); ui.screen = SCREEN_GAME; }
                else if (ui.home_selection == 1) ui.screen = SCREEN_SETTINGS;
                else if (ui.home_selection == 2) ui.screen = SCREEN_CONTROLS;
                else if (ui.home_selection == 3) ui.screen = SCREEN_ABOUT;
                else { ui.quit_selection = 0; ui.screen = SCREEN_QUIT_CONFIRM; }
            }
        } else if (ui.screen == SCREEN_GAME) {
            if (game.paused) {
                if (ui.pause_confirm_quit) {
                    if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) { ui.quit_selection ^= 1; audio_play_move(); }
                    if (pressed & PSP_CTRL_CIRCLE) { ui.pause_confirm_quit = 0; audio_play_back(); }
                    if (pressed & PSP_CTRL_CROSS) {
                        audio_play_select();
                        if (ui.quit_selection == 1) go_home(&game, &ui);
                        else ui.pause_confirm_quit = 0;
                    }
                } else {
                    if (pressed & PSP_CTRL_UP) { ui.pause_selection = (ui.pause_selection + 3) % 4; audio_play_move(); }
                    if (pressed & PSP_CTRL_DOWN) { ui.pause_selection = (ui.pause_selection + 1) % 4; audio_play_move(); }
                    if (pressed & PSP_CTRL_START) { game.paused = false; audio_play_back(); }
                    if (pressed & PSP_CTRL_CROSS) {
                        audio_play_select();
                        if (ui.pause_selection == 0) {
                            game.paused = false;
                        } else if (ui.pause_selection == 1) {
                            game_reset(&game);
                            game.paused = false;
                        } else if (ui.pause_selection == 2) {
                            game.paused = false;
                            game_undo(&game);
                            audio_play_back();
                        } else {
                            ui.quit_selection = 0;
                            ui.pause_confirm_quit = 1;
                        }
                    }
                }
            } else {
                if (pressed & PSP_CTRL_START) { game.paused = true; ui.pause_selection = 0; audio_play_select(); }
                else if ((pressed & PSP_CTRL_CROSS) && game.won) { game.won = false; audio_play_select(); }
                else if ((pressed & PSP_CTRL_CROSS) && game_is_over(&game)) { game_reset(&game); audio_play_select(); }

                if (!game.paused && !game.won && !game_is_over(&game) && !game_is_animating(&game)) {
                    const int analog_active = pad.Ly < 55 || pad.Ly > 200 || pad.Lx < 55 || pad.Lx > 200;
                    if (!analog_active) analog_latched = 0;
                    if (!analog_latched) {
                        bool moved = false;
                        if ((pressed & PSP_CTRL_UP) || pad.Ly < 55) moved = game_move(&game, 0);
                        else if ((pressed & PSP_CTRL_DOWN) || pad.Ly > 200) moved = game_move(&game, 1);
                        else if ((pressed & PSP_CTRL_LEFT) || pad.Lx < 55) moved = game_move(&game, 2);
                        else if ((pressed & PSP_CTRL_RIGHT) || pad.Lx > 200) moved = game_move(&game, 3);
                        if (analog_active) analog_latched = 1;
                        if (moved) {
                            int i;
                            int merged = 0;
                            for (i = 0; i < game.motion_count; ++i) {
                                if (game.motions[i].merged) { merged = 1; break; }
                            }
                            if (merged) audio_play_merge(); else audio_play_move();
                            if (game.score == game.best_score) persist_score(&game);
                        }
                    }
                }
            }
        } else if (ui.screen == SCREEN_SETTINGS) {
            if (pressed & PSP_CTRL_CIRCLE) { persist_settings(&ui); ui.screen = SCREEN_HOME; audio_play_back(); }
            if (pressed & PSP_CTRL_UP) { ui.settings_selection = (ui.settings_selection + 2) % 3; audio_play_move(); }
            if (pressed & PSP_CTRL_DOWN) { ui.settings_selection = (ui.settings_selection + 1) % 3; audio_play_move(); }
            if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
                if (ui.settings_selection == 0) ui.accent = (ui.accent + 1) % 4;
                else if (ui.settings_selection == 1) { ui.sound_enabled = !ui.sound_enabled; audio_set_enabled(ui.sound_enabled); }
                else game.best_score = 0;
                if (ui.settings_selection == 2) persist_score(&game);
                else persist_settings(&ui);
                audio_play_select();
            }
        } else if (ui.screen == SCREEN_QUIT_CONFIRM) {
            if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) { ui.quit_selection ^= 1; audio_play_move(); }
            if (pressed & PSP_CTRL_CIRCLE) { ui.screen = SCREEN_HOME; audio_play_back(); }
            if (pressed & PSP_CTRL_CROSS) {
                audio_play_select();
                if (ui.quit_selection == 1) exit_requested = 1;
                else ui.screen = SCREEN_HOME;
            }
        } else if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)) {
            ui.screen = SCREEN_HOME;
            audio_play_back();
        }

        renderer_draw(&game, &ui);
        game_tick_animation(&game);
        previous = pad;
    }

    persist_score(&game);
    persist_settings(&ui);
    audio_shutdown();
    renderer_shutdown();
    sceKernelExitGame();
}
