#include "app_storage.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "app_logs.h"
#include "ff.h"
#include "./BSP/SDMMC/sdmmc_sdcard.h"

#define STORAGE_REQUEST_QUEUE_LENGTH   4U
#define STORAGE_RESPONSE_QUEUE_LENGTH  2U
#define STORAGE_BINARY_QUEUE_LENGTH    2U
#define STORAGE_LOG_QUEUE_LENGTH       2U

#define STORAGE_BINARY_TEMP_PATH       "0:/~DRAW.TMP"
#define STORAGE_BINARY_BACKUP_PATH     "0:/~DRAW.BAK"
#define STORAGE_LOG_TEMP_PATH          "0:/~LOG.TMP"
#define STORAGE_LOG_BACKUP_PATH        "0:/~LOG.BAK"

static FATFS g_sd_filesystem;
static QueueHandle_t g_request_queue;
static QueueHandle_t g_response_queue;
static QueueHandle_t g_binary_response_queue;
static QueueHandle_t g_log_response_queue;
static volatile app_storage_state_t g_storage_state = APP_STORAGE_STATE_STARTING;
static volatile uint32_t g_capacity_mb;

static void app_storage_copy_text(char *destination,
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

static uint8_t app_storage_name_is_valid(const char *name)
{
    uint32_t index;
    uint32_t length;

    if (name == NULL || name[0] == '\0')
    {
        return 0U;
    }

    length = 0U;
    while (name[length] != '\0' && length < APP_STORAGE_NAME_LENGTH)
    {
        length++;
    }
    if (length == 0U || length >= APP_STORAGE_NAME_LENGTH)
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        if (name[index] == '/' || name[index] == '\\' ||
            name[index] == ':' || name[index] == '*' ||
            name[index] == '?' || name[index] == '"' ||
            name[index] == '<' || name[index] == '>' ||
            name[index] == '|')
        {
            return 0U;
        }
    }

    return 1U;
}

static void app_storage_make_path(char path[APP_STORAGE_PATH_LENGTH],
                                  const char *name)
{
    path[0] = '0';
    path[1] = ':';
    path[2] = '/';
    app_storage_copy_text(&path[3], APP_STORAGE_PATH_LENGTH - 3U, name);
}

static void app_storage_set_file_type(app_storage_file_t *file)
{
    const char *extension;

    extension = strrchr(file->name, '.');
    if (extension != NULL &&
        (strcmp(extension, ".TXT") == 0 || strcmp(extension, ".txt") == 0))
    {
        app_storage_copy_text(file->type, sizeof(file->type), "TEXT");
    }
    else
    {
        app_storage_copy_text(file->type, sizeof(file->type), "DATA");
    }
}

static app_storage_result_t app_storage_map_result(FRESULT result)
{
    if (result == FR_OK)
    {
        return APP_STORAGE_RESULT_OK;
    }
    if (result == FR_EXIST)
    {
        return APP_STORAGE_RESULT_EXISTS;
    }
    if (result == FR_NO_FILE || result == FR_NO_PATH)
    {
        return APP_STORAGE_RESULT_NOT_FOUND;
    }
    if (result == FR_INVALID_NAME)
    {
        return APP_STORAGE_RESULT_INVALID_NAME;
    }
    if (result == FR_NOT_READY || result == FR_NOT_ENABLED ||
        result == FR_NO_FILESYSTEM)
    {
        return APP_STORAGE_RESULT_NOT_READY;
    }
    return APP_STORAGE_RESULT_IO_ERROR;
}

static FRESULT app_storage_mount(void)
{
    FRESULT result;

    g_storage_state = APP_STORAGE_STATE_STARTING;
    result = f_mount(&g_sd_filesystem, "0:", 1U);
    if (result == FR_OK)
    {
        g_capacity_mb = (uint32_t)((uint64_t)SDCardInfo.CardCapacity >> 20);
        g_storage_state = APP_STORAGE_STATE_READY;
    }
    else
    {
        g_capacity_mb = 0U;
        g_storage_state = APP_STORAGE_STATE_ERROR;
    }
    return result;
}

