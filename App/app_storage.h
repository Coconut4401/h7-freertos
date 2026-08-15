#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include <stdint.h>

#include "FreeRTOS.h"

#define APP_STORAGE_MAX_FILES       24U
#define APP_STORAGE_NAME_LENGTH     13U
#define APP_STORAGE_TYPE_LENGTH     8U
#define APP_STORAGE_PATH_LENGTH     17U
#define APP_STORAGE_CONTENT_SIZE    192U

typedef enum
{
    APP_STORAGE_STATE_STARTING = 0,
    APP_STORAGE_STATE_READY,
    APP_STORAGE_STATE_ERROR
} app_storage_state_t;

typedef enum
{
    APP_STORAGE_OP_LIST = 0,
    APP_STORAGE_OP_CREATE,
    APP_STORAGE_OP_READ,
    APP_STORAGE_OP_WRITE,
    APP_STORAGE_OP_DELETE,
    APP_STORAGE_OP_READ_BINARY,
    APP_STORAGE_OP_WRITE_BINARY,
    APP_STORAGE_OP_WRITE_LOG
} app_storage_operation_t;

typedef enum
{
    APP_STORAGE_RESULT_OK = 0,
    APP_STORAGE_RESULT_NOT_READY,
    APP_STORAGE_RESULT_EXISTS,
    APP_STORAGE_RESULT_NOT_FOUND,
    APP_STORAGE_RESULT_INVALID_NAME,
    APP_STORAGE_RESULT_IO_ERROR,
    APP_STORAGE_RESULT_QUEUE_FULL
} app_storage_result_t;

typedef struct
{
    char name[APP_STORAGE_NAME_LENGTH];
    char type[APP_STORAGE_TYPE_LENGTH];
    char location[APP_STORAGE_PATH_LENGTH];
    uint32_t size;
    uint8_t active;
} app_storage_file_t;

typedef struct
{
    app_storage_operation_t operation;
    char name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    uint16_t content_length;
    void *binary_data;
    uint32_t binary_length;
    uint32_t binary_capacity;
} app_storage_request_t;

typedef struct
{
    app_storage_operation_t operation;
    app_storage_result_t result;
    uint8_t filesystem_result;
    uint32_t data_length;
} app_storage_binary_response_t;

typedef struct
{
    app_storage_operation_t operation;
    app_storage_result_t result;
    uint8_t filesystem_result;
    uint8_t file_count;
    uint16_t content_length;
    uint32_t file_size;
    char name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    app_storage_file_t files[APP_STORAGE_MAX_FILES];
} app_storage_response_t;

BaseType_t app_storage_init(void);
BaseType_t app_storage_submit(const app_storage_request_t *request);
BaseType_t app_storage_receive(app_storage_response_t *response);
BaseType_t app_storage_receive_binary(app_storage_binary_response_t *response);
BaseType_t app_storage_receive_log(app_storage_binary_response_t *response);
app_storage_state_t app_storage_get_state(void);
uint32_t app_storage_get_capacity_mb(void);
void AppStorageTask(void *argument);

#endif
