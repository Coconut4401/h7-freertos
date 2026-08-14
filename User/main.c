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
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define LCD_WIDTH              800U
#define LCD_HEIGHT             480U
#define SDRAM_TEST_ADDRESS     (BANK5_SDRAM_ADDR + 0x00200000U)
#define SDRAM_TEST_WORDS       4096U
#define STATUS_BAR_HEIGHT      32U

static const uint16_t touch_colors[5] = {RED, GREEN, BLUE, MAGENTA, CYAN};
static uint8_t touch_bus_scl;
static uint8_t touch_bus_sda;
static uint8_t touch_scan_count;
static uint8_t touch_scan_first;

typedef enum
{
    TOUCH_EVENT_DOWN = 0,
    TOUCH_EVENT_MOVE,
    TOUCH_EVENT_UP
} touch_event_type_t;

typedef struct
{
    touch_event_type_t type;
    uint16_t x;
    uint16_t y;
    uint32_t tick;
} touch_event_t;

static QueueHandle_t g_touch_queue;
static uint8_t g_touch_ok;
static uint8_t g_product_id[5];
static const char *g_controller_id = "NONE";

static char hex_digit(uint8_t value)
{
    value &= 0x0FU;
    return (value < 10U) ? (char)('0' + value) : (char)('A' + value - 10U);
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
    volatile uint32_t *memory = (volatile uint32_t *)SDRAM_TEST_ADDRESS;
    uint32_t index;
    uint32_t expected;

    for (index = 0; index < SDRAM_TEST_WORDS; index++)
    {
        memory[index] = 0x5AA50000U ^ index;
    }

    for (index = 0; index < SDRAM_TEST_WORDS; index++)
    {
        expected = 0x5AA50000U ^ index;
        if (memory[index] != expected)
        {
            return 0;
        }
    }

    for (index = 0; index < SDRAM_TEST_WORDS; index++)
    {
        memory[index] = 0xA55AFFFFU ^ (index * 0x1021U);
    }

    for (index = 0; index < SDRAM_TEST_WORDS; index++)
    {
        expected = 0xA55AFFFFU ^ (index * 0x1021U);
        if (memory[index] != expected)
        {
            return 0;
        }
    }

    return 1;
}

static void fatal_blink(uint8_t code)
{
    uint8_t count;

    while (1)
    {
        for (count = 0; count < code; count++)
        {
            LED0(0);
            delay_ms(180);
            LED0(1);
            delay_ms(180);
        }
        delay_ms(900);
    }
}

static void draw_color_bars(void)
{
    static const uint16_t colors[8] =
    {
        WHITE, YELLOW, CYAN, GREEN, MAGENTA, RED, BLUE, BLACK
    };
    uint16_t index;
    uint16_t x0;
    uint16_t x1;

    for (index = 0; index < 8; index++)
    {
        x0 = (LCD_WIDTH * index) / 8U;
        x1 = (LCD_WIDTH * (index + 1U)) / 8U - 1U;
        lcd_fill(x0, 72, x1, 191, colors[index]);
    }
}

static void draw_grid(void)
{
    uint16_t x;
    uint16_t y;

    lcd_fill(0, 192, LCD_WIDTH - 1U, LCD_HEIGHT - 1U, 0x18E3);

    for (x = 0; x < LCD_WIDTH; x += 40U)
    {
        lcd_draw_line(x, 192, x, LCD_HEIGHT - 1U, 0x4208);
    }

    for (y = 192; y < LCD_HEIGHT; y += 40U)
    {
        lcd_draw_line(0, y, LCD_WIDTH - 1U, y, 0x4208);
    }
}

static void draw_test_screen(uint8_t touch_ok, const char *controller_id)
{
    lcd_clear(0x18E3);
    lcd_fill(0, 0, LCD_WIDTH - 1U, 47, 0x0013);
    lcd_show_string(16, 12, 500, 24, 24, "STM32H743 + 5.0 LCD TEST", WHITE);

    lcd_fill(0, 48, LCD_WIDTH - 1U, 71, 0x2104);
    lcd_show_string(16, 52, 240, 16, 16, "LCD: 800x480 RGB565", GREEN);
    lcd_show_string(270, 52, 200, 16, 16, "SDRAM: PASS", GREEN);

    if (touch_ok)
    {
        lcd_show_string(480, 52, 80, 16, 16, "CTP:", GREEN);
        lcd_show_string(528, 52, 110, 16, 16, (char *)controller_id, GREEN);
        lcd_show_string(650, 52, 130, 16, 16, "TOUCH: PASS", GREEN);
    }
    else
    {
        lcd_show_string(480, 52, 300, 16, 16, "CTP: NO ACK @38/14/5D", RED);
    }

    draw_color_bars();
    draw_grid();

    if (!touch_ok)
    {
        char bus_line[] = "SCL=0  SDA=0";
        char scan_line[] = "DEV=00  ADDR=0x00";

        bus_line[4] = (char)('0' + touch_bus_scl);
        bus_line[11] = (char)('0' + touch_bus_sda);
        scan_line[4] = (char)('0' + ((touch_scan_count / 10U) % 10U));
        scan_line[5] = (char)('0' + (touch_scan_count % 10U));
        scan_line[15] = hex_digit(touch_scan_first >> 4);
        scan_line[16] = hex_digit(touch_scan_first);

        lcd_fill(16, 204, 470, 292, BLACK);
        lcd_show_string(28, 212, 420, 24, 24, "I2C BUS STATUS", WHITE);
        lcd_show_string(28, 240, 420, 24, 24, bus_line, YELLOW);
        lcd_show_string(28, 268, 420, 24, 24, scan_line, YELLOW);
    }

    lcd_fill(0, LCD_HEIGHT - STATUS_BAR_HEIGHT, LCD_WIDTH - 1U, LCD_HEIGHT - 1U, 0x2104);
    lcd_show_string(12, LCD_HEIGHT - 24U, 610, 16, 16, "Touch the grid to draw. Touch CLEAR to reset.", WHITE);
    lcd_fill(690, LCD_HEIGHT - 29U, 790, LCD_HEIGHT - 5U, RED);
    lcd_show_string(714, LCD_HEIGHT - 24U, 64, 16, 16, "CLEAR", WHITE);
}

