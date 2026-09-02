#include "app_audio.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "queue.h"
#include "task.h"
#include "app_logs.h"
#include "app_health.h"
#include "./SYSTEM/sys/sys.h"

#define AUDIO_COMMAND_QUEUE_LENGTH       6U
#define AUDIO_DMA_SAMPLE_COUNT           65504U
#define AUDIO_DMA_HALF_SAMPLES           (AUDIO_DMA_SAMPLE_COUNT / 2U)
#define AUDIO_RAW_BUFFER_SIZE            (AUDIO_DMA_HALF_SAMPLES * 2U)
#define AUDIO_STORAGE_TIMEOUT_MS         2000U
#define AUDIO_PROGRESS_UPDATE_MS         250U
#define AUDIO_TEST_RATE                  48000U
#define AUDIO_TEST_SECONDS               2U
#define AUDIO_TEST_FREQUENCY             440U
#define AUDIO_DMA_NOTIFY_HALF0           (1UL << 0)
#define AUDIO_DMA_NOTIFY_HALF1           (1UL << 1)
#define AUDIO_DMA_NOTIFY_ERROR           (1UL << 2)
#define AUDIO_DMA_ALL_FLAGS              (DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | \
                                          DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0 | \
                                          DMA_LIFCR_CTCIF0)

static QueueHandle_t g_audio_command_queue;
static TaskHandle_t g_audio_task_handle;
static app_audio_snapshot_t g_audio_public;
static app_audio_snapshot_t g_audio_work;
static char g_audio_tracks[APP_STORAGE_AUDIO_MAX_TRACKS][APP_STORAGE_NAME_LENGTH];
static volatile uint8_t g_audio_volume = 50U;
static uint8_t g_audio_file_open;
static uint8_t g_audio_stop_after_half;
static app_audio_state_t g_audio_paused_state;
static uint32_t g_audio_test_frames_remaining;
static uint32_t g_audio_test_phase;
static TickType_t g_audio_last_progress_publish;

static int16_t g_audio_dma_buffer[AUDIO_DMA_SAMPLE_COUNT]
    __attribute__((aligned(32)));
static uint8_t g_audio_raw_buffer[AUDIO_RAW_BUFFER_SIZE]
    __attribute__((aligned(32)));

static void app_audio_clean_dma_half(uint8_t half)
{
    int16_t *address;

    address = &g_audio_dma_buffer[(uint32_t)half * AUDIO_DMA_HALF_SAMPLES];
    SCB_CleanDCache_by_Addr((uint32_t *)address,
                           AUDIO_DMA_HALF_SAMPLES * sizeof(int16_t));
    __DSB();
}

