#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/MPU/mpu.h"
#include "./BSP/SDRAM/sdram.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./BSP/TOUCH/gt9xxx.h"
#include "./BSP/IIC/myiic.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "app_input.h"
#include "app_runtime.h"
#include "app_monitor.h"
#include "app_storage.h"
#include "app_audio.h"

#define LCD_WIDTH              800U
#define LCD_HEIGHT             480U
#define SDRAM_TEST_ADDRESS     (BANK5_SDRAM_ADDR + 0x00200000U)
#define SDRAM_TEST_WORDS       4096U

static uint8_t touch_bus_scl;
static uint8_t touch_bus_sda;
static uint8_t touch_scan_count;
static uint8_t touch_scan_first;

static QueueHandle_t g_touch_queue;
static TaskHandle_t g_input_task_handle;
static TaskHandle_t g_runtime_task_handle;
static app_input_task_context_t g_input_context;
static app_runtime_context_t g_runtime_context;
static app_monitor_context_t g_monitor_context;

static uint8_t g_touch_ok;
static uint8_t g_product_id[5];
static const char *g_controller_id = "NONE";

static uint8_t at24c02_probe(void)
{
    uint8_t no_ack;

    iic_init();
    iic_start();
    iic_send_byte(0xA0U);
    no_ack = iic_wait_ack();
    iic_stop();

    return (no_ack == 0U) ? 1U : 0U;
}

static void touch_i2c_diagnose(void)
                                 {
    uint8_t address;

    touch_scan_count = 0U;
    touch_scan_first = 0U;
    ct_iic_init();
    CT_IIC_SCL(1);
    CT_IIC_SDA(1);
    delay_ms(2);
    touch_bus_scl = sys_gpio_pin_get(GPIOH, SYS_GPIO_PIN6);
    touch_bus_sda = sys_gpio_pin_get(GPIOI, SYS_GPIO_PIN3);

    if (touch_bus_scl && touch_bus_sda)
    {
        for (address = 0x08U; address <= 0x77U; address++)
        {
            if (ct_iic_check_device(address << 1) == 0U &&
                ct_iic_check_device(address << 1) == 0U &&
                ct_iic_check_device(address << 1) == 0U)
            {
                if (touch_scan_count == 0U)
                {
                    touch_scan_first = address;
                }
                touch_scan_count++;
                printf("Touch I2C scan: ACK at 7-bit address 0x%02X\r\n", address);
            }
        }
    }

    printf("Touch I2C bus: SCL=%u SDA=%u, devices=%u\r\n",
           touch_bus_scl, touch_bus_sda, touch_scan_count);
}

static uint8_t sdram_test(void)
{
    volatile uint32_t *memory;
    uint32_t index;
    uint32_t expected;

    memory = (volatile uint32_t *)SDRAM_TEST_ADDRESS;
    for (index = 0U; index < SDRAM_TEST_WORDS; index++)
    {
        memory[index] = 0x5AA50000U ^ index;
    }

    for (index = 0U; index < SDRAM_TEST_WORDS; index++)
    {
        expected = 0x5AA50000U ^ index;
        if (memory[index] != expected)
        {
            return 0U;
        }
    }

    for (index = 0U; index < SDRAM_TEST_WORDS; index++)
    {
        memory[index] = 0xA55AFFFFU ^ (index * 0x1021U);
    }

    for (index = 0U; index < SDRAM_TEST_WORDS; index++)
    {
        expected = 0xA55AFFFFU ^ (index * 0x1021U);
        if (memory[index] != expected)
        {
            return 0U;
        }
    }

    return 1U;
}

static void fatal_blink(uint8_t code)
{
    uint8_t count;

    while (1)
    {
        for (count = 0U; count < code; count++)
        {
            LED0(0);
            delay_ms(180);
            LED0(1);
            delay_ms(180);
        }
        delay_ms(900);
    }
}

