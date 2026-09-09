/**
 * @file app_screen.c
 * @brief 管理屏幕刷新边界以及显示休眠、唤醒等屏幕状态。
 * @details 这是 app_screen 模块的实现文件（App/app_screen.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_screen.h"

#include "./BSP/LCD/lcd.h"
#include "FreeRTOS.h"
#include "task.h"

static uint8_t g_screen_off;
static uint8_t g_brightness_percent = 100U;

/**
 * @brief app_screen_apply_brightness：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_screen_apply_brightness(void)
{
    TIM3->CCR2 = g_screen_off ? 0U :
        (uint32_t)g_brightness_percent * 10U;
}

/**
 * @brief app_screen_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_screen_off：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_screen_off(void)
{
    taskENTER_CRITICAL();
    g_screen_off = 1U;
    app_screen_apply_brightness();
    taskEXIT_CRITICAL();
}

/**
 * @brief app_screen_on：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_screen_on(void)
{
    taskENTER_CRITICAL();
    g_screen_off = 0U;
    app_screen_apply_brightness();
    taskEXIT_CRITICAL();
}

/**
 * @brief app_screen_is_off：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_screen_is_off(void)
{
    uint8_t is_off;

    taskENTER_CRITICAL();
    is_off = g_screen_off;
    taskEXIT_CRITICAL();
    return is_off;
}

/**
 * @brief app_screen_set_brightness：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param brightness_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_screen_get_brightness：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_screen_get_brightness(void)
{
    uint8_t brightness_percent;

    taskENTER_CRITICAL();
    brightness_percent = g_brightness_percent;
    taskEXIT_CRITICAL();
    return brightness_percent;
}
