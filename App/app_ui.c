#include "app_ui.h"

#include <string.h>

#include "./BSP/LCD/lcd.h"
#include "app_input.h"

#define UI_WIDTH              800U
#define UI_HEIGHT             480U
#define UI_TOP_HEIGHT         48U
#define UI_BOTTOM_Y           448U

#define UI_COLOR_BG           0xDEFB
#define UI_COLOR_TOP          0x10A2
#define UI_COLOR_PANEL        0x2945
#define UI_COLOR_PANEL_DARK   0x18E3
#define UI_COLOR_ACCENT       0x04FF
#define UI_COLOR_BUTTON       0x4A69
#define UI_COLOR_SELECTED     0x05ED
#define UI_COLOR_MUTED        0x9CF3

#define KEYPAD_X              310U
#define KEYPAD_Y              160U
#define KEY_WIDTH             100U
#define KEY_HEIGHT            56U
#define KEY_GAP_X             12U
#define KEY_GAP_Y             10U

#define CURSOR_BASE_WIDTH     6U
#define CURSOR_BASE_HEIGHT    8U
#define CURSOR_MAX_SCALE      APP_UI_CURSOR_SIZE_LARGE
#define CURSOR_MAX_WIDTH      (CURSOR_BASE_WIDTH * CURSOR_MAX_SCALE)
#define CURSOR_MAX_HEIGHT     (CURSOR_BASE_HEIGHT * CURSOR_MAX_SCALE)

#define FILE_LIST_X           24U
#define FILE_LIST_Y           74U
#define FILE_LIST_WIDTH       566U
#define FILE_ROW_HEIGHT       54U
#define FILE_BUTTON_X         620U
#define FILE_BUTTON_WIDTH     155U
#define FILE_BUTTON_HEIGHT    42U

#define DRAW_BUTTON_X         642U
#define DRAW_BUTTON_WIDTH     134U
#define DRAW_BUTTON_HEIGHT    42U

#define LOG_LIST_X            18U
#define LOG_LIST_Y            78U
#define LOG_LIST_WIDTH        590U
#define LOG_ROW_HEIGHT        46U
#define LOG_BUTTON_X          630U
#define LOG_BUTTON_WIDTH      150U
#define LOG_BUTTON_HEIGHT     42U

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    const char *label;
} desktop_icon_t;

static const char *const g_key_labels[4][3] =
{
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {"CLR", "0", "OK"}
};

static const desktop_icon_t g_desktop_icons[6] =
{
    {220U, 92U, 150U, 120U, "FILES"},
    {400U, 92U, 150U, 120U, "DRAW"},
    {580U, 92U, 150U, 120U, "MUSIC"},
    {220U, 250U, 150U, 120U, "LOGS"},
    {400U, 250U, 150U, 120U, "MONITOR"},
    {580U, 250U, 150U, 120U, "SETTINGS"}
};

static const char g_cursor_bitmap[CURSOR_BASE_HEIGHT][CURSOR_BASE_WIDTH + 1U] =
{
    "#.....",
    "##....",
    "#+#...",
    "#++#..",
    "#+++#.",
    "#+###.",
    "##+.#.",
    ".##..."
};

static uint32_t g_cursor_background[CURSOR_MAX_WIDTH * CURSOR_MAX_HEIGHT];
static uint16_t g_cursor_x;
static uint16_t g_cursor_y;
static uint8_t g_cursor_visible;
static uint8_t g_cursor_size = APP_UI_CURSOR_SIZE_MEDIUM;
static int8_t g_selected_icon = -1;

static uint16_t app_ui_text_width(const char *text, uint8_t font_size)
{
    uint16_t length;

    length = 0U;
    while (text[length] != '\0')
    {
        length++;
    }

    return (uint16_t)(length * (font_size / 2U));
}

static void app_ui_show_text(uint16_t x,
                             uint16_t y,
                             uint16_t width,
                             uint16_t height,
                             uint8_t font_size,
                             const char *text,
                             uint16_t color)
{
    uint16_t start_x;
    uint16_t end_x;
    uint16_t end_y;

    start_x = x;
    end_x = (uint16_t)(x + width);
    end_y = (uint16_t)(y + height);

    while (*text >= ' ' && *text <= '~')
    {
        if ((uint16_t)(x + font_size / 2U) > end_x)
        {
            x = start_x;
            y = (uint16_t)(y + font_size);
        }
        if ((uint16_t)(y + font_size) > end_y)
        {
            break;
        }

        lcd_show_char(x, y, *text, font_size, 1U, color);
        x = (uint16_t)(x + font_size / 2U);
        text++;
    }
}

static void app_ui_show_number(uint16_t x,
                               uint16_t y,
                               uint32_t number,
                               uint8_t digits,
                               uint8_t font_size,
                               uint16_t color)
{
    char text[11];
    uint8_t index;

    if (digits > 10U)
    {
        digits = 10U;
    }

    text[digits] = '\0';
    for (index = digits; index > 0U; index--)
    {
        text[index - 1U] = (char)('0' + number % 10U);
        number /= 10U;
    }

    app_ui_show_text(x, y, (uint16_t)(digits * font_size / 2U),
                     font_size, font_size, text, color);
}

static void app_ui_show_u32(uint16_t x,
                            uint16_t y,
                            uint32_t number,
                            uint8_t font_size,
                            uint16_t color)
{
    char text[11];
    uint8_t start;

    start = 10U;
    text[10] = '\0';
    do
    {
        start--;
        text[start] = (char)('0' + number % 10U);
        number /= 10U;
    } while (number > 0U && start > 0U);

    app_ui_show_text(x, y, (uint16_t)((10U - start) * font_size / 2U),
                     font_size, font_size, &text[start], color);
}

static void app_ui_show_centered(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint8_t font_size,
                                 const char *text,
                                 uint16_t color)
{
    uint16_t text_width;
    uint16_t text_x;

    text_width = app_ui_text_width(text, font_size);
    text_x = (text_width < width) ? (uint16_t)(x + (width - text_width) / 2U) : x;
    app_ui_show_text(text_x, y, width, font_size, font_size, text, color);
}

static void app_ui_cursor_hide(void)
{
    uint16_t row;
    uint16_t column;
    uint16_t cursor_width;
    uint16_t cursor_height;

    if (!g_cursor_visible)
    {
        return;
    }

    cursor_width = (uint16_t)(CURSOR_BASE_WIDTH * g_cursor_size);
    cursor_height = (uint16_t)(CURSOR_BASE_HEIGHT * g_cursor_size);
    for (row = 0U; row < cursor_height; row++)
    {
        for (column = 0U; column < cursor_width; column++)
        {
            lcd_draw_point((uint16_t)(g_cursor_x + column),
                           (uint16_t)(g_cursor_y + row),
                           g_cursor_background[row * CURSOR_MAX_WIDTH + column]);
        }
    }

    g_cursor_visible = 0U;
}

static void app_ui_cursor_show(uint16_t x, uint16_t y)
{
    uint16_t row;
    uint16_t column;
    uint16_t cursor_width;
    uint16_t cursor_height;
    char pixel;

    cursor_width = (uint16_t)(CURSOR_BASE_WIDTH * g_cursor_size);
    cursor_height = (uint16_t)(CURSOR_BASE_HEIGHT * g_cursor_size);
    if (x > UI_WIDTH - cursor_width)
    {
        x = UI_WIDTH - cursor_width;
    }
    if (y > UI_HEIGHT - cursor_height)
    {
        y = UI_HEIGHT - cursor_height;
    }

    g_cursor_x = x;
    g_cursor_y = y;

    for (row = 0U; row < cursor_height; row++)
    {
        for (column = 0U; column < cursor_width; column++)
        {
            g_cursor_background[row * CURSOR_MAX_WIDTH + column] =
                lcd_read_point((uint16_t)(x + column), (uint16_t)(y + row));
        }
    }

    for (row = 0U; row < cursor_height; row++)
    {
        for (column = 0U; column < cursor_width; column++)
        {
            pixel = g_cursor_bitmap[row / g_cursor_size]
                                   [column / g_cursor_size];
            if (pixel == '#')
            {
                lcd_draw_point((uint16_t)(x + column), (uint16_t)(y + row), BLACK);
            }
            else if (pixel == '+')
            {
                lcd_draw_point((uint16_t)(x + column), (uint16_t)(y + row), WHITE);
            }
        }
    }

    g_cursor_visible = 1U;
}

static void app_ui_draw_key(uint8_t row, uint8_t column)
{
    uint16_t x;
    uint16_t y;

    x = (uint16_t)(KEYPAD_X + column * (KEY_WIDTH + KEY_GAP_X));
    y = (uint16_t)(KEYPAD_Y + row * (KEY_HEIGHT + KEY_GAP_Y));
    lcd_fill(x, y, (uint16_t)(x + KEY_WIDTH - 1U),
             (uint16_t)(y + KEY_HEIGHT - 1U), UI_COLOR_BUTTON);
    lcd_draw_rectangle(x, y, (uint16_t)(x + KEY_WIDTH - 1U),
                       (uint16_t)(y + KEY_HEIGHT - 1U), WHITE);
    app_ui_show_centered(x, (uint16_t)(y + 16U), KEY_WIDTH, 24U,
                         g_key_labels[row][column], WHITE);
}

static void app_ui_draw_icon(uint8_t index, uint8_t selected)
{
    const desktop_icon_t *icon;
    uint16_t fill_color;
    uint16_t border_color;

    icon = &g_desktop_icons[index];
    fill_color = selected ? UI_COLOR_SELECTED : UI_COLOR_BUTTON;
    border_color = selected ? YELLOW : WHITE;

    lcd_fill(icon->x, icon->y,
             (uint16_t)(icon->x + icon->width - 1U),
             (uint16_t)(icon->y + icon->height - 1U), fill_color);
    lcd_draw_rectangle(icon->x, icon->y,
                       (uint16_t)(icon->x + icon->width - 1U),
                       (uint16_t)(icon->y + icon->height - 1U), border_color);
    lcd_fill((uint16_t)(icon->x + 54U), (uint16_t)(icon->y + 18U),
             (uint16_t)(icon->x + 95U), (uint16_t)(icon->y + 59U),
             (uint16_t)(0xF800U + index * 0x00E0U));
    app_ui_show_centered(icon->x, (uint16_t)(icon->y + 82U),
                         icon->width, 16U, icon->label, WHITE);
}