static void touch_controller_init(void)
{
    g_touch_ok = 0U;

    /* The 800x480 panel is sold with either FT/CST or GT9xxx touch ICs. */
    if (ft5206_init() == 0U)
    {
        tp_dev.scan = ft5206_scan;
        tp_dev.touchtype = 0x81U;
        g_controller_id = "FT/CST";
        g_touch_ok = 1U;
        printf("Touch controller: FT/CST family\r\n");
    }
    else if (gt9xxx_init() == 0U)
    {
        gt9xxx_rd_reg(GT9XXX_PID_REG, g_product_id, 4U);
        tp_dev.scan = gt9xxx_scan;
        tp_dev.touchtype = 0x81U;
        g_controller_id = (char *)g_product_id;
        g_touch_ok = 1U;
        printf("GT9xxx product ID: %s, 7-bit address: 0x%02X\r\n",
               g_product_id, gt9xxx_get_i2c_address());
    }
    else
    {
        printf("Touch I2C: no supported device at 0x38, 0x14 or 0x5D\r\n");
        touch_i2c_diagnose();
    }
}

static void app_tasks_start(void)
{
    if (app_storage_init() != pdPASS)
    {
        fatal_blink(4U);
    }
    if (app_audio_init() != pdPASS)
    {
        fatal_blink(4U);
    }

    g_touch_queue = xQueueCreate(APP_INPUT_QUEUE_LENGTH,
                                 sizeof(app_input_event_t));
    if (g_touch_queue == NULL)
    {
        fatal_blink(4U);
    }
    vQueueAddToRegistry(g_touch_queue, "TouchEvents");

    g_input_context.event_queue = g_touch_queue;
    g_input_context.touch_available = g_touch_ok;

    g_runtime_context.event_queue = g_touch_queue;
    g_runtime_context.touch_available = g_touch_ok;
    g_runtime_context.controller_id = g_controller_id;

    if (xTaskCreate(AppInputTask, "InputTask", 512U,
                    &g_input_context, 4U, &g_input_task_handle) != pdPASS)
    {
        fatal_blink(4U);
    }

    if (xTaskCreate(AppRuntimeTask, "GuiTask", 1024U,
                    &g_runtime_context, 3U, &g_runtime_task_handle) != pdPASS)
    {
        fatal_blink(4U);
    }

    g_monitor_context.event_queue = g_touch_queue;
    g_monitor_context.input_task = g_input_task_handle;
    g_monitor_context.runtime_task = g_runtime_task_handle;
    if (xTaskCreate(AppMonitorTask, "MonitorTask", 512U,
                    &g_monitor_context, 1U, NULL) != pdPASS)
    {
        fatal_blink(4U);
    }

    if (xTaskCreate(AppStorageTask, "StorageTask", 1024U,
                    NULL, 4U, NULL) != pdPASS)
    {
        fatal_blink(4U);
    }

    if (xTaskCreate(AppAudioTask, "AudioTask", 1024U,
                    NULL, 5U, NULL) != pdPASS)
    {
        fatal_blink(4U);
    }
}

int main(void)
{
    sys_stm32_clock_init(160, 5, 2, 4);
    delay_init(400);
    usart_init(100, 115200);
    led_init();
    mpu_memory_protection();

    printf("\r\nSTM32H743 micro desktop runtime\r\n");
    printf("System clock: 400 MHz\r\n");

    sdram_init();
    if (!sdram_test())
    {
        printf("SDRAM test: FAIL at 0x%08X\r\n", SDRAM_TEST_ADDRESS);
        fatal_blink(2U);
    }
    printf("SDRAM test: PASS\r\n");

    lcd_init();
    lcd_display_dir(1U);
    if (lcddev.width != LCD_WIDTH || lcddev.height != LCD_HEIGHT)
    {
        printf("LCD geometry: FAIL (%u x %u)\r\n", lcddev.width, lcddev.height);
        fatal_blink(3U);
    }
    printf("LCD timing: 800x480, PCLK 33.3 MHz, RGB565\r\n");

    touch_controller_init();
    if (at24c02_probe())
    {
        printf("AT24C02: ACK at 0x50\r\n");
    }
    else
    {
        printf("AT24C02: NO ACK at 0x50\r\n");
    }

    lcd_clear(BLACK);
    LED0(1);
    LED1(0);

    app_tasks_start();
    printf("Starting FreeRTOS scheduler...\r\n");
    vTaskStartScheduler();

    fatal_blink(5U);
}
