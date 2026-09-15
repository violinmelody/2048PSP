#ifndef FLUENT2048_AUDIO_H
#define FLUENT2048_AUDIO_H

void audio_init(void);
void audio_shutdown(void);
void audio_set_enabled(int enabled);
void audio_play_move(void);
void audio_play_merge(void);
void audio_play_select(void);
void audio_play_back(void);

#endif
