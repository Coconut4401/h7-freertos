/**
 * @file app_diagnostics.c
 * @brief 鏀堕泦骞舵樉绀哄惎鍔ㄨ瘖鏂�淇℃伅锛岃緟鍔╁畾浣嶅�栬�惧拰绯荤粺鍒濆�嬪寲鏁呴殰銆�
 * @details 杩欐槸 app_diagnostics 妯″潡鐨勫疄鐜版枃浠讹紙App/app_diagnostics.c锛夈€傝皟鐢ㄦ湰妯″潡鎺ュ彛鏃讹紝搴旈伒瀹�
 *          鐩稿簲澶栬�惧垵濮嬪寲椤哄簭銆佺紦鍐插尯鏈夋晥鏈熷拰 FreeRTOS 浠诲姟涓婁笅鏂囩害鏉熴€�
 * @note 鏂囦欢閲囩敤 UTF-8 缂栫爜锛涚‖浠惰祫婧愬垎閰嶄互鏉跨骇鍘熺悊鍥惧拰宸ョ▼閰嶇疆涓哄噯銆�
 */

#include "app_diagnostics.h"

#include <stdint.h>

#include "app_input.h"
#include "app_logs.h"

#if APP_DIAGNOSTIC_MODE == 1U
/**
 * @brief app_diagnostics_overflow锛氬畬鎴愯�ユ帴鍙ｈ礋璐ｇ殑妯″潡鎿嶄綔锛屽苟淇濇寔鐩稿叧纭�浠朵笌杞�浠剁姸鎬佷竴鑷淬€�
 * @details 姝ゅ�勪负鎺ュ彛瀹炵幇锛涙墽琛岄『搴忔部鐢ㄦā鍧楁棦鏈夎�捐�°€傛秹鍙婂叡浜�鐘舵€佹椂锛岃皟鐢ㄦ柟闇€淇濊瘉
 *          鍒濆�嬪寲宸茬粡瀹屾垚锛屽苟閬垮厤涓庝腑鏂�鎴栧叾浠栦换鍔′骇鐢熸湭鍙楁帶鐨勫苟鍙戣�块棶銆�
 * @param depth 璋冪敤鏂规彁渚涚殑杈撳叆鎴栬緭鍑哄弬鏁帮紱鍏跺彇鍊艰寖鍥村拰缂撳啿鍖烘湁鏁堟湡椤荤�﹀悎鎺ュ彛绾﹀畾銆�
 * @return 鏃犺繑鍥炲€笺€�
 */
__attribute__((noinline)) static void app_diagnostics_overflow(uint32_t depth)
{
    volatile uint8_t padding[256];
    uint32_t index;

    for (index = 0U; index < sizeof(padding); index++)
    {
        padding[index] = (uint8_t)(depth + index);
    }
    app_diagnostics_overflow(depth + padding[depth & 0xFFU]);
}
#endif

/**
 * @brief AppDiagnosticsTask锛氫綔涓� FreeRTOS 浠诲姟鍏ュ彛锛屽惊鐜�澶勭悊浜嬩欢銆佸懆鏈熷伐浣滃拰杩愯�岀姸鎬併€�
 * @details 姝ゅ�勪负鎺ュ彛瀹炵幇锛涙墽琛岄『搴忔部鐢ㄦā鍧楁棦鏈夎�捐�°€傛秹鍙婂叡浜�鐘舵€佹椂锛岃皟鐢ㄦ柟闇€淇濊瘉
 *          鍒濆�嬪寲宸茬粡瀹屾垚锛屽苟閬垮厤涓庝腑鏂�鎴栧叾浠栦换鍔′骇鐢熸湭鍙楁帶鐨勫苟鍙戣�块棶銆�
 * @param argument 璋冪敤鏂规彁渚涚殑杈撳叆鎴栬緭鍑哄弬鏁帮紱鍏跺彇鍊艰寖鍥村拰缂撳啿鍖烘湁鏁堟湡椤荤�﹀悎鎺ュ彛绾﹀畾銆�
 * @return 鏃犺繑鍥炲€笺€�
 * @warning 璇ュ叆鍙ｅ叿鏈夌壒瀹氫腑鏂�鎴栦换鍔′笂涓嬫枃锛岀�佹�㈡墽琛屼笉绗﹀悎璇ヤ笂涓嬫枃绾︽潫鐨勬搷浣溿€�
 */
void AppDiagnosticsTask(void *argument)
{
    const app_diagnostics_context_t *context;

    context = (const app_diagnostics_context_t *)argument;
    vTaskDelay(pdMS_TO_TICKS(5000U));

#if APP_DIAGNOSTIC_MODE == 1U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "TRIGGER STACK OVERFLOW");
    app_diagnostics_overflow(1U);
#elif APP_DIAGNOSTIC_MODE == 2U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "TRIGGER HEAP EXHAUSTION");
    while (pvPortMalloc(4096U) != NULL)
    {
    }
#elif APP_DIAGNOSTIC_MODE == 3U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "SUSPEND INPUT TASK");
    vTaskSuspend(context->input_task);
#elif APP_DIAGNOSTIC_MODE == 4U
    {
        app_input_event_t event;

        event.type = APP_INPUT_EVENT_MOVE;
        event.source = APP_INPUT_SOURCE_TOUCH;
        event.x = APP_LCD_WIDTH / 2U;
        event.y = APP_LCD_HEIGHT / 2U;
        event.wheel = 0;
        event.buttons = 0U;
        event.tick = (uint32_t)xTaskGetTickCount();
        while (xQueueSend(context->input_queue, &event, 0U) == pdPASS)
        {
        }
        event.type = APP_INPUT_EVENT_BACK;
        (void)app_input_post_event(context->input_queue, &event);
        vTaskDelay(pdMS_TO_TICKS(1000U));
        (void)app_input_post_event(context->input_queue, &event);
        app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG",
                     "INPUT QUEUE RECOVERY TESTED");
    }
#else
    (void)context;
#endif

    vTaskDelete(NULL);
}