static void app_ui_format_time(uint32_t seconds, char time_text[9])
{
    uint32_t hours;
    uint32_t minutes;

    seconds %= 86400U;
    hours = seconds / 3600U;
    minutes = (seconds % 3600U) / 60U;
    seconds %= 60U;

    time_text[0] = (char)('0' + hours / 10U);
    time_text[1] = (char)('0' + hours % 10U);
    time_text[2] = ':';
    time_text[3] = (char)('0' + minutes / 10U);
    time_text[4] = (char)('0' + minutes % 10U);
    time_text[5] = ':';
    time_text[6] = (char)('0' + seconds / 10U);
    time_text[7] = (char)('0' + seconds % 10U);
    time_text[8] = '\0';
}

static void app_ui_draw_application_header(const char *title)
{
    g_cursor_visible = 0U;
    lcd_clear(UI_COLOR_BG);
    lcd_fill(0U, 0U, UI_WIDTH - 1U, UI_TOP_HEIGHT - 1U, UI_COLOR_TOP);
    lcd_fill(0U, UI_BOTTOM_Y, UI_WIDTH - 1U, UI_HEIGHT - 1U, UI_COLOR_TOP);

    lcd_fill(12U, 8U, 108U, 39U, UI_COLOR_ACCENT);
    lcd_draw_rectangle(12U, 8U, 108U, 39U, WHITE);
    app_ui_show_centered(12U, 12U, 97U, 16U, "BACK", BLACK);
    app_ui_show_text(142U, 12U, 500U, 24U, 24U, title, WHITE);
    lcd_fill(680U, 8U, 788U, 39U, UI_COLOR_BUTTON);
    lcd_draw_rectangle(680U, 8U, 788U, 39U, WHITE);
    app_ui_show_centered(680U, 12U, 109U, 16U, "SLEEP", WHITE);
    app_ui_show_text(16U, 456U, 500U, 16U, 16U,
                     "BACK returns to the desktop", WHITE);
}

