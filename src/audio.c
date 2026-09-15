#include "audio.h"

#include <pspaudio.h>
#include <stdint.h>
#include <string.h>

#define AUDIO_SAMPLES 256

static int channel = -1;
static int sound_enabled = 1;
static int16_t click_buffer[AUDIO_SAMPLES * 2];
static int16_t merge_buffer[AUDIO_SAMPLES * 2];

static void build_tone(int16_t *buffer, int period, int amplitude) {
    int i;
    for (i = 0; i < AUDIO_SAMPLES; ++i) {
        const int envelope = AUDIO_SAMPLES - i;
        const int phase = i % period;
        const int wave = phase < period / 2 ? amplitude : -amplitude;
        const int16_t sample = (int16_t)((wave * envelope) / AUDIO_SAMPLES);
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }
}

static void play(const int16_t *buffer) {
    if (!sound_enabled || channel < 0) return;
    sceAudioOutputPannedBlocking(channel, PSP_AUDIO_VOLUME_MAX / 3,
                                 PSP_AUDIO_VOLUME_MAX / 3, (void *)buffer);
}

void audio_init(void) {
    build_tone(click_buffer, 24, 6000);
    build_tone(merge_buffer, 16, 8000);
    channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
}

void audio_shutdown(void) {
    if (channel >= 0) sceAudioChRelease(channel);
    channel = -1;
}

void audio_set_enabled(int enabled) { sound_enabled = enabled != 0; }
void audio_play_move(void) { play(click_buffer); }
void audio_play_merge(void) { play(merge_buffer); }
void audio_play_select(void) { play(merge_buffer); }
void audio_play_back(void) { play(click_buffer); }
