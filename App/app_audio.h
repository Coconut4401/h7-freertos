#ifndef APP_AUDIO_H
#define APP_AUDIO_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "app_storage.h"

typedef enum
{
    APP_AUDIO_STATE_STARTING = 0,
    APP_AUDIO_STATE_STOPPED,
    APP_AUDIO_STATE_PLAYING,
    APP_AUDIO_STATE_PAUSED,
    APP_AUDIO_STATE_TEST_TONE,
    APP_AUDIO_STATE_NO_TRACKS,
    APP_AUDIO_STATE_ERROR
} app_audio_state_t;

typedef enum
{
    APP_AUDIO_COMMAND_RESCAN = 0,
    APP_AUDIO_COMMAND_PLAY_PAUSE,
    APP_AUDIO_COMMAND_STOP,
    APP_AUDIO_COMMAND_PREVIOUS,
    APP_AUDIO_COMMAND_NEXT,
    APP_AUDIO_COMMAND_TEST_TONE
} app_audio_command_t;

typedef struct
{
    uint32_t revision;
    app_audio_state_t state;
    uint8_t track_count;
    uint8_t selected_track;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t volume_percent;
    uint32_t sample_rate;
    uint32_t data_size;
    uint32_t data_loaded;
    char track_name[APP_STORAGE_NAME_LENGTH];
    char status[48];
} app_audio_snapshot_t;

BaseType_t app_audio_init(void);
BaseType_t app_audio_submit(app_audio_command_t command);
void app_audio_set_volume(uint8_t volume_percent);
uint8_t app_audio_get_volume(void);
void app_audio_get_snapshot(app_audio_snapshot_t *snapshot);
void AppAudioTask(void *argument);
void DMA1_Stream0_IRQHandler(void);

#endif
