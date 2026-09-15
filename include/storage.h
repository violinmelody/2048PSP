#ifndef FLUENT2048_STORAGE_H
#define FLUENT2048_STORAGE_H

typedef struct {
    int accent;
    int sound_enabled;
} Settings;

int storage_load_score(void);
void storage_save_score(int best_score);
void storage_load_settings(Settings *settings);
void storage_save_settings(const Settings *settings);

#endif
