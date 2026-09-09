/**
 * @file app_music.c
 * @brief 实现音乐应用的曲目选择、播放控制和界面联动。
 * @details 这是 app_music 模块的实现文件（App/app_music.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_music.h"

#include <stddef.h>
#include <string.h>

#include "app_audio.h"
#include "app_settings.h"
#include "app_ui.h"

static uint8_t g_music_active;
static uint32_t g_music_revision;
static app_audio_snapshot_t g_music_snapshot;

/**
 * @brief app_music_redraw：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_music_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_music_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_music_close(void)
{
    g_music_active = 0U;
}

/**
 * @brief app_music_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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
    else if (action == APP_UI_MUSIC_ACTION_SEEK_BACK)
    {
        command = APP_AUDIO_COMMAND_SEEK_BACK_5S;
    }
    else if (action == APP_UI_MUSIC_ACTION_SEEK_FORWARD)
    {
        command = APP_AUDIO_COMMAND_SEEK_FORWARD_5S;
    }
    else if (action == APP_UI_MUSIC_ACTION_VOLUME_DOWN ||
             action == APP_UI_MUSIC_ACTION_VOLUME_UP)
    {
        uint8_t volume;
        volume = app_settings_get_volume_percent();
        if (action == APP_UI_MUSIC_ACTION_VOLUME_DOWN)
        {
            volume = (volume >= 25U) ? (uint8_t)(volume - 25U) : 0U;
        }
        else
        {
            volume = (volume <= 75U) ? (uint8_t)(volume + 25U) : 100U;
        }
        (void)app_settings_set_volume_percent(volume);
        return;
    }
    else
    {
        command = APP_AUDIO_COMMAND_RESCAN;
    }
    (void)app_audio_submit(command);
}

/**
 * @brief app_music_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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
