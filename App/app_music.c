#include "app_music.h"

#include <stddef.h>
#include <string.h>

#include "app_audio.h"
#include "app_ui.h"

static uint8_t g_music_active;
static uint32_t g_music_revision;
static app_audio_snapshot_t g_music_snapshot;

static void app_music_redraw(void)
{
    app_audio_snapshot_t snapshot;

    if (!g_music_active)
    {
        return;
    }
    app_audio_get_snapshot(&snapshot);
    g_music_revision = snapshot.revision;
    app_ui_show_music(&snapshot);
    g_music_snapshot = snapshot;
}

void app_music_open(void)
{
    app_audio_snapshot_t snapshot;

    g_music_active = 1U;
    g_music_revision = 0xFFFFFFFFU;
    app_audio_get_snapshot(&snapshot);
    if (snapshot.state == APP_AUDIO_STATE_NO_TRACKS ||
        snapshot.state == APP_AUDIO_STATE_ERROR)
    {
        (void)app_audio_submit(APP_AUDIO_COMMAND_RESCAN);
    }
    app_music_redraw();
}

void app_music_close(void)
{
    g_music_active = 0U;
}

void app_music_handle_event(const app_input_event_t *event)
{
    app_ui_music_action_t action;
    app_audio_command_t command;

    if (!g_music_active || event == NULL ||
        event->type != APP_INPUT_EVENT_DOWN)
    {
        return;
    }

    action = app_ui_music_action_at(event->x, event->y);
    if (action == APP_UI_MUSIC_ACTION_NONE)
    {
        return;
    }
    if (action == APP_UI_MUSIC_ACTION_PREVIOUS)
    {
        command = APP_AUDIO_COMMAND_PREVIOUS;
    }
    else if (action == APP_UI_MUSIC_ACTION_PLAY_PAUSE)
    {
        command = APP_AUDIO_COMMAND_PLAY_PAUSE;
    }
    else if (action == APP_UI_MUSIC_ACTION_STOP)
    {
        command = APP_AUDIO_COMMAND_STOP;
    }
    else if (action == APP_UI_MUSIC_ACTION_NEXT)
    {
        command = APP_AUDIO_COMMAND_NEXT;
    }
    else if (action == APP_UI_MUSIC_ACTION_TEST_TONE)
    {
        command = APP_AUDIO_COMMAND_TEST_TONE;
    }
    else
    {
        command = APP_AUDIO_COMMAND_RESCAN;
    }
    (void)app_audio_submit(command);
}

void app_music_update(void)
{
    app_audio_snapshot_t snapshot;

    if (!g_music_active)
    {
        return;
    }
    app_audio_get_snapshot(&snapshot);
    if (snapshot.revision != g_music_revision)
    {
        g_music_revision = snapshot.revision;
        if (snapshot.track_count != g_music_snapshot.track_count ||
            snapshot.selected_track != g_music_snapshot.selected_track ||
            snapshot.channels != g_music_snapshot.channels ||
            snapshot.bits_per_sample != g_music_snapshot.bits_per_sample ||
            snapshot.sample_rate != g_music_snapshot.sample_rate ||
            snapshot.data_size != g_music_snapshot.data_size ||
            strcmp(snapshot.track_name, g_music_snapshot.track_name) != 0)
        {
            app_ui_show_music(&snapshot);
        }
        else
        {
            app_ui_update_music(&snapshot, &g_music_snapshot);
        }
        g_music_snapshot = snapshot;
    }
}
