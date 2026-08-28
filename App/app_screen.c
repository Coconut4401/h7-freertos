#include "app_screen.h"

#include "./BSP/LCD/lcd.h"
#include "FreeRTOS.h"
#include "task.h"

static uint8_t g_screen_off;
static uint8_t g_brightness_percent = 100U;

static void app_screen_apply_brightness(void)
{
    TIM3->CCR2 = g_screen_off ? 0U :
        (uint32_t)g_brightness_percent * 10U;
}

void app_screen_init(void)
{
    taskENTER_CRITICAL();
    LCD_BL_GPIO_CLK_ENABLE();
    RCC->APB1LENR |= RCC_APB1LENR_TIM3EN;

    TIM3->CR1 = 0U;
    TIM3->PSC = 9U;
    TIM3->ARR = 999U;
    TIM3->CCMR1 &= ~(TIM_CCMR1_CC2S | TIM_CCMR1_OC2M);
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE |
                   TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
    TIM3->CCER &= ~(TIM_CCER_CC2P | TIM_CCER_CC2NP);
    TIM3->CCER |= TIM_CCER_CC2E;
    TIM3->EGR = TIM_EGR_UG;

    sys_gpio_set(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP,
                 SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);
    sys_gpio_af_set(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, 2U);

    g_screen_off = 0U;
    app_screen_apply_brightness();
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    taskEXIT_CRITICAL();
}

void app_screen_off(void)
{
    taskENTER_CRITICAL();
    g_screen_off = 1U;
    app_screen_apply_brightness();
    taskEXIT_CRITICAL();
}

void app_screen_on(void)
{
    taskENTER_CRITICAL();
    g_screen_off = 0U;
    app_screen_apply_brightness();
    taskEXIT_CRITICAL();
}

uint8_t app_screen_is_off(void)
{
    uint8_t is_off;

    taskENTER_CRITICAL();
    is_off = g_screen_off;
    taskEXIT_CRITICAL();
    return is_off;
}

void app_screen_set_brightness(uint8_t brightness_percent)
{
    if (brightness_percent < 1U || brightness_percent > 100U)
    {
        return;
    }

    taskENTER_CRITICAL();
    g_brightness_percent = brightness_percent;
    app_screen_apply_brightness();
    taskEXIT_CRITICAL();
}

uint8_t app_screen_get_brightness(void)
{
    uint8_t brightness_percent;

    taskENTER_CRITICAL();
    brightness_percent = g_brightness_percent;
    taskEXIT_CRITICAL();
    return brightness_percent;
}