static void app_audio_copy_text(char *destination,
                                uint32_t destination_size,
                                const char *source)
{
    uint32_t index;

    if (destination == NULL || destination_size == 0U)
    {
        return;
    }
    index = 0U;
    if (source != NULL)
    {
        while (source[index] != '\0' && index + 1U < destination_size)
        {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static void app_audio_publish(const char *status)
{
    if (status != NULL)
    {
        app_audio_copy_text(g_audio_work.status,
                            sizeof(g_audio_work.status), status);
    }
    g_audio_work.volume_percent = g_audio_volume;
    taskENTER_CRITICAL();
    g_audio_work.revision = g_audio_public.revision + 1U;
    g_audio_public = g_audio_work;
    taskEXIT_CRITICAL();
}

static void app_audio_gpio_init(void)
{
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC->AHB4ENR;
    sys_gpio_af_set(GPIOB, SYS_GPIO_PIN12 | SYS_GPIO_PIN13 | SYS_GPIO_PIN15, 5U);
    sys_gpio_set(GPIOB, SYS_GPIO_PIN12 | SYS_GPIO_PIN13 | SYS_GPIO_PIN15,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP,
                 SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_NONE);
}

static void app_audio_peripheral_init(void)
{
    app_audio_gpio_init();
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1LENR |= RCC_APB1LENR_SPI2EN;
    (void)RCC->APB1LENR;

    RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL) |
                    RCC_D2CCIP1R_SPI123SEL_0;
    RCC->APB1LRSTR |= RCC_APB1LRSTR_SPI2RST;
    RCC->APB1LRSTR &= ~RCC_APB1LRSTR_SPI2RST;

    DMA1_Stream0->CR = 0U;
    DMA1_Stream0->FCR = 0U;
    DMA1->LIFCR = AUDIO_DMA_ALL_FLAGS;
    DMAMUX1_Channel0->CCR = 40U;

    NVIC_SetPriority(DMA1_Stream0_IRQn, 6U);
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

static void app_audio_hardware_stop(void)
{
    SPI2->CFG1 &= ~SPI_CFG1_TXDMAEN;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream0->CR & DMA_SxCR_EN) != 0U)
    {
    }
    DMA1->LIFCR = AUDIO_DMA_ALL_FLAGS;
    SPI2->IFCR = 0x0FF8U;
}

static uint8_t app_audio_hardware_configure(uint32_t sample_rate)
{
    uint32_t divider;
    uint32_t odd;
    uint32_t i2sdiv;

    divider = (220000000U + (16U * sample_rate)) /
              (32U * sample_rate);
    odd = divider & 1U;
    i2sdiv = (divider - odd) / 2U;
    if (i2sdiv == 0U || i2sdiv > 255U ||
        (i2sdiv == 1U && odd != 0U))
    {
        return 0U;
    }

    app_audio_hardware_stop();
    SPI2->CR1 = 0U;
    SPI2->CFG1 = 0U;
    SPI2->CFG2 = 0U;
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD |
                    SPI_I2SCFGR_I2SCFG_1 |
                    (i2sdiv << SPI_I2SCFGR_I2SDIV_Pos) |
                    (odd << SPI_I2SCFGR_ODD_Pos);
    SPI2->IFCR = 0x0FF8U;
    return 1U;
}

static void app_audio_hardware_start(void)
{
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream0->CR & DMA_SxCR_EN) != 0U)
    {
    }
    DMA1->LIFCR = AUDIO_DMA_ALL_FLAGS;
    DMAMUX1_Channel0->CCR = 40U;
    DMA1_Stream0->PAR = (uint32_t)&SPI2->TXDR;
    DMA1_Stream0->M0AR = (uint32_t)g_audio_dma_buffer;
    DMA1_Stream0->NDTR = AUDIO_DMA_SAMPLE_COUNT;
    DMA1_Stream0->FCR = 0U;
    DMA1_Stream0->CR = DMA_SxCR_DIR_0 | DMA_SxCR_MINC |
                       DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_PL_1 | DMA_SxCR_CIRC |
                       DMA_SxCR_HTIE | DMA_SxCR_TCIE |
                       DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
    SCB_CleanDCache_by_Addr((uint32_t *)g_audio_dma_buffer,
                           sizeof(g_audio_dma_buffer));
    __DSB();
    DMA1_Stream0->CR |= DMA_SxCR_EN;
    SPI2->CFG1 |= SPI_CFG1_TXDMAEN;
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_CSTART;
}

static void app_audio_hardware_pause(void)
{
    uint32_t timeout;

    SPI2->CR1 |= SPI_CR1_CSUSP;
    timeout = 100000U;
    while ((SPI2->CR1 & SPI_CR1_CSTART) != 0U && timeout > 0U)
    {
        timeout--;
    }
    SPI2->CR1 &= ~SPI_CR1_SPE;
}

static void app_audio_hardware_resume(void)
{
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_CSTART;
}