static void app_ui_draw_metric_panel(uint16_t x,
                                     uint16_t y,
                                     const char *label)
{
    lcd_fill(x, y, (uint16_t)(x + 349U), (uint16_t)(y + 89U),
             UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(x, y, (uint16_t)(x + 349U),
                       (uint16_t)(y + 89U), UI_COLOR_MUTED);
    app_ui_show_text((uint16_t)(x + 14U), (uint16_t)(y + 10U),
                     320U, 16U, 16U, label, UI_COLOR_MUTED);
}

void app_ui_show_boot(uint8_t touch_available, const char *controller_id)
{
    g_cursor_visible = 0U;
    g_selected_icon = -1;
    lcd_clear(UI_COLOR_PANEL_DARK);
    lcd_fill(0U, 0U, UI_WIDTH - 1U, UI_TOP_HEIGHT - 1U, UI_COLOR_TOP);
    app_ui_show_text(18U, 12U, 500U, 24U, 24U, "MICRO DESKTOP RUNTIME", WHITE);
    app_ui_show_centered(80U, 126U, 640U, 32U, "STM32H743", WHITE);
    app_ui_show_centered(80U, 176U, 640U, 24U, "FreeRTOS system is starting", UI_COLOR_ACCENT);

    lcd_draw_rectangle(149U, 272U, 650U, 304U, WHITE);
    lcd_fill(154U, 277U, 645U, 299U, UI_COLOR_ACCENT);
    app_ui_show_centered(100U, 330U, 600U, 16U,
                         touch_available ? "TOUCH INPUT: ONLINE" : "TOUCH INPUT: OFFLINE",
                         touch_available ? GREEN : RED);
    if (touch_available && controller_id != NULL)
    {
        app_ui_show_text(330U, 360U, 80U, 16U, 16U, "CTRL:", UI_COLOR_MUTED);
        app_ui_show_text(382U, 360U, 140U, 16U, 16U, controller_id, WHITE);
    }
}

void app_ui_show_login(uint8_t touch_available)
{
    uint8_t row;
    uint8_t column;

    g_cursor_visible = 0U;
    lcd_clear(UI_COLOR_BG);
    lcd_fill(0U, 0U, 190U, UI_HEIGHT - 1U, UI_COLOR_TOP);
    app_ui_show_text(24U, 44U, 150U, 64U, 32U, "LOGIN", WHITE);
    app_ui_show_text(24U, 112U, 145U, 48U, 16U, "Enter the", UI_COLOR_MUTED);
    app_ui_show_text(24U, 132U, 145U, 48U, 16U, "4-digit PIN", UI_COLOR_MUTED);
    app_ui_show_text(24U, 402U, 150U, 18U, 16U,
                     touch_available ? "INPUT ONLINE" : "INPUT OFFLINE",
                     touch_available ? GREEN : RED);

    app_ui_show_centered(250U, 28U, 450U, 24U, "USER AUTHENTICATION", UI_COLOR_TOP);
    for (row = 0U; row < 4U; row++)
    {
        for (column = 0U; column < 3U; column++)
        {
            app_ui_draw_key(row, column);
        }
    }

    app_ui_update_login(0U, 0U, "ENTER PIN", 0U);
}

void app_ui_update_login(uint8_t digit_count,
                         uint8_t failed_attempts,
                         const char *message,
                         uint8_t is_error)
{
    char mask[5];
    uint8_t index;

    if (digit_count > 4U)
    {
        digit_count = 4U;
    }

    for (index = 0U; index < digit_count; index++)
    {
        mask[index] = '*';
    }
    mask[digit_count] = '\0';

    lcd_fill(310U, 78U, 634U, 121U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(310U, 78U, 634U, 121U, UI_COLOR_TOP);
    app_ui_show_centered(310U, 88U, 325U, 24U, mask, WHITE);

    lcd_fill(250U, 128U, 700U, 151U, UI_COLOR_BG);
    app_ui_show_centered(250U, 130U, 450U, 16U, message,
                         is_error ? RED : UI_COLOR_TOP);
    lcd_fill(650U, 82U, 750U, 105U, UI_COLOR_BG);
    app_ui_show_text(650U, 84U, 72U, 16U, 16U, "FAIL:", UI_COLOR_TOP);
    app_ui_show_number(706U, 84U, failed_attempts, 1U, 16U,
                       failed_attempts ? RED : UI_COLOR_TOP);
}

int8_t app_ui_login_key_at(uint16_t x, uint16_t y)
{
    uint16_t local_x;
    uint16_t local_y;
    uint8_t column;
    uint8_t row;
    static const int8_t key_values[4][3] =
    {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {APP_UI_KEY_CLEAR, 0, APP_UI_KEY_ENTER}
    };

    if (x < KEYPAD_X || y < KEYPAD_Y)
    {
        return APP_UI_KEY_NONE;
    }

    local_x = (uint16_t)(x - KEYPAD_X);
    local_y = (uint16_t)(y - KEYPAD_Y);
    column = (uint8_t)(local_x / (KEY_WIDTH + KEY_GAP_X));
    row = (uint8_t)(local_y / (KEY_HEIGHT + KEY_GAP_Y));

    if (column >= 3U || row >= 4U ||
        (local_x % (KEY_WIDTH + KEY_GAP_X)) >= KEY_WIDTH ||
        (local_y % (KEY_HEIGHT + KEY_GAP_Y)) >= KEY_HEIGHT)
    {
        return APP_UI_KEY_NONE;
    }

    return key_values[row][column];
}

void app_ui_show_locked(uint32_t remaining_seconds)
{
    g_cursor_visible = 0U;
    lcd_clear(UI_COLOR_PANEL_DARK);
    lcd_fill(0U, 0U, UI_WIDTH - 1U, UI_TOP_HEIGHT - 1U, RED);
    app_ui_show_centered(0U, 12U, UI_WIDTH, 24U, "LOGIN LOCKED", WHITE);
    app_ui_show_centered(100U, 150U, 600U, 32U, "TOO MANY FAILED ATTEMPTS", WHITE);
    app_ui_show_centered(100U, 220U, 600U, 24U, "PLEASE WAIT", UI_COLOR_MUTED);
    app_ui_update_locked(remaining_seconds);
}

void app_ui_update_locked(uint32_t remaining_seconds)
{
    lcd_fill(330U, 275U, 470U, 335U, UI_COLOR_TOP);
    app_ui_show_number(366U, 289U, remaining_seconds, 2U, 32U, YELLOW);
    app_ui_show_text(424U, 299U, 32U, 16U, 16U, "s", WHITE);
}

void app_ui_show_desktop(uint8_t touch_available,
                         const char *controller_id,
                         uint32_t uptime_seconds)
{
    uint8_t index;

    g_cursor_visible = 0U;
    g_selected_icon = -1;
    lcd_clear(UI_COLOR_BG);
    lcd_fill(0U, 0U, UI_WIDTH - 1U, UI_TOP_HEIGHT - 1U, UI_COLOR_TOP);
    lcd_fill(0U, UI_TOP_HEIGHT, 180U, UI_BOTTOM_Y - 1U, UI_COLOR_PANEL);
    lcd_fill(0U, UI_BOTTOM_Y, UI_WIDTH - 1U, UI_HEIGHT - 1U, UI_COLOR_TOP);

    app_ui_show_text(16U, 12U, 360U, 24U, 24U, "MICRO DESKTOP", WHITE);
    app_ui_show_text(18U, 72U, 145U, 16U, 16U, "SYSTEM READY", GREEN);
    app_ui_show_text(18U, 110U, 145U, 16U, 16U,
                     touch_available ? "TOUCH ONLINE" : "TOUCH OFFLINE",
                     touch_available ? GREEN : RED);
    app_ui_show_text(18U, 144U, 145U, 16U, 16U,
                     "MOUSE WAITING", YELLOW);
    app_ui_show_text(18U, 178U, 65U, 16U, 16U, "CTRL:", UI_COLOR_MUTED);
    if (controller_id != NULL)
    {
        app_ui_show_text(18U, 200U, 145U, 16U, 16U,
                         controller_id, WHITE);
    }
    app_ui_show_text(18U, 240U, 145U, 16U, 16U, "APPS: 6", WHITE);
    app_ui_show_text(18U, 270U, 145U, 16U, 16U, "CURSOR: ACTIVE", WHITE);
    lcd_fill(18U, 306U, 162U, 350U, UI_COLOR_BUTTON);
    lcd_draw_rectangle(18U, 306U, 162U, 350U, WHITE);
    app_ui_show_centered(18U, 320U, 145U, 16U, "SLEEP", WHITE);

    for (index = 0U; index < 6U; index++)
    {
        app_ui_draw_icon(index, 0U);
    }

    app_ui_show_text(16U, 456U, 600U, 16U, 16U,
                     "Select an application icon", WHITE);
    app_ui_update_desktop_time(uptime_seconds);
}

void app_ui_update_desktop_time(uint32_t uptime_seconds)
{
    char time_text[9];
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    app_ui_format_time(uptime_seconds, time_text);
    lcd_fill(652U, 8U, 790U, 39U, UI_COLOR_TOP);
    app_ui_show_text(662U, 12U, 128U, 24U, 24U, time_text, WHITE);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

void app_ui_update_desktop_mouse(uint8_t connection_known,
                                 uint8_t mouse_connected)
{
    const char *status;
    uint16_t color;
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    if (!connection_known)
    {
        status = "MOUSE WAITING";
        color = YELLOW;
    }
    else if (mouse_connected)
    {
        status = "MOUSE ONLINE";
        color = GREEN;
    }
    else
    {
        status = "MOUSE OFFLINE";
        color = RED;
    }

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    lcd_fill(18U, 140U, 162U, 163U, UI_COLOR_PANEL);
    app_ui_show_text(18U, 144U, 145U, 16U, 16U, status, color);
    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

void app_ui_move_cursor(uint16_t x, uint16_t y)
{
    app_ui_cursor_hide();
    app_ui_cursor_show(x, y);
}

void app_ui_set_cursor_size(uint8_t cursor_size)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    if (cursor_size < APP_UI_CURSOR_SIZE_SMALL ||
        cursor_size > APP_UI_CURSOR_SIZE_LARGE ||
        cursor_size == g_cursor_size)
    {
        return;
    }

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    g_cursor_size = cursor_size;
    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

int8_t app_ui_desktop_icon_at(uint16_t x, uint16_t y)
{
    uint8_t index;
    const desktop_icon_t *icon;

    for (index = 0U; index < 6U; index++)
    {
        icon = &g_desktop_icons[index];
        if (x >= icon->x && x < icon->x + icon->width &&
            y >= icon->y && y < icon->y + icon->height)
        {
            return (int8_t)index;
        }
    }

    return -1;
}

void app_ui_select_desktop_icon(int8_t icon_index)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    if (icon_index < 0 || icon_index >= 6)
    {
        return;
    }

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    if (g_selected_icon >= 0)
    {
        app_ui_draw_icon((uint8_t)g_selected_icon, 0U);
    }
    g_selected_icon = icon_index;
    app_ui_draw_icon((uint8_t)g_selected_icon, 1U);

    lcd_fill(200U, 452U, 640U, 475U, UI_COLOR_TOP);
    app_ui_show_text(216U, 456U, 110U, 16U, 16U, "SELECTED:", YELLOW);
    app_ui_show_text(312U, 456U, 150U, 16U, 16U,
                     g_desktop_icons[(uint8_t)g_selected_icon].label, WHITE);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

uint8_t app_ui_desktop_sleep_button_at(uint16_t x, uint16_t y)
{
    return (x >= 18U && x <= 162U && y >= 306U && y <= 350U) ? 1U : 0U;
}

uint8_t app_ui_back_button_at(uint16_t x, uint16_t y)
{
    return (x >= 12U && x <= 108U && y >= 8U && y <= 39U) ? 1U : 0U;
}

uint8_t app_ui_application_sleep_button_at(uint16_t x, uint16_t y)
{
    return (x >= 680U && x <= 788U && y >= 8U && y <= 39U) ? 1U : 0U;
}

void app_ui_show_application(app_ui_application_t application)
{
    const desktop_icon_t *icon;

    if (application >= APP_UI_APP_COUNT)
    {
        return;
    }

    icon = &g_desktop_icons[(uint8_t)application];
    app_ui_draw_application_header(icon->label);

    lcd_fill(300U, 120U, 500U, 300U, UI_COLOR_PANEL);
    lcd_draw_rectangle(300U, 120U, 500U, 300U, UI_COLOR_TOP);
    lcd_fill(365U, 150U, 435U, 220U,
             (uint16_t)(0xF800U + (uint8_t)application * 0x00E0U));
    app_ui_show_centered(300U, 242U, 201U, 24U, icon->label, WHITE);
    app_ui_show_centered(140U, 340U, 520U, 16U,
                         "APPLICATION SHELL READY", UI_COLOR_TOP);
    app_ui_show_centered(140U, 370U, 520U, 16U,
                         "This feature will be implemented later", UI_COLOR_PANEL);
}

static void app_ui_draw_music_button(uint16_t y,
                                     const char *label,
                                     uint8_t enabled)
{
    uint16_t fill_color;
    uint16_t text_color;

    fill_color = enabled ? UI_COLOR_BUTTON : UI_COLOR_PANEL_DARK;
    text_color = enabled ? WHITE : UI_COLOR_MUTED;
    lcd_fill(FILE_BUTTON_X, y,
             FILE_BUTTON_X + FILE_BUTTON_WIDTH - 1U, y + 39U,
             fill_color);
    lcd_draw_rectangle(FILE_BUTTON_X, y,
                       FILE_BUTTON_X + FILE_BUTTON_WIDTH - 1U, y + 39U,
                       enabled ? WHITE : UI_COLOR_MUTED);
    app_ui_show_centered(FILE_BUTTON_X, (uint16_t)(y + 12U),
                         FILE_BUTTON_WIDTH, 16U, label, text_color);
}

static const char *app_ui_music_state_text(app_audio_state_t state)
{
    if (state == APP_AUDIO_STATE_PLAYING)
    {
        return "PLAYING";
    }
    if (state == APP_AUDIO_STATE_PAUSED)
    {
        return "PAUSED";
    }
    if (state == APP_AUDIO_STATE_TEST_TONE)
    {
        return "TEST TONE";
    }
    if (state == APP_AUDIO_STATE_NO_TRACKS)
    {
        return "NO TRACKS";
    }
    if (state == APP_AUDIO_STATE_ERROR)
    {
        return "ERROR";
    }
    if (state == APP_AUDIO_STATE_STARTING)
    {
        return "SCANNING";
    }
    return "STOPPED";
}

static uint32_t app_ui_music_progress(const app_audio_snapshot_t *snapshot)
{
    uint32_t progress;

    progress = 0U;
    if (snapshot->data_size > 0U)
    {
        progress = (uint32_t)(((uint64_t)snapshot->data_loaded * 100U) /
                              snapshot->data_size);
        if (progress > 100U)
        {
            progress = 100U;
        }
    }
    return progress;
}

void app_ui_show_music(const app_audio_snapshot_t *snapshot)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint32_t progress;
    const char *play_label;

    if (snapshot == NULL)
    {
        return;
    }
    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("MUSIC PLAYER");

    lcd_fill(24U, 66U, 590U, 410U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 66U, 590U, 410U, UI_COLOR_MUTED);
    app_ui_show_text(44U, 82U, 160U, 16U, 16U, "CURRENT TRACK", UI_COLOR_MUTED);
    app_ui_show_text(44U, 108U, 510U, 24U, 24U,
                     snapshot->track_name[0] != '\0' ? snapshot->track_name :
                     "NO WAV SELECTED", WHITE);

    app_ui_show_text(44U, 154U, 80U, 16U, 16U, "STATE", UI_COLOR_MUTED);
    app_ui_show_text(140U, 154U, 180U, 16U, 16U,
                     app_ui_music_state_text(snapshot->state),
                     snapshot->state == APP_AUDIO_STATE_ERROR ? RED : YELLOW);
    app_ui_show_text(344U, 154U, 80U, 16U, 16U, "TRACK", UI_COLOR_MUTED);
    if (snapshot->track_count > 0U)
    {
        app_ui_show_u32(424U, 154U,
                        (uint32_t)snapshot->selected_track + 1U, 16U, WHITE);
        app_ui_show_text(448U, 154U, 16U, 16U, 16U, "/", UI_COLOR_MUTED);
        app_ui_show_u32(464U, 154U, snapshot->track_count, 16U, WHITE);
    }
    else
    {
        app_ui_show_text(424U, 154U, 32U, 16U, 16U, "0", WHITE);
    }

    app_ui_show_text(44U, 198U, 112U, 16U, 16U, "PCM FORMAT", UI_COLOR_MUTED);
    if (snapshot->sample_rate > 0U)
    {
        app_ui_show_u32(174U, 198U, snapshot->sample_rate, 16U, WHITE);
        app_ui_show_text(230U, 198U, 40U, 16U, 16U, "HZ", WHITE);
        app_ui_show_u32(302U, 198U, snapshot->bits_per_sample, 16U, WHITE);
        app_ui_show_text(326U, 198U, 40U, 16U, 16U, "BIT", WHITE);
        app_ui_show_text(402U, 198U, 90U, 16U, 16U,
                         snapshot->channels == 1U ? "MONO" : "STEREO", WHITE);
    }
    else
    {
        app_ui_show_text(174U, 198U, 150U, 16U, 16U, "NOT OPENED", WHITE);
    }

    app_ui_show_text(44U, 240U, 80U, 16U, 16U, "VOLUME", UI_COLOR_MUTED);
    app_ui_show_u32(140U, 240U, snapshot->volume_percent, 16U, WHITE);
    app_ui_show_text(176U, 240U, 24U, 16U, 16U, "%", WHITE);
    app_ui_show_text(294U, 240U, 96U, 16U, 16U, "PROGRESS", UI_COLOR_MUTED);
    progress = app_ui_music_progress(snapshot);
    app_ui_show_u32(414U, 240U, progress, 16U, WHITE);
    app_ui_show_text(450U, 240U, 24U, 16U, 16U, "%", WHITE);
    lcd_fill(44U, 274U, 558U, 299U, UI_COLOR_PANEL);
    lcd_draw_rectangle(44U, 274U, 558U, 299U, UI_COLOR_MUTED);
    if (progress > 0U)
    {
        lcd_fill(46U, 276U,
                 (uint16_t)(46U + (510U * progress) / 100U),
                 297U, UI_COLOR_SELECTED);
    }

    app_ui_show_text(44U, 330U, 510U, 16U, 16U,
                     snapshot->status, WHITE);
    app_ui_show_text(44U, 372U, 510U, 16U, 16U,
                     "WAV: PCM 16-BIT, 16/32/44.1/48 KHZ", UI_COLOR_MUTED);

    play_label = (snapshot->state == APP_AUDIO_STATE_PLAYING ||
                  snapshot->state == APP_AUDIO_STATE_TEST_TONE) ? "PAUSE" :
                 (snapshot->state == APP_AUDIO_STATE_PAUSED) ? "RESUME" : "PLAY";
    app_ui_draw_music_button(70U, "PREVIOUS", snapshot->track_count > 0U);
    app_ui_draw_music_button(120U, play_label, snapshot->track_count > 0U ||
                             snapshot->state == APP_AUDIO_STATE_PAUSED);
    app_ui_draw_music_button(170U, "STOP", 1U);
    app_ui_draw_music_button(220U, "NEXT", snapshot->track_count > 0U);
    app_ui_draw_music_button(290U, "TEST 440HZ", 1U);
    app_ui_draw_music_button(350U, "RESCAN SD", 1U);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

void app_ui_update_music(const app_audio_snapshot_t *snapshot,
                         const app_audio_snapshot_t *previous)
{
    uint8_t restore_cursor;
    uint8_t state_changed;
    uint8_t volume_changed;
    uint8_t progress_changed;
    uint8_t status_changed;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint32_t progress;
    uint32_t previous_progress;
    const char *play_label;

    if (snapshot == NULL || previous == NULL)
    {
        return;
    }
    progress = app_ui_music_progress(snapshot);
    previous_progress = app_ui_music_progress(previous);
    state_changed = (snapshot->state != previous->state) ? 1U : 0U;
    volume_changed = (snapshot->volume_percent !=
                      previous->volume_percent) ? 1U : 0U;
    progress_changed = (progress != previous_progress) ? 1U : 0U;
    status_changed = (strcmp(snapshot->status, previous->status) != 0) ?
                     1U : 0U;
    if (!state_changed && !volume_changed && !progress_changed &&
        !status_changed)
    {
        return;
    }

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    if (state_changed)
    {
        lcd_fill(140U, 154U, 319U, 169U, UI_COLOR_PANEL_DARK);
        app_ui_show_text(140U, 154U, 180U, 16U, 16U,
                         app_ui_music_state_text(snapshot->state),
                         snapshot->state == APP_AUDIO_STATE_ERROR ? RED : YELLOW);
        play_label = (snapshot->state == APP_AUDIO_STATE_PLAYING ||
                      snapshot->state == APP_AUDIO_STATE_TEST_TONE) ? "PAUSE" :
                     (snapshot->state == APP_AUDIO_STATE_PAUSED) ? "RESUME" : "PLAY";
        app_ui_draw_music_button(120U, play_label,
                                 snapshot->track_count > 0U ||
                                 snapshot->state == APP_AUDIO_STATE_PAUSED);
    }
    if (volume_changed)
    {
        lcd_fill(140U, 240U, 175U, 255U, UI_COLOR_PANEL_DARK);
        app_ui_show_u32(140U, 240U, snapshot->volume_percent, 16U, WHITE);
    }
    if (progress_changed)
    {
        lcd_fill(414U, 240U, 449U, 255U, UI_COLOR_PANEL_DARK);
        app_ui_show_u32(414U, 240U, progress, 16U, WHITE);
        lcd_fill(44U, 274U, 558U, 299U, UI_COLOR_PANEL);
        lcd_draw_rectangle(44U, 274U, 558U, 299U, UI_COLOR_MUTED);
        if (progress > 0U)
        {
            lcd_fill(46U, 276U,
                     (uint16_t)(46U + (510U * progress) / 100U),
                     297U, UI_COLOR_SELECTED);
        }
    }
    if (status_changed)
    {
        lcd_fill(44U, 330U, 553U, 345U, UI_COLOR_PANEL_DARK);
        app_ui_show_text(44U, 330U, 510U, 16U, 16U,
                         snapshot->status, WHITE);
    }

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

app_ui_music_action_t app_ui_music_action_at(uint16_t x, uint16_t y)
{
    if (x < FILE_BUTTON_X || x >= FILE_BUTTON_X + FILE_BUTTON_WIDTH)
    {
        return APP_UI_MUSIC_ACTION_NONE;
    }
    if (y >= 70U && y < 110U)
    {
        return APP_UI_MUSIC_ACTION_PREVIOUS;
    }
    if (y >= 120U && y < 160U)
    {
        return APP_UI_MUSIC_ACTION_PLAY_PAUSE;
    }
    if (y >= 170U && y < 210U)
    {
        return APP_UI_MUSIC_ACTION_STOP;
    }
    if (y >= 220U && y < 260U)
    {
        return APP_UI_MUSIC_ACTION_NEXT;
    }
    if (y >= 290U && y < 330U)
    {
        return APP_UI_MUSIC_ACTION_TEST_TONE;
    }
    if (y >= 350U && y < 390U)
    {
        return APP_UI_MUSIC_ACTION_RESCAN;
    }
    return APP_UI_MUSIC_ACTION_NONE;
}

static void app_ui_draw_draw_button(uint16_t y,
                                    const char *label,
                                    uint8_t enabled)
{
    uint16_t fill_color;
    uint16_t text_color;

    fill_color = enabled ? UI_COLOR_BUTTON : UI_COLOR_PANEL_DARK;
    text_color = enabled ? WHITE : UI_COLOR_MUTED;
    lcd_fill(DRAW_BUTTON_X, y,
             DRAW_BUTTON_X + DRAW_BUTTON_WIDTH - 1U,
             y + DRAW_BUTTON_HEIGHT - 1U, fill_color);
    lcd_draw_rectangle(DRAW_BUTTON_X, y,
                       DRAW_BUTTON_X + DRAW_BUTTON_WIDTH - 1U,
                       y + DRAW_BUTTON_HEIGHT - 1U,
                       enabled ? WHITE : UI_COLOR_MUTED);
    app_ui_show_centered(DRAW_BUTTON_X, (uint16_t)(y + 13U),
                         DRAW_BUTTON_WIDTH, 16U, label, text_color);
}

static void app_ui_draw_color_button(uint16_t x,
                                     uint16_t y,
                                     uint16_t color,
                                     uint16_t selected_color)
{
    uint16_t border_color;

    border_color = (color == selected_color) ? YELLOW : UI_COLOR_MUTED;
    lcd_fill(x, y, (uint16_t)(x + 57U), (uint16_t)(y + 43U),
             UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(x, y, (uint16_t)(x + 57U),
                       (uint16_t)(y + 43U), border_color);
    lcd_fill((uint16_t)(x + 6U), (uint16_t)(y + 6U),
             (uint16_t)(x + 51U), (uint16_t)(y + 37U), color);
    if (color == BLACK)
    {
        lcd_draw_rectangle((uint16_t)(x + 6U), (uint16_t)(y + 6U),
                           (uint16_t)(x + 51U), (uint16_t)(y + 37U), WHITE);
    }
}

void app_ui_update_draw_controls(uint16_t selected_color,
                                 const char *status,
                                 uint8_t busy)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    app_ui_draw_draw_button(70U, "CLEAR", !busy);
    app_ui_draw_draw_button(128U, "SAVE", !busy);
    app_ui_draw_draw_button(186U, "OPEN", !busy);

    lcd_fill(638U, 244U, 790U, 433U, UI_COLOR_BG);
    app_ui_show_text(648U, 250U, 120U, 16U, 16U, "BRUSH COLOR", UI_COLOR_TOP);
    app_ui_draw_color_button(642U, 280U, BLACK, selected_color);
    app_ui_draw_color_button(714U, 280U, RED, selected_color);
    app_ui_draw_color_button(642U, 340U, GREEN, selected_color);
    app_ui_draw_color_button(714U, 340U, BLUE, selected_color);

    lcd_fill(190U, 452U, 790U, 475U, UI_COLOR_TOP);
    if (busy)
    {
        app_ui_show_text(200U, 456U, 64U, 16U, 16U, "BUSY", YELLOW);
        app_ui_show_text(260U, 456U, 510U, 16U, 16U,
                         status != NULL ? status : "PLEASE WAIT", WHITE);
    }
    else
    {
        app_ui_show_text(200U, 456U, 570U, 16U, 16U,
                         status != NULL ? status : "READY", WHITE);
    }

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

void app_ui_show_draw(uint16_t selected_color,
                      const char *status,
                      uint8_t busy)
{
    app_ui_cursor_hide();
    app_ui_draw_application_header("DRAW");
    lcd_fill(24U, 66U, 616U, 426U, WHITE);
    lcd_draw_rectangle(24U, 66U, 616U, 426U, UI_COLOR_TOP);
    app_ui_update_draw_controls(selected_color, status, busy);
}

void app_ui_draw_stroke(uint16_t x1,
                        uint16_t y1,
                        uint16_t x2,
                        uint16_t y2,
                        uint16_t color)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;
    int32_t delta_x;
    int32_t delta_y;
    uint32_t steps;
    uint32_t step;
    uint16_t x;
    uint16_t y;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    lcd_draw_line(x1, y1, x2, y2, color);

    /* Fill the brush along the segment, not only at its endpoints.  Input
       reports can be several pixels apart during fast movement; endpoint
       dots otherwise leave a visibly thin, dotted-looking stroke. */
    delta_x = (int32_t)x2 - (int32_t)x1;
    delta_y = (int32_t)y2 - (int32_t)y1;
    steps = (uint32_t)((delta_x < 0) ? -delta_x : delta_x);
    if ((uint32_t)((delta_y < 0) ? -delta_y : delta_y) > steps)
    {
        steps = (uint32_t)((delta_y < 0) ? -delta_y : delta_y);
    }
    for (step = 0U; step <= steps; step += 2U)
    {
        x = (uint16_t)((int32_t)x1 +
                       (delta_x * (int32_t)step) / (int32_t)((steps == 0U) ? 1U : steps));
        y = (uint16_t)((int32_t)y1 +
                       (delta_y * (int32_t)step) / (int32_t)((steps == 0U) ? 1U : steps));
        lcd_fill_circle(x, y, 2U, color);
    }
    if (steps > 0U && (step - 2U) != steps)
    {
        lcd_fill_circle(x2, y2, 2U, color);
    }

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

app_ui_draw_action_t app_ui_draw_action_at(uint16_t x, uint16_t y)
{
    if (x >= DRAW_BUTTON_X && x < DRAW_BUTTON_X + DRAW_BUTTON_WIDTH)
    {
        if (y >= 70U && y < 70U + DRAW_BUTTON_HEIGHT)
        {
            return APP_UI_DRAW_ACTION_CLEAR;
        }
        if (y >= 128U && y < 128U + DRAW_BUTTON_HEIGHT)
        {
            return APP_UI_DRAW_ACTION_SAVE;
        }
        if (y >= 186U && y < 186U + DRAW_BUTTON_HEIGHT)
        {
            return APP_UI_DRAW_ACTION_OPEN;
        }
    }

    if (y >= 280U && y < 324U)
    {
        if (x >= 642U && x < 700U)
        {
            return APP_UI_DRAW_ACTION_BLACK;
        }
        if (x >= 714U && x < 772U)
        {
            return APP_UI_DRAW_ACTION_RED;
        }
    }
    if (y >= 340U && y < 384U)
    {
        if (x >= 642U && x < 700U)
        {
            return APP_UI_DRAW_ACTION_GREEN;
        }
        if (x >= 714U && x < 772U)
        {
            return APP_UI_DRAW_ACTION_BLUE;
        }
    }
    return APP_UI_DRAW_ACTION_NONE;
}

static void app_ui_draw_log_button(uint16_t y,
                                   const char *label,
                                   uint8_t enabled)
{
    uint16_t fill_color;
    uint16_t text_color;

    fill_color = enabled ? UI_COLOR_BUTTON : UI_COLOR_PANEL_DARK;
    text_color = enabled ? WHITE : UI_COLOR_MUTED;
    lcd_fill(LOG_BUTTON_X, y,
             LOG_BUTTON_X + LOG_BUTTON_WIDTH - 1U,
             y + LOG_BUTTON_HEIGHT - 1U, fill_color);
    lcd_draw_rectangle(LOG_BUTTON_X, y,
                       LOG_BUTTON_X + LOG_BUTTON_WIDTH - 1U,
                       y + LOG_BUTTON_HEIGHT - 1U,
                       enabled ? WHITE : UI_COLOR_MUTED);
    app_ui_show_centered(LOG_BUTTON_X, (uint16_t)(y + 13U),
                         LOG_BUTTON_WIDTH, 16U, label, text_color);
}

static uint16_t app_ui_log_level_color(app_log_level_t level)
{
    if (level == APP_LOG_LEVEL_ERROR)
    {
        return RED;
    }
    if (level == APP_LOG_LEVEL_WARNING)
    {
        return YELLOW;
    }
    return GREEN;
}

static const char *app_ui_log_level_text(app_log_level_t level)
{
    if (level == APP_LOG_LEVEL_ERROR)
    {
        return "ERROR";
    }
    if (level == APP_LOG_LEVEL_WARNING)
    {
        return "WARN";
    }
    return "INFO";
}

void app_ui_show_logs(const app_log_entry_t *entries,
                      uint8_t entry_count,
                      uint8_t page,
                      const char *status,
                      uint8_t busy,
                      uint8_t clear_armed)
{
    uint8_t row;
    uint8_t index;
    uint8_t page_count;
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint16_t y;
    char time_text[9];
    const app_log_entry_t *entry;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("SYSTEM LOGS");

    app_ui_show_text(28U, 54U, 74U, 16U, 16U, "TIME", UI_COLOR_TOP);
    app_ui_show_text(112U, 54U, 54U, 16U, 16U, "LEVEL", UI_COLOR_TOP);
    app_ui_show_text(176U, 54U, 70U, 16U, 16U, "MODULE", UI_COLOR_TOP);
    app_ui_show_text(256U, 54U, 340U, 16U, 16U, "MESSAGE", UI_COLOR_TOP);

    for (row = 0U; row < APP_UI_LOGS_PAGE_SIZE; row++)
    {
        index = (uint8_t)(page * APP_UI_LOGS_PAGE_SIZE + row);
        y = (uint16_t)(LOG_LIST_Y + row * LOG_ROW_HEIGHT);
        lcd_fill(LOG_LIST_X, y,
                 LOG_LIST_X + LOG_LIST_WIDTH - 1U,
                 y + LOG_ROW_HEIGHT - 5U, UI_COLOR_PANEL_DARK);
        lcd_draw_rectangle(LOG_LIST_X, y,
                           LOG_LIST_X + LOG_LIST_WIDTH - 1U,
                           y + LOG_ROW_HEIGHT - 5U, UI_COLOR_MUTED);
        if (entries != NULL && index < entry_count)
        {
            entry = &entries[index];
            app_ui_format_time(entry->uptime_seconds, time_text);
            app_ui_show_text(28U, (uint16_t)(y + 13U), 76U, 16U, 16U,
                             time_text, WHITE);
            app_ui_show_text(112U, (uint16_t)(y + 13U), 54U, 16U, 16U,
                             app_ui_log_level_text(entry->level),
                             app_ui_log_level_color(entry->level));
            app_ui_show_text(176U, (uint16_t)(y + 13U), 72U, 16U, 16U,
                             entry->module, UI_COLOR_ACCENT);
            app_ui_show_text(256U, (uint16_t)(y + 13U), 334U, 16U, 16U,
                             entry->message, WHITE);
        }
    }

    page_count = (uint8_t)((entry_count + APP_UI_LOGS_PAGE_SIZE - 1U) /
                           APP_UI_LOGS_PAGE_SIZE);
    app_ui_draw_log_button(78U, clear_armed ? "CONFIRM" : "CLEAR", !busy);
    app_ui_draw_log_button(136U, "EXPORT", !busy);
    app_ui_draw_log_button(222U, "PREVIOUS", !busy && page > 0U);
    app_ui_draw_log_button(280U, "NEXT",
                           !busy && page + 1U < page_count);

    app_ui_show_text(638U, 352U, 50U, 16U, 16U, "PAGE", UI_COLOR_TOP);
    app_ui_show_u32(688U, 352U, (uint32_t)page + 1U, 16U, WHITE);
    app_ui_show_text(718U, 352U, 12U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(738U, 352U, page_count ? page_count : 1U, 16U, WHITE);
    app_ui_show_text(638U, 384U, 54U, 16U, 16U, "COUNT", UI_COLOR_TOP);
    app_ui_show_u32(696U, 384U, entry_count, 16U, WHITE);

    lcd_fill(190U, 452U, 790U, 475U, UI_COLOR_TOP);
    if (busy)
    {
        app_ui_show_text(200U, 456U, 64U, 16U, 16U, "BUSY", YELLOW);
        app_ui_show_text(260U, 456U, 510U, 16U, 16U,
                         status != NULL ? status : "PLEASE WAIT", WHITE);
    }
    else
    {
        app_ui_show_text(200U, 456U, 570U, 16U, 16U,
                         status != NULL ? status : "READY", WHITE);
    }

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

app_ui_logs_action_t app_ui_logs_action_at(uint16_t x, uint16_t y)
{
    if (x < LOG_BUTTON_X || x >= LOG_BUTTON_X + LOG_BUTTON_WIDTH)
    {
        return APP_UI_LOGS_ACTION_NONE;
    }
    if (y >= 78U && y < 78U + LOG_BUTTON_HEIGHT)
    {
        return APP_UI_LOGS_ACTION_CLEAR;
    }
    if (y >= 136U && y < 136U + LOG_BUTTON_HEIGHT)
    {
        return APP_UI_LOGS_ACTION_EXPORT;
    }
    if (y >= 222U && y < 222U + LOG_BUTTON_HEIGHT)
    {
        return APP_UI_LOGS_ACTION_PREVIOUS;
    }
    if (y >= 280U && y < 280U + LOG_BUTTON_HEIGHT)
    {
        return APP_UI_LOGS_ACTION_NEXT;
    }
    return APP_UI_LOGS_ACTION_NONE;
}

static void app_ui_draw_settings_button(uint16_t x,
                                        uint16_t y,
                                        uint16_t width,
                                        uint16_t height,
                                        const char *label,
                                        uint8_t selected,
                                        uint8_t enabled)
{
    uint16_t fill_color;
    uint16_t border_color;
    uint16_t text_color;

    if (selected)
    {
        fill_color = UI_COLOR_SELECTED;
        border_color = YELLOW;
        text_color = WHITE;
    }
    else if (enabled)
    {
        fill_color = UI_COLOR_BUTTON;
        border_color = WHITE;
        text_color = WHITE;
    }
    else
    {
        fill_color = UI_COLOR_PANEL_DARK;
        border_color = UI_COLOR_MUTED;
        text_color = UI_COLOR_MUTED;
    }

    lcd_fill(x, y, (uint16_t)(x + width - 1U),
             (uint16_t)(y + height - 1U), fill_color);
    lcd_draw_rectangle(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U), border_color);
    app_ui_show_centered(x, (uint16_t)(y + (height - 16U) / 2U),
                         width, 16U, label, text_color);
}

static void app_ui_draw_time_field(uint16_t x,
                                   uint16_t y,
                                   const char *label,
                                   uint32_t value,
                                   uint8_t digits,
                                   uint8_t enabled)
{
    lcd_fill(x, y, (uint16_t)(x + 239U), (uint16_t)(y + 119U),
             UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(x, y, (uint16_t)(x + 239U),
                       (uint16_t)(y + 119U), UI_COLOR_MUTED);
    app_ui_show_centered(x, (uint16_t)(y + 12U), 240U, 16U,
                         label, UI_COLOR_MUTED);
    app_ui_draw_settings_button((uint16_t)(x + 12U),
                                (uint16_t)(y + 52U),
                                56U, 44U, "-", 0U, enabled);
    app_ui_show_number((uint16_t)(x + 92U), (uint16_t)(y + 61U),
                       value, digits, 24U,
                       enabled ? WHITE : UI_COLOR_MUTED);
    app_ui_draw_settings_button((uint16_t)(x + 172U),
                                (uint16_t)(y + 52U),
                                56U, 44U, "+", 0U, enabled);
}

void app_ui_show_time_settings(const app_rtc_datetime_t *datetime,
                               uint8_t rtc_available)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;
    app_rtc_datetime_t value;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("DATE AND TIME");

    memset(&value, 0, sizeof(value));
    if (datetime != NULL)
    {
        value = *datetime;
    }

    app_ui_draw_time_field(24U, 64U, "YEAR", value.year, 4U,
                           rtc_available);
    app_ui_draw_time_field(280U, 64U, "MONTH", value.month, 2U,
                           rtc_available);
    app_ui_draw_time_field(536U, 64U, "DAY", value.date, 2U,
                           rtc_available);
    app_ui_draw_time_field(24U, 198U, "HOUR", value.hour, 2U,
                           rtc_available);
    app_ui_draw_time_field(280U, 198U, "MINUTE", value.minute, 2U,
                           rtc_available);
    app_ui_draw_time_field(536U, 198U, "SECOND", value.second, 2U,
                           rtc_available);

    app_ui_draw_settings_button(214U, 346U, 170U, 46U, "CANCEL",
                                0U, 1U);
    app_ui_draw_settings_button(416U, 346U, 170U, 46U, "APPLY",
                                0U, rtc_available);
    app_ui_show_centered(190U, 416U, 420U, 16U,
                         rtc_available ? "DS3231 READY" : "DS3231 OFFLINE",
                         rtc_available ? GREEN : RED);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

app_ui_time_action_t app_ui_time_action_at(uint16_t x, uint16_t y)
{
    uint8_t column;
    uint8_t row;
    uint16_t field_x;
    uint16_t field_y;
    static const app_ui_time_action_t down_actions[2][3] =
    {
        {APP_UI_TIME_ACTION_YEAR_DOWN,
         APP_UI_TIME_ACTION_MONTH_DOWN,
         APP_UI_TIME_ACTION_DATE_DOWN},
        {APP_UI_TIME_ACTION_HOUR_DOWN,
         APP_UI_TIME_ACTION_MINUTE_DOWN,
         APP_UI_TIME_ACTION_SECOND_DOWN}
    };
    static const app_ui_time_action_t up_actions[2][3] =
    {
        {APP_UI_TIME_ACTION_YEAR_UP,
         APP_UI_TIME_ACTION_MONTH_UP,
         APP_UI_TIME_ACTION_DATE_UP},
        {APP_UI_TIME_ACTION_HOUR_UP,
         APP_UI_TIME_ACTION_MINUTE_UP,
         APP_UI_TIME_ACTION_SECOND_UP}
    };

    for (row = 0U; row < 2U; row++)
    {
        field_y = row == 0U ? 64U : 198U;
        if (y < field_y + 52U || y >= field_y + 96U)
        {
            continue;
        }
        for (column = 0U; column < 3U; column++)
        {
            field_x = (uint16_t)(24U + column * 256U);
            if (x >= field_x + 12U && x < field_x + 68U)
            {
                return down_actions[row][column];
            }
            if (x >= field_x + 172U && x < field_x + 228U)
            {
                return up_actions[row][column];
            }
        }
    }

    if (y >= 346U && y < 392U)
    {
        if (x >= 214U && x < 384U)
        {
            return APP_UI_TIME_ACTION_CANCEL;
        }
        if (x >= 416U && x < 586U)
        {
            return APP_UI_TIME_ACTION_APPLY;
        }
    }
    return APP_UI_TIME_ACTION_NONE;
}

void app_ui_show_settings(uint8_t cursor_sensitivity,
                          uint8_t cursor_size,
                          uint8_t brightness_percent,
                          uint8_t volume_percent,
                          uint32_t idle_timeout_seconds,
                          uint8_t serial_output_enabled,
                          uint8_t dirty,
                          const char *status,
                          uint8_t busy)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("SYSTEM SETTINGS");

    if (dirty)
    {
        app_ui_show_text(550U, 16U, 96U, 16U, 16U,
                         "UNSAVED", YELLOW);
    }

    lcd_fill(24U, 62U, 388U, 132U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 62U, 388U, 132U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 70U, 260U, 16U, 16U,
                     "CURSOR SENSITIVITY", UI_COLOR_MUTED);
    app_ui_draw_settings_button(40U, 92U, 100U, 32U, "LOW",
        cursor_sensitivity == APP_INPUT_SENSITIVITY_LOW, !busy);
    app_ui_draw_settings_button(148U, 92U, 100U, 32U, "NORMAL",
        cursor_sensitivity == APP_INPUT_SENSITIVITY_NORMAL, !busy);
    app_ui_draw_settings_button(256U, 92U, 100U, 32U, "HIGH",
        cursor_sensitivity == APP_INPUT_SENSITIVITY_HIGH, !busy);

    lcd_fill(412U, 62U, 776U, 132U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(412U, 62U, 776U, 132U, UI_COLOR_MUTED);
    app_ui_show_text(428U, 70U, 230U, 16U, 16U,
                     "CURSOR SIZE", UI_COLOR_MUTED);
    app_ui_draw_settings_button(428U, 92U, 100U, 32U, "SMALL",
        cursor_size == APP_UI_CURSOR_SIZE_SMALL, !busy);
    app_ui_draw_settings_button(536U, 92U, 100U, 32U, "MEDIUM",
        cursor_size == APP_UI_CURSOR_SIZE_MEDIUM, !busy);
    app_ui_draw_settings_button(644U, 92U, 100U, 32U, "LARGE",
        cursor_size == APP_UI_CURSOR_SIZE_LARGE, !busy);

    lcd_fill(24U, 144U, 776U, 210U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 144U, 776U, 210U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 152U, 230U, 16U, 16U,
                     "SCREEN BRIGHTNESS", UI_COLOR_MUTED);
    app_ui_draw_settings_button(40U, 174U, 160U, 30U, "25%",
        brightness_percent == 25U, !busy);
    app_ui_draw_settings_button(220U, 174U, 160U, 30U, "50%",
        brightness_percent == 50U, !busy);
    app_ui_draw_settings_button(400U, 174U, 160U, 30U, "75%",
        brightness_percent == 75U, !busy);
    app_ui_draw_settings_button(580U, 174U, 160U, 30U, "100%",
        brightness_percent == 100U, !busy);

    lcd_fill(24U, 220U, 776U, 286U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 220U, 776U, 286U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 228U, 360U, 16U, 16U,
                     "SCREEN OFF AFTER INACTIVITY", UI_COLOR_MUTED);
    app_ui_draw_settings_button(40U, 250U, 160U, 30U, "OFF",
        idle_timeout_seconds == 0U, !busy);
    app_ui_draw_settings_button(220U, 250U, 160U, 30U, "30 SEC",
        idle_timeout_seconds == 30U, !busy);
    app_ui_draw_settings_button(400U, 250U, 160U, 30U, "60 SEC",
        idle_timeout_seconds == 60U, !busy);
    app_ui_draw_settings_button(580U, 250U, 160U, 30U, "120 SEC",
        idle_timeout_seconds == 120U, !busy);

    lcd_fill(24U, 296U, 388U, 374U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 296U, 388U, 374U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 304U, 230U, 16U, 16U,
                     "SERIAL RTOS STATUS", UI_COLOR_MUTED);
    app_ui_draw_settings_button(40U, 328U, 130U, 32U, "ON",
        serial_output_enabled ? 1U : 0U, !busy);
    app_ui_draw_settings_button(190U, 328U, 130U, 32U, "OFF",
        serial_output_enabled ? 0U : 1U, !busy);

    app_ui_show_text(428U, 304U, 160U, 16U, 16U,
                     "ACTIONS", UI_COLOR_MUTED);
    app_ui_draw_settings_button(430U, 320U, 100U, 42U, "TIME",
                                0U, !busy);
    app_ui_draw_settings_button(545U, 320U, 100U, 42U, "DEFAULTS",
                                0U, !busy);
    app_ui_draw_settings_button(660U, 320U, 100U, 42U, "SAVE",
                                0U, !busy && dirty);

    lcd_fill(24U, 382U, 776U, 442U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 382U, 776U, 442U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 389U, 230U, 16U, 16U,
                     "SYSTEM VOLUME", UI_COLOR_MUTED);
    app_ui_draw_settings_button(40U, 408U, 120U, 28U, "0%",
        volume_percent == 0U, !busy);
    app_ui_draw_settings_button(184U, 408U, 120U, 28U, "25%",
        volume_percent == 25U, !busy);
    app_ui_draw_settings_button(328U, 408U, 120U, 28U, "50%",
        volume_percent == 50U, !busy);
    app_ui_draw_settings_button(472U, 408U, 120U, 28U, "75%",
        volume_percent == 75U, !busy);
    app_ui_draw_settings_button(616U, 408U, 120U, 28U, "100%",
        volume_percent == 100U, !busy);

    lcd_fill(190U, 452U, 790U, 475U, UI_COLOR_TOP);
    if (busy)
    {
        app_ui_show_text(200U, 456U, 64U, 16U, 16U, "BUSY", YELLOW);
        app_ui_show_text(260U, 456U, 510U, 16U, 16U,
                         status != NULL ? status : "PLEASE WAIT", WHITE);
    }
    else
    {
        app_ui_show_text(200U, 456U, 570U, 16U, 16U,
                         status != NULL ? status : "READY", WHITE);
    }

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

app_ui_settings_action_t app_ui_settings_action_at(uint16_t x, uint16_t y)
{
    if (y >= 92U && y < 124U)
    {
        if (x >= 40U && x < 140U)
        {
            return APP_UI_SETTINGS_ACTION_SENSITIVITY_LOW;
        }
        if (x >= 148U && x < 248U)
        {
            return APP_UI_SETTINGS_ACTION_SENSITIVITY_NORMAL;
        }
        if (x >= 256U && x < 356U)
        {
            return APP_UI_SETTINGS_ACTION_SENSITIVITY_HIGH;
        }
        if (x >= 428U && x < 528U)
        {
            return APP_UI_SETTINGS_ACTION_CURSOR_SMALL;
        }
        if (x >= 536U && x < 636U)
        {
            return APP_UI_SETTINGS_ACTION_CURSOR_MEDIUM;
        }
        if (x >= 644U && x < 744U)
        {
            return APP_UI_SETTINGS_ACTION_CURSOR_LARGE;
        }
    }
    if (y >= 174U && y < 204U)
    {
        if (x >= 40U && x < 200U)
        {
            return APP_UI_SETTINGS_ACTION_BRIGHTNESS_25;
        }
        if (x >= 220U && x < 380U)
        {
            return APP_UI_SETTINGS_ACTION_BRIGHTNESS_50;
        }
        if (x >= 400U && x < 560U)
        {
            return APP_UI_SETTINGS_ACTION_BRIGHTNESS_75;
        }
        if (x >= 580U && x < 740U)
        {
            return APP_UI_SETTINGS_ACTION_BRIGHTNESS_100;
        }
    }
    if (y >= 250U && y < 280U)
    {
        if (x >= 40U && x < 200U)
        {
            return APP_UI_SETTINGS_ACTION_SCREEN_OFF_DISABLED;
        }
        if (x >= 220U && x < 380U)
        {
            return APP_UI_SETTINGS_ACTION_SCREEN_OFF_30;
        }
        if (x >= 400U && x < 560U)
        {
            return APP_UI_SETTINGS_ACTION_SCREEN_OFF_60;
        }
        if (x >= 580U && x < 740U)
        {
            return APP_UI_SETTINGS_ACTION_SCREEN_OFF_120;
        }
    }
    if (y >= 408U && y < 436U)
    {
        if (x >= 40U && x < 160U)
        {
            return APP_UI_SETTINGS_ACTION_VOLUME_0;
        }
        if (x >= 184U && x < 304U)
        {
            return APP_UI_SETTINGS_ACTION_VOLUME_25;
        }
        if (x >= 328U && x < 448U)
        {
            return APP_UI_SETTINGS_ACTION_VOLUME_50;
        }
        if (x >= 472U && x < 592U)
        {
            return APP_UI_SETTINGS_ACTION_VOLUME_75;
        }
        if (x >= 616U && x < 736U)
        {
            return APP_UI_SETTINGS_ACTION_VOLUME_100;
        }
    }
    if (y >= 328U && y < 360U)
    {
        if (x >= 40U && x < 170U)
        {
            return APP_UI_SETTINGS_ACTION_SERIAL_ON;
        }
        if (x >= 190U && x < 320U)
        {
            return APP_UI_SETTINGS_ACTION_SERIAL_OFF;
        }
    }
    if (y >= 320U && y < 362U)
    {
        if (x >= 430U && x < 530U)
        {
            return APP_UI_SETTINGS_ACTION_TIME;
        }
        if (x >= 545U && x < 645U)
        {
            return APP_UI_SETTINGS_ACTION_DEFAULTS;
        }
        if (x >= 660U && x < 760U)
        {
            return APP_UI_SETTINGS_ACTION_SAVE;
        }
    }
    return APP_UI_SETTINGS_ACTION_NONE;
}

static void app_ui_draw_file_button(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    const char *label,
                                    uint8_t enabled)
{
    uint16_t fill_color;
    uint16_t text_color;

    fill_color = enabled ? UI_COLOR_BUTTON : UI_COLOR_PANEL_DARK;
    text_color = enabled ? WHITE : UI_COLOR_MUTED;
    lcd_fill(x, y, (uint16_t)(x + width - 1U),
             (uint16_t)(y + FILE_BUTTON_HEIGHT - 1U), fill_color);
    lcd_draw_rectangle(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + FILE_BUTTON_HEIGHT - 1U),
                       enabled ? WHITE : UI_COLOR_MUTED);
    app_ui_show_centered(x, (uint16_t)(y + 13U), width, 16U,
                         label, text_color);
}

static void app_ui_show_multiline(uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  const char *text,
                                  uint16_t color)
{
    uint16_t start_x;
    uint16_t end_x;
    uint16_t end_y;
    char character;

    if (text == NULL)
    {
        return;
    }

    start_x = x;
    end_x = (uint16_t)(x + width);
    end_y = (uint16_t)(y + height);
    while (*text != '\0' && (uint16_t)(y + 16U) <= end_y)
    {
        character = *text++;
        if (character == '\r')
        {
            continue;
        }
        if (character == '\n')
        {
            x = start_x;
            y = (uint16_t)(y + 18U);
            continue;
        }
        if ((uint16_t)(x + 8U) > end_x)
        {
            x = start_x;
            y = (uint16_t)(y + 18U);
            if ((uint16_t)(y + 16U) > end_y)
            {
                break;
            }
        }
        if (character < ' ' || character > '~')
        {
            character = '.';
        }
        lcd_show_char(x, y, character, 16U, 1U, color);
        x = (uint16_t)(x + 8U);
    }
}

static void app_ui_show_files_status(const char *status, uint8_t busy)
{
    lcd_fill(190U, 452U, 790U, 475U, UI_COLOR_TOP);
    if (busy)
    {
        app_ui_show_text(200U, 456U, 64U, 16U, 16U, "BUSY", YELLOW);
        app_ui_show_text(260U, 456U, 510U, 16U, 16U,
                         status != NULL ? status : "PLEASE WAIT", WHITE);
    }
    else
    {
        app_ui_show_text(200U, 456U, 570U, 16U, 16U,
                         status != NULL ? status : "READY", WHITE);
    }
}

void app_ui_show_files_list(const app_storage_file_t *files,
                            uint8_t file_count,
                            uint8_t page,
                            int8_t selected_file,
                            const char *status,
                            uint8_t busy)
{
    uint8_t row;
    uint8_t index;
    uint8_t restore_cursor;
    uint8_t page_count;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint16_t y;
    uint16_t fill_color;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("FILE MANAGER");

    app_ui_show_text(40U, 54U, 130U, 16U, 16U, "NAME", UI_COLOR_TOP);
    app_ui_show_text(220U, 54U, 70U, 16U, 16U, "TYPE", UI_COLOR_TOP);
    app_ui_show_text(324U, 54U, 80U, 16U, 16U, "BYTES", UI_COLOR_TOP);
    app_ui_show_text(430U, 54U, 140U, 16U, 16U, "LOCATION", UI_COLOR_TOP);

    for (row = 0U; row < APP_UI_FILES_PAGE_SIZE; row++)
    {
        index = (uint8_t)(page * APP_UI_FILES_PAGE_SIZE + row);
        y = (uint16_t)(FILE_LIST_Y + row * FILE_ROW_HEIGHT);
        fill_color = ((int8_t)index == selected_file) ?
                     UI_COLOR_SELECTED : UI_COLOR_PANEL_DARK;
        lcd_fill(FILE_LIST_X, y,
                 (uint16_t)(FILE_LIST_X + FILE_LIST_WIDTH - 1U),
                 (uint16_t)(y + FILE_ROW_HEIGHT - 7U), fill_color);
        lcd_draw_rectangle(FILE_LIST_X, y,
                           (uint16_t)(FILE_LIST_X + FILE_LIST_WIDTH - 1U),
                           (uint16_t)(y + FILE_ROW_HEIGHT - 7U),
                           ((int8_t)index == selected_file) ? YELLOW : UI_COLOR_MUTED);
        if (files != NULL && index < file_count)
        {
            app_ui_show_text(40U, (uint16_t)(y + 15U), 150U, 16U, 16U,
                             files[index].name, WHITE);
            app_ui_show_text(220U, (uint16_t)(y + 15U), 72U, 16U, 16U,
                             files[index].type, UI_COLOR_MUTED);
            app_ui_show_u32(324U, (uint16_t)(y + 15U),
                            files[index].size, 16U, WHITE);
            app_ui_show_text(430U, (uint16_t)(y + 15U), 140U, 16U, 16U,
                             files[index].location, UI_COLOR_MUTED);
        }
    }

    app_ui_draw_file_button(FILE_BUTTON_X, 70U, FILE_BUTTON_WIDTH, "NEW", !busy);
    app_ui_draw_file_button(FILE_BUTTON_X, 128U, FILE_BUTTON_WIDTH, "OPEN",
                            !busy && selected_file >= 0);
    app_ui_draw_file_button(FILE_BUTTON_X, 186U, FILE_BUTTON_WIDTH, "DELETE",
                            !busy && selected_file >= 0);
    app_ui_draw_file_button(FILE_BUTTON_X, 244U, FILE_BUTTON_WIDTH, "PREVIOUS",
                            !busy && page > 0U);
    page_count = (uint8_t)((file_count + APP_UI_FILES_PAGE_SIZE - 1U) /
                           APP_UI_FILES_PAGE_SIZE);
    app_ui_draw_file_button(FILE_BUTTON_X, 302U, FILE_BUTTON_WIDTH, "NEXT",
                            !busy && page + 1U < page_count);
    app_ui_draw_file_button(FILE_BUTTON_X, 360U, FILE_BUTTON_WIDTH, "REFRESH", !busy);

    app_ui_show_text(620U, 414U, 44U, 16U, 16U, "PAGE", UI_COLOR_TOP);
    app_ui_show_u32(668U, 414U, (uint32_t)page + 1U, 16U, UI_COLOR_TOP);
    app_ui_show_text(700U, 414U, 12U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(720U, 414U, page_count ? page_count : 1U, 16U, UI_COLOR_TOP);
    app_ui_show_files_status(status, busy);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

void app_ui_show_file_content(const char *name,
                              const char *content,
                              uint32_t file_size,
                              uint8_t dirty,
                              const char *status,
                              uint8_t busy)
{
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();
    app_ui_draw_application_header("FILE VIEWER");

    lcd_fill(24U, 66U, 775U, 330U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(24U, 66U, 775U, 330U, UI_COLOR_MUTED);
    app_ui_show_text(40U, 80U, 60U, 16U, 16U, "NAME:", UI_COLOR_MUTED);
    app_ui_show_text(96U, 80U, 160U, 16U, 16U,
                     name != NULL ? name : "", WHITE);
    app_ui_show_text(300U, 80U, 64U, 16U, 16U, "SIZE:", UI_COLOR_MUTED);
    app_ui_show_u32(356U, 80U, file_size, 16U, WHITE);
    if (dirty)
    {
        app_ui_show_text(650U, 80U, 96U, 16U, 16U, "MODIFIED", YELLOW);
    }
    lcd_draw_rectangle(38U, 108U, 761U, 314U, UI_COLOR_TOP);
    app_ui_show_multiline(48U, 120U, 700U, 180U,
                          content != NULL ? content : "", WHITE);

    app_ui_draw_file_button(40U, 366U, 150U, "EDIT", !busy);
    app_ui_draw_file_button(220U, 366U, 150U, "SAVE", !busy && dirty);
    app_ui_draw_file_button(400U, 366U, 150U, "DELETE", !busy);
    app_ui_draw_file_button(580U, 366U, 150U, "LIST", !busy);
    app_ui_show_files_status(status, busy);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}

int8_t app_ui_files_row_at(uint16_t x, uint16_t y)
{
    uint16_t relative_y;
    uint8_t row;

    if (x < FILE_LIST_X || x >= FILE_LIST_X + FILE_LIST_WIDTH ||
        y < FILE_LIST_Y)
    {
        return -1;
    }
    relative_y = (uint16_t)(y - FILE_LIST_Y);
    row = (uint8_t)(relative_y / FILE_ROW_HEIGHT);
    if (row >= APP_UI_FILES_PAGE_SIZE ||
        (relative_y % FILE_ROW_HEIGHT) >= FILE_ROW_HEIGHT - 6U)
    {
        return -1;
    }
    return (int8_t)row;
}

app_ui_files_action_t app_ui_files_action_at(uint16_t x,
                                             uint16_t y,
                                             uint8_t content_view)
{
    if (content_view)
    {
        if (y < 366U || y >= 366U + FILE_BUTTON_HEIGHT)
        {
            return APP_UI_FILES_ACTION_NONE;
        }
        if (x >= 40U && x < 190U)
        {
            return APP_UI_FILES_ACTION_EDIT;
        }
        if (x >= 220U && x < 370U)
        {
            return APP_UI_FILES_ACTION_SAVE;
        }
        if (x >= 400U && x < 550U)
        {
            return APP_UI_FILES_ACTION_DELETE;
        }
        if (x >= 580U && x < 730U)
        {
            return APP_UI_FILES_ACTION_LIST;
        }
        return APP_UI_FILES_ACTION_NONE;
    }

    if (x < FILE_BUTTON_X || x >= FILE_BUTTON_X + FILE_BUTTON_WIDTH)
    {
        return APP_UI_FILES_ACTION_NONE;
    }
    if (y >= 70U && y < 70U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_NEW;
    }
    if (y >= 128U && y < 128U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_OPEN;
    }
    if (y >= 186U && y < 186U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_DELETE;
    }
    if (y >= 244U && y < 244U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_PREVIOUS;
    }
    if (y >= 302U && y < 302U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_NEXT;
    }
    if (y >= 360U && y < 360U + FILE_BUTTON_HEIGHT)
    {
        return APP_UI_FILES_ACTION_REFRESH;
    }
    return APP_UI_FILES_ACTION_NONE;
}

void app_ui_show_monitor(const app_monitor_snapshot_t *snapshot)
{
    app_ui_draw_application_header("SYSTEM MONITOR");
    app_ui_draw_metric_panel(40U, 72U, "UPTIME");
    app_ui_draw_metric_panel(410U, 72U, "FREE HEAP (BYTES)");
    app_ui_draw_metric_panel(40U, 182U, "INPUT EVENTS");
    app_ui_draw_metric_panel(410U, 182U, "DROPPED EVENTS");
    app_ui_draw_metric_panel(40U, 292U, "QUEUE NOW / PEAK");
    app_ui_draw_metric_panel(410U, 292U, "STACK FREE I / G / M");
    lcd_fill(40U, 390U, 759U, 439U, UI_COLOR_PANEL_DARK);
    lcd_draw_rectangle(40U, 390U, 759U, 439U, UI_COLOR_MUTED);
    app_ui_update_monitor(snapshot);
}

void app_ui_update_monitor(const app_monitor_snapshot_t *snapshot)
{
    char time_text[9];
    uint8_t restore_cursor;
    uint16_t cursor_x;
    uint16_t cursor_y;

    if (snapshot == NULL)
    {
        return;
    }

    restore_cursor = g_cursor_visible;
    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    app_ui_cursor_hide();

    lcd_fill(54U, 110U, 374U, 148U, UI_COLOR_PANEL_DARK);
    lcd_fill(424U, 110U, 744U, 148U, UI_COLOR_PANEL_DARK);
    lcd_fill(54U, 220U, 374U, 258U, UI_COLOR_PANEL_DARK);
    lcd_fill(424U, 220U, 744U, 258U, UI_COLOR_PANEL_DARK);
    lcd_fill(54U, 330U, 374U, 368U, UI_COLOR_PANEL_DARK);
    lcd_fill(424U, 330U, 744U, 368U, UI_COLOR_PANEL_DARK);

    app_ui_format_time(snapshot->uptime_seconds, time_text);
    app_ui_show_text(64U, 116U, 190U, 24U, 24U, time_text, WHITE);
    app_ui_show_u32(434U, 116U, snapshot->free_heap_bytes, 24U, GREEN);
    app_ui_show_u32(64U, 226U, snapshot->input_event_count, 24U, WHITE);
    app_ui_show_u32(434U, 226U, snapshot->dropped_event_count, 24U,
                    snapshot->dropped_event_count ? RED : GREEN);

    app_ui_show_u32(64U, 336U, snapshot->queue_depth, 24U, WHITE);
    app_ui_show_text(120U, 336U, 24U, 24U, 24U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(148U, 336U, snapshot->queue_high_water, 24U, WHITE);

    app_ui_show_u32(434U, 336U, snapshot->input_stack_watermark, 16U, WHITE);
    app_ui_show_text(482U, 336U, 16U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(502U, 336U, snapshot->runtime_stack_watermark, 16U, WHITE);
    app_ui_show_text(550U, 336U, 16U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(570U, 336U, snapshot->monitor_stack_watermark, 16U, WHITE);

    lcd_fill(48U, 396U, 751U, 433U, UI_COLOR_PANEL_DARK);
    app_ui_show_text(52U, 397U, 48U, 16U, 16U, "MOUSE", UI_COLOR_MUTED);
    app_ui_show_text(104U, 397U, 64U, 16U, 16U,
                     snapshot->ch9350_connection_known ?
                         (snapshot->ch9350_mouse_connected ? "ONLINE" : "OFFLINE") :
                         "WAIT",
                     snapshot->ch9350_connection_known ?
                         (snapshot->ch9350_mouse_connected ? GREEN : RED) :
                         YELLOW);
    app_ui_show_text(184U, 397U, 56U, 16U, 16U, "REPORT", UI_COLOR_MUTED);
    app_ui_show_u32(244U, 397U, snapshot->ch9350_mouse_report_count, 16U, WHITE);
    app_ui_show_text(360U, 397U, 40U, 16U, 16U, "STATE", UI_COLOR_MUTED);
    app_ui_show_u32(404U, 397U, snapshot->ch9350_state_frame_count, 16U, WHITE);
    app_ui_show_text(520U, 397U, 32U, 16U, 16U, "C/D", UI_COLOR_MUTED);
    app_ui_show_u32(556U, 397U, snapshot->ch9350_connect_event_count, 16U, WHITE);
    app_ui_show_text(636U, 397U, 8U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(648U, 397U, snapshot->ch9350_disconnect_event_count, 16U, WHITE);

    app_ui_show_text(52U, 417U, 88U, 16U, 16U, "ERR F/S/U", UI_COLOR_MUTED);
    app_ui_show_u32(144U, 417U, snapshot->ch9350_discarded_frame_count, 16U,
                    snapshot->ch9350_discarded_frame_count ? RED : GREEN);
    app_ui_show_text(224U, 417U, 8U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(236U, 417U, snapshot->ch9350_sync_error_count, 16U,
                    snapshot->ch9350_sync_error_count ? RED : GREEN);
    app_ui_show_text(316U, 417U, 8U, 16U, 16U, "/", UI_COLOR_MUTED);
    app_ui_show_u32(328U, 417U, snapshot->ch9350_uart_dropped_count, 16U,
                    snapshot->ch9350_uart_dropped_count ? RED : GREEN);

    if (restore_cursor)
    {
        app_ui_cursor_show(cursor_x, cursor_y);
    }
}