static void app_storage_run_self_test(void)
{
    static const char expected_text[] = "STM32 SD TEST";
    static const char write_text[] = "STM32 SD WRITE OK\r\n";
    FIL file;
    FRESULT result;
    UINT transferred;
    char buffer[64];

    result = f_open(&file, "0:/TEST.TXT", FA_READ);
    if (result == FR_OK)
    {
        memset(buffer, 0, sizeof(buffer));
        result = f_read(&file, buffer, sizeof(buffer) - 1U, &transferred);
        f_close(&file);
        if (result == FR_OK && transferred >= sizeof(expected_text) - 1U &&
            memcmp(buffer, expected_text, sizeof(expected_text) - 1U) == 0)
        {
            printf("SD TEST.TXT content: PASS\r\n");
        }
        else
        {
            printf("SD TEST.TXT content: FAIL (%u)\r\n", (unsigned int)result);
        }
    }
    else
    {
        printf("SD TEST.TXT: OPEN FAIL (%u)\r\n", (unsigned int)result);
    }

    result = f_open(&file, "0:/STM32_OK.TXT", FA_CREATE_ALWAYS | FA_WRITE);
    if (result == FR_OK)
    {
        result = f_write(&file, write_text, sizeof(write_text) - 1U, &transferred);
        if (result == FR_OK && transferred == sizeof(write_text) - 1U)
        {
            result = f_sync(&file);
        }
        f_close(&file);
    }
    if (result == FR_OK && transferred == sizeof(write_text) - 1U)
    {
        printf("SD write/read verify: PASS\r\n");
    }
    else
    {
        printf("SD write/read verify: FAIL (%u)\r\n", (unsigned int)result);
    }
}

static void app_storage_sort_files(app_storage_response_t *response)
{
    uint8_t first;
    uint8_t second;
    app_storage_file_t temporary;

    for (first = 0U; first < response->file_count; first++)
    {
        for (second = (uint8_t)(first + 1U);
             second < response->file_count;
             second++)
        {
            if (strcmp(response->files[first].name,
                       response->files[second].name) > 0)
            {
                temporary = response->files[first];
                response->files[first] = response->files[second];
                response->files[second] = temporary;
            }
        }
    }
}