static void draw_touch_point(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= (LCD_HEIGHT - STATUS_BAR_HEIGHT))
    {
        return;
    }

    lcd_fill_circle(x, y, 5, color);
    lcd_draw_line(x > 10U ? x - 10U : 0U, y, x + 10U < LCD_WIDTH ? x + 10U : LCD_WIDTH - 1U, y, color);
    lcd_draw_line(x, y > 10U ? y - 10U : 0U, x, y + 10U < LCD_HEIGHT ? y + 10U : LCD_HEIGHT - 1U, color);
}

static void MonitorTask(void *argument)
{
    TickType_t last_wake;

    (void)argument;
    last_wake = xTaskGetTickCount();

    while (1)
    {
        LED1_TOGGLE();
        printf("FreeRTOS running: tick=%lu, free heap=%u\r\n",
               (unsigned long)xTaskGetTickCount(),
               (unsigned int)xPortGetFreeHeapSize());
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

static void InputTask(void *argument)
{
    touch_event_t event;
    TickType_t last_wake;
    uint16_t last_x = 0U;
    uint16_t last_y = 0U;
    uint8_t was_pressed = 0U;
    uint8_t is_pressed;

    (void)argument;
    last_wake = xTaskGetTickCount();

    while (1)
    {
        if (g_touch_ok)
        {
            tp_dev.scan(0);
            is_pressed = (tp_dev.sta & TP_PRES_DOWN) ? 1U : 0U;

            if (is_pressed &&
                tp_dev.x[0] < LCD_WIDTH &&
                tp_dev.y[0] < LCD_HEIGHT)
            {
                if (!was_pressed ||
                    tp_dev.x[0] != last_x ||
                    tp_dev.y[0] != last_y)
                {
                    event.type = was_pressed ? TOUCH_EVENT_MOVE : TOUCH_EVENT_DOWN;
                    event.x = tp_dev.x[0];
                    event.y = tp_dev.y[0];
                    event.tick = xTaskGetTickCount();
                    (void)xQueueSend(g_touch_queue, &event, 0);

                    last_x = event.x;
                    last_y = event.y;
                }

                was_pressed = 1U;
            }
            else if (!is_pressed && was_pressed)
            {
                event.type = TOUCH_EVENT_UP;
                event.x = last_x;
                event.y = last_y;
                event.tick = xTaskGetTickCount();
                (void)xQueueSend(g_touch_queue, &event, 0);
                was_pressed = 0U;
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5));
    }
}

static void GuiTask(void *argument)
{
    touch_event_t event;

    (void)argument;

    while (1)
    {
        if (xQueueReceive(g_touch_queue, &event, portMAX_DELAY) == pdPASS)
        {
            if (event.type == TOUCH_EVENT_DOWN &&
                event.x >= 690U &&
                event.y >= (LCD_HEIGHT - STATUS_BAR_HEIGHT))
            {
                draw_test_screen(g_touch_ok, g_controller_id);
            }
            else if (event.type == TOUCH_EVENT_DOWN ||
                     event.type == TOUCH_EVENT_MOVE)
            {
                draw_touch_point(event.x, event.y, touch_colors[0]);
            }
        }
    }
}
int main(void)
{

    sys_stm32_clock_init(160, 5, 2, 4);
    delay_init(400);
    usart_init(100, 115200);
    led_init();
    mpu_memory_protection();

    printf("\r\nSTM32H743 5-inch LCD/GT911 self-test\r\n");
    printf("System clock: 400 MHz\r\n");

    sdram_init();
    if (!sdram_test())
    {
        printf("SDRAM test: FAIL at 0x%08X\r\n", SDRAM_TEST_ADDRESS);
        fatal_blink(2);
    }
    printf("SDRAM test: PASS\r\n");

    lcd_init();
    lcd_display_dir(1);
    if (lcddev.width != LCD_WIDTH || lcddev.height != LCD_HEIGHT)
    {
        printf("LCD geometry: FAIL (%u x %u)\r\n", lcddev.width, lcddev.height);
        fatal_blink(3);
    }
    printf("LCD timing: 800x480, PCLK 33.3 MHz, RGB565\r\n");

    g_touch_ok = 0U;

    /* The 800x480 adapter is sold with either FT/CST or GT9xxx touch ICs. */
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
        gt9xxx_rd_reg(GT9XXX_PID_REG, g_product_id, 4);
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

    draw_test_screen(g_touch_ok, g_controller_id);
    LED0(1);
    LED1(0);

    g_touch_queue = xQueueCreate(32, sizeof(touch_event_t));
    if (g_touch_queue == NULL)
    {
        fatal_blink(4);
    }

    if (xTaskCreate(InputTask, "InputTask", 512, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(GuiTask, "GuiTask", 1024, NULL, 3, NULL) != pdPASS ||
        xTaskCreate(MonitorTask, "MonitorTask", 512, NULL, 1, NULL) != pdPASS)
    {
        fatal_blink(4);
    }

    printf("Starting FreeRTOS scheduler...\r\n");
    vTaskStartScheduler();

    fatal_blink(5);
}
