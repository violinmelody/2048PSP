#include "storage.h"

#include <pspiofilemgr.h>

#define SCORE_PATH "ms0:/PSP/GAME/2048/score.dat"
#define SETTINGS_PATH "ms0:/PSP/GAME/2048/settings.dat"
#define LOCAL_SCORE_PATH "score.dat"
#define LOCAL_SETTINGS_PATH "settings.dat"
#define SCORE_MAGIC 0x53434F52u
#define SETTINGS_MAGIC 0x53455454u
#define DATA_VERSION 1u

typedef struct {
    unsigned int magic;
    unsigned int version;
    int best_score;
} ScoreData;

typedef struct {
    unsigned int magic;
    unsigned int version;
    int accent;
    int sound_enabled;
} SettingsData;

static int open_read(const char *primary, const char *fallback) {
    int fd = sceIoOpen(primary, PSP_O_RDONLY, 0777);
    if (fd < 0) fd = sceIoOpen(fallback, PSP_O_RDONLY, 0777);
    return fd;
}

static int open_write(const char *primary, const char *fallback) {
    int fd = sceIoOpen(primary, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) fd = sceIoOpen(fallback, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    return fd;
}

int storage_load_score(void) {
    ScoreData data = {0};
    const int fd = open_read(SCORE_PATH, LOCAL_SCORE_PATH);
    int bytes_read;

    if (fd < 0) return 0;
    bytes_read = sceIoRead(fd, &data, sizeof(data));
    sceIoClose(fd);

    if (bytes_read != (int)sizeof(data) || data.magic != SCORE_MAGIC ||
        data.version != DATA_VERSION || data.best_score < 0) return 0;
    return data.best_score;
}

void storage_save_score(int best_score) {
    const ScoreData data = {SCORE_MAGIC, DATA_VERSION, best_score > 0 ? best_score : 0};
    const int fd = open_write(SCORE_PATH, LOCAL_SCORE_PATH);
    if (fd < 0) return;
    sceIoWrite(fd, &data, sizeof(data));
    sceIoClose(fd);
}

void storage_load_settings(Settings *settings) {
    SettingsData data = {0};
    const int fd = open_read(SETTINGS_PATH, LOCAL_SETTINGS_PATH);
    int bytes_read;

    settings->accent = 0;
    settings->sound_enabled = 1;
    if (fd < 0) return;

    bytes_read = sceIoRead(fd, &data, sizeof(data));
    sceIoClose(fd);
    if (bytes_read != (int)sizeof(data) || data.magic != SETTINGS_MAGIC || data.version != DATA_VERSION) return;

    settings->accent = data.accent >= 0 ? data.accent % 4 : 0;
    settings->sound_enabled = data.sound_enabled != 0;
}

void storage_save_settings(const Settings *settings) {
    const SettingsData data = {SETTINGS_MAGIC, DATA_VERSION,
                               settings->accent >= 0 ? settings->accent % 4 : 0,
                               settings->sound_enabled != 0};
    const int fd = open_write(SETTINGS_PATH, LOCAL_SETTINGS_PATH);
    if (fd < 0) return;
    sceIoWrite(fd, &data, sizeof(data));
    sceIoClose(fd);
}