static void app_storage_list(app_storage_response_t *response)
{
    DIR directory;
    FILINFO information;
    FRESULT result;
    app_storage_file_t *file;

    result = f_opendir(&directory, "0:/");
    if (result == FR_OK)
    {
        while (response->file_count < APP_STORAGE_MAX_FILES)
        {
            result = f_readdir(&directory, &information);
            if (result != FR_OK || information.fname[0] == '\0')
            {
                break;
            }
            if ((information.fattrib & AM_DIR) != 0U)
            {
                continue;
            }

            file = &response->files[response->file_count];
            app_storage_copy_text(file->name, sizeof(file->name), information.fname);
            app_storage_make_path(file->location, file->name);
            app_storage_set_file_type(file);
            file->size = (uint32_t)information.fsize;
            file->active = 1U;
            response->file_count++;
        }
        f_closedir(&directory);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    if (response->result == APP_STORAGE_RESULT_OK)
    {
        app_storage_sort_files(response);
    }
}

static void app_storage_create(const app_storage_request_t *request,
                               app_storage_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (request->content_length >= APP_STORAGE_CONTENT_SIZE)
    {
        response->result = APP_STORAGE_RESULT_IO_ERROR;
        return;
    }

    app_storage_make_path(path, request->name);
    result = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    transferred = 0U;
    if (result == FR_OK)
    {
        result = f_write(&file, request->content, request->content_length,
                         &transferred);
        if (result == FR_OK && transferred == request->content_length)
        {
            result = f_sync(&file);
        }
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

static void app_storage_read(const app_storage_request_t *request,
                             app_storage_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    app_storage_make_path(path, request->name);
    result = f_open(&file, path, FA_READ);
    transferred = 0U;
    if (result == FR_OK)
    {
        response->file_size = (uint32_t)f_size(&file);
        result = f_read(&file, response->content,
                        APP_STORAGE_CONTENT_SIZE - 1U, &transferred);
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->content_length = (uint16_t)transferred;
    response->content[transferred] = '\0';
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

static void app_storage_write(const app_storage_request_t *request,
                              app_storage_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (request->content_length >= APP_STORAGE_CONTENT_SIZE)
    {
        response->result = APP_STORAGE_RESULT_IO_ERROR;
        return;
    }

    app_storage_make_path(path, request->name);
    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    transferred = 0U;
    if (result == FR_OK)
    {
        result = f_write(&file, request->content, request->content_length,
                         &transferred);
        if (result == FR_OK && transferred == request->content_length)
        {
            result = f_sync(&file);
        }
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->content_length = (uint16_t)transferred;
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

static void app_storage_delete(const app_storage_request_t *request,
                               app_storage_response_t *response)
{
    FRESULT result;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    app_storage_make_path(path, request->name);
    result = f_unlink(path);
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

static FRESULT app_storage_recover_binary_target(const char *path,
                                                  const char *temporary_path,
                                                  const char *backup_path)
{
    FILINFO information;
    FRESULT result;

    result = f_stat(path, &information);
    if (result == FR_OK)
    {
        return FR_OK;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        return result;
    }

    result = f_stat(backup_path, &information);
    if (result == FR_OK)
    {
        return f_rename(backup_path, path);
    }

    result = f_stat(temporary_path, &information);
    if (result == FR_OK)
    {
        return f_rename(temporary_path, path);
    }
    return FR_NO_FILE;
}

static void app_storage_read_binary(const app_storage_request_t *request,
                                    app_storage_binary_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    uint32_t file_size;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name) ||
        request->binary_data == NULL || request->binary_capacity == 0U)
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    app_storage_make_path(path, request->name);
    result = app_storage_recover_binary_target(path,
                                               STORAGE_BINARY_TEMP_PATH,
                                               STORAGE_BINARY_BACKUP_PATH);
    if (result != FR_OK)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }

    result = f_open(&file, path, FA_READ);
    transferred = 0U;
    file_size = 0U;
    if (result == FR_OK)
    {
        file_size = (uint32_t)f_size(&file);
        if (file_size > request->binary_capacity)
        {
            result = FR_INVALID_OBJECT;
        }
        else
        {
            result = f_read(&file, request->binary_data, file_size,
                            &transferred);
        }
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->data_length = (result == FR_OK) ? (uint32_t)transferred : file_size;
}

static void app_storage_write_binary(const app_storage_request_t *request,
                                     app_storage_binary_response_t *response)
{
    FIL file;
    FILINFO information;
    FRESULT result;
    FRESULT old_result;
    UINT transferred;
    uint8_t had_old_file;
    char path[APP_STORAGE_PATH_LENGTH];
    const char *temporary_path;
    const char *backup_path;

    if (!app_storage_name_is_valid(request->name) ||
        request->binary_data == NULL || request->binary_length == 0U)
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    if (request->operation == APP_STORAGE_OP_WRITE_LOG)
    {
        temporary_path = STORAGE_LOG_TEMP_PATH;
        backup_path = STORAGE_LOG_BACKUP_PATH;
    }
    else
    {
        temporary_path = STORAGE_BINARY_TEMP_PATH;
        backup_path = STORAGE_BINARY_BACKUP_PATH;
    }

    app_storage_make_path(path, request->name);
    (void)app_storage_recover_binary_target(path, temporary_path, backup_path);
    (void)f_unlink(temporary_path);

    result = f_open(&file, temporary_path,
                    FA_CREATE_ALWAYS | FA_WRITE);
    transferred = 0U;
    if (result == FR_OK)
    {
        result = f_write(&file, request->binary_data,
                         request->binary_length, &transferred);
        if (result == FR_OK && transferred == request->binary_length)
        {
            result = f_sync(&file);
        }
        else if (result == FR_OK)
        {
            result = FR_DISK_ERR;
        }
        f_close(&file);
    }
    if (result != FR_OK)
    {
        (void)f_unlink(temporary_path);
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        response->data_length = (uint32_t)transferred;
        return;
    }

    had_old_file = (f_stat(path, &information) == FR_OK) ? 1U : 0U;
    if (had_old_file)
    {
        (void)f_unlink(backup_path);
        result = f_rename(path, backup_path);
    }
    if (result == FR_OK)
    {
        result = f_rename(temporary_path, path);
    }
    if (result != FR_OK)
    {
        if (had_old_file)
        {
            old_result = f_rename(backup_path, path);
            (void)old_result;
        }
        (void)f_unlink(temporary_path);
    }
    else if (had_old_file)
    {
        (void)f_unlink(backup_path);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->data_length = (result == FR_OK) ? request->binary_length : 0U;
}

static void app_storage_process_binary(const app_storage_request_t *request,
                                       app_storage_binary_response_t *response)
{
    FRESULT mount_result;

    memset(response, 0, sizeof(*response));
    response->operation = request->operation;
    if (g_storage_state != APP_STORAGE_STATE_READY)
    {
        mount_result = app_storage_mount();
        if (mount_result != FR_OK)
        {
            response->filesystem_result = (uint8_t)mount_result;
            response->result = APP_STORAGE_RESULT_NOT_READY;
            return;
        }
    }

    if (request->operation == APP_STORAGE_OP_READ_BINARY)
    {
        app_storage_read_binary(request, response);
    }
    else
    {
        app_storage_write_binary(request, response);
    }
}

static void app_storage_process(const app_storage_request_t *request,
                                app_storage_response_t *response)
{
    FRESULT mount_result;

    memset(response, 0, sizeof(*response));
    response->operation = request->operation;

    if (g_storage_state != APP_STORAGE_STATE_READY)
    {
        mount_result = app_storage_mount();
        if (mount_result != FR_OK)
        {
            response->filesystem_result = (uint8_t)mount_result;
            response->result = APP_STORAGE_RESULT_NOT_READY;
            return;
        }
    }

    switch (request->operation)
    {
        case APP_STORAGE_OP_LIST:
            app_storage_list(response);
            break;

        case APP_STORAGE_OP_CREATE:
            app_storage_create(request, response);
            break;

        case APP_STORAGE_OP_READ:
            app_storage_read(request, response);
            break;

        case APP_STORAGE_OP_WRITE:
            app_storage_write(request, response);
            break;

        case APP_STORAGE_OP_DELETE:
            app_storage_delete(request, response);
            break;

        default:
            response->result = APP_STORAGE_RESULT_IO_ERROR;
            break;
    }
}

BaseType_t app_storage_init(void)
{
    g_request_queue = xQueueCreate(STORAGE_REQUEST_QUEUE_LENGTH,
                                   sizeof(app_storage_request_t));
    g_response_queue = xQueueCreate(STORAGE_RESPONSE_QUEUE_LENGTH,
                                    sizeof(app_storage_response_t));
    g_binary_response_queue = xQueueCreate(STORAGE_BINARY_QUEUE_LENGTH,
                                    sizeof(app_storage_binary_response_t));
    g_log_response_queue = xQueueCreate(STORAGE_LOG_QUEUE_LENGTH,
                                    sizeof(app_storage_binary_response_t));
    if (g_request_queue == NULL || g_response_queue == NULL ||
        g_binary_response_queue == NULL || g_log_response_queue == NULL)
    {
        return pdFAIL;
    }

    vQueueAddToRegistry(g_request_queue, "StorageRequests");
    vQueueAddToRegistry(g_response_queue, "StorageResponses");
    vQueueAddToRegistry(g_binary_response_queue, "StorageBinaryResponses");
    vQueueAddToRegistry(g_log_response_queue, "StorageLogResponses");
    return pdPASS;
}

BaseType_t app_storage_submit(const app_storage_request_t *request)
{
    if (request == NULL || g_request_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueSend(g_request_queue, request, 0U);
}

BaseType_t app_storage_receive(app_storage_response_t *response)
{
    if (response == NULL || g_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_response_queue, response, 0U);
}

BaseType_t app_storage_receive_binary(app_storage_binary_response_t *response)
{
    if (response == NULL || g_binary_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_binary_response_queue, response, 0U);
}

BaseType_t app_storage_receive_log(app_storage_binary_response_t *response)
{
    if (response == NULL || g_log_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_log_response_queue, response, 0U);
}

app_storage_state_t app_storage_get_state(void)
{
    return g_storage_state;
}

uint32_t app_storage_get_capacity_mb(void)
{
    return g_capacity_mb;
}

void AppStorageTask(void *argument)
{
    static app_storage_response_t response;
    app_storage_binary_response_t binary_response;
    app_storage_request_t request;
    FRESULT result;

    (void)argument;
    printf("SD storage service: starting\r\n");
    result = app_storage_mount();
    if (result == FR_OK)
    {
        app_logs_add(APP_LOG_LEVEL_INFO, "STORAGE", "SD FAT32 MOUNTED");
        printf("SD card: %lu MB, FAT32 mount: PASS\r\n",
               (unsigned long)g_capacity_mb);
        app_storage_run_self_test();
    }
    else
    {
        app_logs_add(APP_LOG_LEVEL_ERROR, "STORAGE", "SD MOUNT FAILED");
        printf("SD/FatFs mount: FAIL (%u)\r\n", (unsigned int)result);
    }

    while (1)
    {
        if (xQueueReceive(g_request_queue, &request, portMAX_DELAY) == pdPASS)
        {
            if (request.operation == APP_STORAGE_OP_READ_BINARY ||
                request.operation == APP_STORAGE_OP_WRITE_BINARY)
            {
                app_storage_process_binary(&request, &binary_response);
                xQueueSend(g_binary_response_queue, &binary_response,
                           portMAX_DELAY);
            }
            else if (request.operation == APP_STORAGE_OP_WRITE_LOG)
            {
                app_storage_process_binary(&request, &binary_response);
                xQueueSend(g_log_response_queue, &binary_response,
                           portMAX_DELAY);
            }
            else
            {
                app_storage_process(&request, &response);
                xQueueSend(g_response_queue, &response, portMAX_DELAY);
            }
        }
    }
}