static BaseType_t app_audio_storage_transaction(
    const app_storage_request_t *request,
    app_storage_audio_response_t *response)
{
    TickType_t start;

    start = xTaskGetTickCount();
    while (app_storage_submit(request) != pdPASS)
    {
        if ((TickType_t)(xTaskGetTickCount() - start) >=
            pdMS_TO_TICKS(AUDIO_STORAGE_TIMEOUT_MS))
        {
            return pdFAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(2U));
    }

    while ((TickType_t)(xTaskGetTickCount() - start) <
           pdMS_TO_TICKS(AUDIO_STORAGE_TIMEOUT_MS))
    {
        if (app_storage_receive_audio(response) == pdPASS)
        {
            return pdPASS;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return pdFAIL;
}

static void app_audio_close_file(void)
{
    app_storage_request_t request;
    app_storage_audio_response_t response;

    if (!g_audio_file_open)
    {
        return;
    }
    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_AUDIO_CLOSE;
    (void)app_audio_storage_transaction(&request, &response);
    g_audio_file_open = 0U;
}

static void app_audio_stop(const char *status)
{
    app_audio_hardware_stop();
    app_audio_close_file();
    g_audio_stop_after_half = 0U;
    g_audio_test_frames_remaining = 0U;
    if (g_audio_work.track_count > 0U)
    {
        app_audio_copy_text(g_audio_work.track_name,
                            sizeof(g_audio_work.track_name),
                            g_audio_tracks[g_audio_work.selected_track]);
    }
    else
    {
        g_audio_work.track_name[0] = '\0';
    }
    g_audio_work.state = (g_audio_work.track_count == 0U) ?
                         APP_AUDIO_STATE_NO_TRACKS : APP_AUDIO_STATE_STOPPED;
    app_audio_publish(status);
}

static void app_audio_scan(void)
{
    app_storage_request_t request;
    app_storage_audio_response_t response;
    uint8_t index;

    app_audio_hardware_stop();
    app_audio_close_file();
    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_AUDIO_SCAN;
    g_audio_work.state = APP_AUDIO_STATE_STARTING;
    app_audio_publish("SCANNING SD ROOT FOR WAV FILES");
    if (app_audio_storage_transaction(&request, &response) != pdPASS ||
        response.result != APP_STORAGE_RESULT_OK)
    {
        g_audio_work.track_count = 0U;
        g_audio_work.state = APP_AUDIO_STATE_ERROR;
        app_audio_publish("SD CARD SCAN FAILED");
        app_logs_add(APP_LOG_LEVEL_ERROR, "AUDIO", "WAV SCAN FAILED");
        return;
    }

    memset(g_audio_tracks, 0, sizeof(g_audio_tracks));
    for (index = 0U; index < response.track_count; index++)
    {
        app_audio_copy_text(g_audio_tracks[index], APP_STORAGE_NAME_LENGTH,
                            response.tracks[index]);
    }
    g_audio_work.track_count = response.track_count;
    if (g_audio_work.selected_track >= response.track_count)
    {
        g_audio_work.selected_track = 0U;
    }
    g_audio_work.channels = 0U;
    g_audio_work.bits_per_sample = 0U;
    g_audio_work.sample_rate = 0U;
    g_audio_work.data_size = 0U;
    g_audio_work.data_loaded = 0U;
    if (response.track_count == 0U)
    {
        g_audio_work.track_name[0] = '\0';
        g_audio_work.state = APP_AUDIO_STATE_NO_TRACKS;
        app_audio_publish("NO SUPPORTED WAV FILES IN SD ROOT");
    }
    else
    {
        app_audio_copy_text(g_audio_work.track_name,
                            sizeof(g_audio_work.track_name),
                            g_audio_tracks[g_audio_work.selected_track]);
        g_audio_work.state = APP_AUDIO_STATE_STOPPED;
        app_audio_publish("READY - TOUCH PLAY OR TEST TONE");
    }
}

static int8_t app_audio_fill_file_half(uint8_t half)
{
    app_storage_request_t request;
    app_storage_audio_response_t response;
    int16_t *destination;
    const int16_t *source;
    uint32_t source_samples;
    uint32_t index;
    int32_t sample;

    destination = &g_audio_dma_buffer[(uint32_t)half * AUDIO_DMA_HALF_SAMPLES];
    memset(destination, 0, AUDIO_DMA_HALF_SAMPLES * sizeof(int16_t));
    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_AUDIO_READ;
    request.binary_data = g_audio_raw_buffer;
    request.binary_capacity = (g_audio_work.channels == 1U) ?
                              AUDIO_DMA_HALF_SAMPLES : AUDIO_RAW_BUFFER_SIZE;
    if (app_audio_storage_transaction(&request, &response) != pdPASS ||
        response.result != APP_STORAGE_RESULT_OK)
    {
        return -1;
    }

    source = (const int16_t *)g_audio_raw_buffer;
    source_samples = response.data_length / 2U;
    if (g_audio_work.channels == 1U)
    {
        if (source_samples > AUDIO_DMA_HALF_SAMPLES / 2U)
        {
            source_samples = AUDIO_DMA_HALF_SAMPLES / 2U;
        }
        for (index = 0U; index < source_samples; index++)
        {
            sample = ((int32_t)source[index] * g_audio_volume) / 100;
            destination[index * 2U] = (int16_t)sample;
            destination[index * 2U + 1U] = (int16_t)sample;
        }
    }
    else
    {
        if (source_samples > AUDIO_DMA_HALF_SAMPLES)
        {
            source_samples = AUDIO_DMA_HALF_SAMPLES;
        }
        for (index = 0U; index < source_samples; index++)
        {
            sample = ((int32_t)source[index] * g_audio_volume) / 100;
            destination[index] = (int16_t)sample;
        }
    }
    g_audio_work.data_loaded += response.data_length;
    app_audio_clean_dma_half(half);
    return response.end_of_file ? 1 : 0;
}

static uint8_t app_audio_fill_test_half(uint8_t half)
{
    int16_t *destination;
    uint32_t frames;
    uint32_t frame;
    uint32_t phase_step;
    int32_t sample;

    destination = &g_audio_dma_buffer[(uint32_t)half * AUDIO_DMA_HALF_SAMPLES];
    memset(destination, 0, AUDIO_DMA_HALF_SAMPLES * sizeof(int16_t));
    frames = AUDIO_DMA_HALF_SAMPLES / 2U;
    if (frames > g_audio_test_frames_remaining)
    {
        frames = g_audio_test_frames_remaining;
    }
    phase_step = (uint32_t)(((uint64_t)AUDIO_TEST_FREQUENCY << 32U) /
                            AUDIO_TEST_RATE);
    for (frame = 0U; frame < frames; frame++)
    {
        g_audio_test_phase += phase_step;
        sample = ((g_audio_test_phase & 0x80000000U) != 0U) ? 6000 : -6000;
        sample = (sample * g_audio_volume) / 100;
        destination[frame * 2U] = (int16_t)sample;
        destination[frame * 2U + 1U] = (int16_t)sample;
    }
    g_audio_test_frames_remaining -= frames;
    g_audio_work.data_loaded += frames * 4U;
    app_audio_clean_dma_half(half);
    return (g_audio_test_frames_remaining == 0U) ? 1U : 0U;
}

static void app_audio_start_file(void)
{
    app_storage_request_t request;
    app_storage_audio_response_t response;
    int8_t end_of_file;

    if (g_audio_work.track_count == 0U)
    {
        app_audio_publish("NO WAV TRACK SELECTED");
        return;
    }
    app_audio_hardware_stop();
    app_audio_close_file();
    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_AUDIO_OPEN;
    app_audio_copy_text(request.name, sizeof(request.name),
                        g_audio_tracks[g_audio_work.selected_track]);
    if (app_audio_storage_transaction(&request, &response) != pdPASS ||
        response.result != APP_STORAGE_RESULT_OK)
    {
        g_audio_work.state = APP_AUDIO_STATE_ERROR;
        app_audio_publish("UNSUPPORTED OR UNREADABLE WAV FILE");
        app_logs_add(APP_LOG_LEVEL_ERROR, "AUDIO", "WAV OPEN FAILED");
        return;
    }
    g_audio_file_open = 1U;
    g_audio_work.channels = response.channels;
    g_audio_work.bits_per_sample = response.bits_per_sample;
    g_audio_work.sample_rate = response.sample_rate;
    g_audio_work.data_size = response.data_size;
    g_audio_work.data_loaded = 0U;
    app_audio_copy_text(g_audio_work.track_name,
                        sizeof(g_audio_work.track_name),
                        g_audio_tracks[g_audio_work.selected_track]);
    if (!app_audio_hardware_configure(response.sample_rate))
    {
        app_audio_stop("I2S SAMPLE RATE CONFIGURATION FAILED");
        g_audio_work.state = APP_AUDIO_STATE_ERROR;
        app_audio_publish(NULL);
        return;
    }

    g_audio_stop_after_half = 0U;
    end_of_file = app_audio_fill_file_half(0U);
    if (end_of_file < 0)
    {
        app_audio_stop("SD READ FAILED");
        return;
    }
    if (end_of_file > 0)
    {
        memset(&g_audio_dma_buffer[AUDIO_DMA_HALF_SAMPLES], 0,
               AUDIO_DMA_HALF_SAMPLES * sizeof(int16_t));
        app_audio_clean_dma_half(1U);
        g_audio_stop_after_half = 1U;
    }
    else
    {
        end_of_file = app_audio_fill_file_half(1U);
        if (end_of_file < 0)
        {
            app_audio_stop("SD READ FAILED");
            return;
        }
        if (end_of_file > 0)
        {
            g_audio_stop_after_half = 2U;
        }
    }
    g_audio_work.state = APP_AUDIO_STATE_PLAYING;
    g_audio_last_progress_publish = xTaskGetTickCount();
    app_audio_publish("PLAYING WAV FROM SD CARD");
    app_logs_add(APP_LOG_LEVEL_INFO, "AUDIO", "WAV PLAYBACK STARTED");
    app_audio_hardware_start();
}

static void app_audio_start_test(void)
{
    app_audio_hardware_stop();
    app_audio_close_file();
    g_audio_test_frames_remaining = AUDIO_TEST_RATE * AUDIO_TEST_SECONDS;
    g_audio_test_phase = 0U;
    g_audio_stop_after_half = 0U;
    g_audio_work.channels = 2U;
    g_audio_work.bits_per_sample = 16U;
    g_audio_work.sample_rate = AUDIO_TEST_RATE;
    g_audio_work.data_size = AUDIO_TEST_RATE * AUDIO_TEST_SECONDS * 4U;
    g_audio_work.data_loaded = 0U;
    app_audio_copy_text(g_audio_work.track_name,
                        sizeof(g_audio_work.track_name), "440HZ TEST");
    if (!app_audio_hardware_configure(AUDIO_TEST_RATE))
    {
        g_audio_work.state = APP_AUDIO_STATE_ERROR;
        app_audio_publish("I2S TEST TONE CONFIGURATION FAILED");
        return;
    }
    if (app_audio_fill_test_half(0U))
    {
        g_audio_stop_after_half = 1U;
    }
    if (g_audio_stop_after_half == 0U && app_audio_fill_test_half(1U))
    {
        g_audio_stop_after_half = 2U;
    }
    g_audio_work.state = APP_AUDIO_STATE_TEST_TONE;
    g_audio_last_progress_publish = xTaskGetTickCount();
    app_audio_publish("PLAYING 440 HZ HARDWARE TEST TONE");
    app_logs_add(APP_LOG_LEVEL_INFO, "AUDIO", "TEST TONE STARTED");
    app_audio_hardware_start();
}

static void app_audio_service_half(uint8_t half)
{
    int8_t end_of_file;
    uint8_t half_mask;

    half_mask = (uint8_t)(1U << half);
    if ((g_audio_stop_after_half & half_mask) != 0U)
    {
        app_audio_stop("PLAYBACK COMPLETE");
        app_logs_add(APP_LOG_LEVEL_INFO, "AUDIO", "PLAYBACK COMPLETE");
        return;
    }

    if (g_audio_work.state == APP_AUDIO_STATE_TEST_TONE)
    {
        if (app_audio_fill_test_half(half))
        {
            g_audio_stop_after_half |= half_mask;
        }
    }
    else if (g_audio_work.state == APP_AUDIO_STATE_PLAYING)
    {
        end_of_file = app_audio_fill_file_half(half);
        if (end_of_file < 0)
        {
            app_audio_stop("SD READ FAILED DURING PLAYBACK");
        }
        else if (end_of_file > 0)
        {
            g_audio_stop_after_half |= half_mask;
        }
    }
}

static void app_audio_select(int8_t direction)
{
    uint8_t restart;

    if (g_audio_work.track_count == 0U)
    {
        app_audio_publish("NO WAV TRACKS TO SELECT");
        return;
    }
    restart = (g_audio_work.state == APP_AUDIO_STATE_PLAYING ||
               g_audio_work.state == APP_AUDIO_STATE_PAUSED) ? 1U : 0U;
    app_audio_stop("TRACK SELECTED");
    if (direction < 0)
    {
        g_audio_work.selected_track = (g_audio_work.selected_track == 0U) ?
            (uint8_t)(g_audio_work.track_count - 1U) :
            (uint8_t)(g_audio_work.selected_track - 1U);
    }
    else
    {
        g_audio_work.selected_track =
            (uint8_t)((g_audio_work.selected_track + 1U) %
                      g_audio_work.track_count);
    }
    app_audio_copy_text(g_audio_work.track_name,
                        sizeof(g_audio_work.track_name),
                        g_audio_tracks[g_audio_work.selected_track]);
    app_audio_publish("TRACK SELECTED");
    if (restart)
    {
        app_audio_start_file();
    }
}

static void app_audio_process_command(app_audio_command_t command)
{
    if (command == APP_AUDIO_COMMAND_RESCAN)
    {
        app_audio_scan();
    }
    else if (command == APP_AUDIO_COMMAND_STOP)
    {
        app_audio_stop("STOPPED");
    }
    else if (command == APP_AUDIO_COMMAND_PREVIOUS)
    {
        app_audio_select(-1);
    }
    else if (command == APP_AUDIO_COMMAND_NEXT)
    {
        app_audio_select(1);
    }
    else if (command == APP_AUDIO_COMMAND_TEST_TONE)
    {
        app_audio_start_test();
    }
    else if (command == APP_AUDIO_COMMAND_PLAY_PAUSE)
    {
        if (g_audio_work.state == APP_AUDIO_STATE_PLAYING ||
            g_audio_work.state == APP_AUDIO_STATE_TEST_TONE)
        {
            g_audio_paused_state = g_audio_work.state;
            app_audio_hardware_pause();
            g_audio_work.state = APP_AUDIO_STATE_PAUSED;
            app_audio_publish("PAUSED");
        }
        else if (g_audio_work.state == APP_AUDIO_STATE_PAUSED)
        {
            app_audio_hardware_resume();
            g_audio_work.state = g_audio_paused_state;
            app_audio_publish(g_audio_paused_state == APP_AUDIO_STATE_TEST_TONE ?
                              "TEST TONE RESUMED" : "PLAYBACK RESUMED");
        }
        else
        {
            app_audio_start_file();
        }
    }
}

BaseType_t app_audio_init(void)
{
    memset(&g_audio_public, 0, sizeof(g_audio_public));
    memset(&g_audio_work, 0, sizeof(g_audio_work));
    g_audio_work.state = APP_AUDIO_STATE_STARTING;
    g_audio_work.volume_percent = g_audio_volume;
    app_audio_copy_text(g_audio_work.status, sizeof(g_audio_work.status),
                        "AUDIO TASK STARTING");
    g_audio_public = g_audio_work;
    g_audio_command_queue = xQueueCreate(AUDIO_COMMAND_QUEUE_LENGTH,
                                         sizeof(app_audio_command_t));
    if (g_audio_command_queue == NULL)
    {
        return pdFAIL;
    }
    vQueueAddToRegistry(g_audio_command_queue, "AudioCommands");
    app_audio_peripheral_init();
    return pdPASS;
}

BaseType_t app_audio_submit(app_audio_command_t command)
{
    if (g_audio_command_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueSend(g_audio_command_queue, &command, 0U);
}

void app_audio_set_volume(uint8_t volume_percent)
{
    if (volume_percent > 100U)
    {
        volume_percent = 100U;
    }
    taskENTER_CRITICAL();
    g_audio_volume = volume_percent;
    g_audio_public.volume_percent = volume_percent;
    g_audio_public.revision++;
    taskEXIT_CRITICAL();
}

uint8_t app_audio_get_volume(void)
{
    return g_audio_volume;
}

void app_audio_get_snapshot(app_audio_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    *snapshot = g_audio_public;
    taskEXIT_CRITICAL();
}

void AppAudioTask(void *argument)
{
    app_audio_command_t command;
    uint32_t notifications;
    TickType_t now;

    (void)argument;
    g_audio_task_handle = xTaskGetCurrentTaskHandle();
    app_logs_add(APP_LOG_LEVEL_INFO, "AUDIO", "AUDIO TASK STARTED");
    app_audio_scan();

    while (1)
    {
        app_health_beat(APP_HEALTH_AUDIO);
        while (xQueueReceive(g_audio_command_queue, &command, 0U) == pdPASS)
        {
            app_audio_process_command(command);
        }

        notifications = 0U;
        (void)xTaskNotifyWait(0U, 0xFFFFFFFFU, &notifications,
                              pdMS_TO_TICKS(10U));
        if ((notifications & AUDIO_DMA_NOTIFY_ERROR) != 0U)
        {
            app_audio_stop("I2S DMA ERROR");
            app_logs_add(APP_LOG_LEVEL_ERROR, "AUDIO", "DMA ERROR");
        }
        else
        {
            if ((notifications & AUDIO_DMA_NOTIFY_HALF0) != 0U)
            {
                app_audio_service_half(0U);
            }
            if ((notifications & AUDIO_DMA_NOTIFY_HALF1) != 0U &&
                (g_audio_work.state == APP_AUDIO_STATE_PLAYING ||
                 g_audio_work.state == APP_AUDIO_STATE_TEST_TONE))
            {
                app_audio_service_half(1U);
            }
        }

        now = xTaskGetTickCount();
        if ((g_audio_work.state == APP_AUDIO_STATE_PLAYING ||
             g_audio_work.state == APP_AUDIO_STATE_TEST_TONE) &&
            (TickType_t)(now - g_audio_last_progress_publish) >=
            pdMS_TO_TICKS(AUDIO_PROGRESS_UPDATE_MS))
        {
            g_audio_last_progress_publish = now;
            app_audio_publish(NULL);
        }
    }
}

void DMA1_Stream0_IRQHandler(void)
{
    uint32_t flags;
    uint32_t notification;
    BaseType_t higher_priority_task_woken;

    flags = DMA1->LISR;
    DMA1->LIFCR = AUDIO_DMA_ALL_FLAGS;
    notification = 0U;
    if ((flags & DMA_LISR_HTIF0) != 0U)
    {
        notification |= AUDIO_DMA_NOTIFY_HALF0;
    }
    if ((flags & DMA_LISR_TCIF0) != 0U)
    {
        notification |= AUDIO_DMA_NOTIFY_HALF1;
    }
    if ((flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0)) != 0U)
    {
        notification |= AUDIO_DMA_NOTIFY_ERROR;
    }
    if (notification != 0U && g_audio_task_handle != NULL)
    {
        higher_priority_task_woken = pdFALSE;
        xTaskNotifyFromISR(g_audio_task_handle, notification, eSetBits,
                           &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}
