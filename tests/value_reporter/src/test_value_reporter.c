#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>
#include <string.h>
#include "value_reporter.h"

static void value_reporter_test_before(void *fixture)
{
    //memset(report_periods, 0, sizeof(report_periods));
}

/* -------------------------------------------------------------------------- */
/* 測試案例 (Test Cases)                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief 測試：成功設定一個回報週期
 * 期望：
 * 1. 函式回傳 0 (成功)
 * 2. report_periods 陣列中對應的元素被正確更新
 */
ZTEST(value_reporter_suite, test_set_period_success)
{
    uint8_t sensor_id = 2;
    uint8_t reg_addr = FREQ;
    uint32_t period_ms = 1000;

    // 執行要測試的函式
    uint32_t period = value_reporter_set_report_period(sensor_id, reg_addr, period_ms);

    // 驗證回傳值
    zassert_equal(period, period_ms, "Return value should be the same as input");
}

ZTEST(value_reporter_suite, test_reg_not_in_range)
{
    uint8_t sensor_id = 2;
    uint8_t reg_addr = 0x39;
    uint32_t period_ms = 1000;

    // 執行要測試的函式
    uint32_t period = value_reporter_set_report_period(sensor_id, reg_addr, period_ms);

    // 驗證回傳值
    zassert_equal(period, PERIOD_NOT_VALIDE, "Return value should be the same as input");

    reg_addr = 0x70;

    // 執行要測試的函式
    period = value_reporter_set_report_period(sensor_id, reg_addr, period_ms);

    // 驗證回傳值
    zassert_equal(period, PERIOD_NOT_VALIDE, "Return value should be the same as input");
}

ZTEST(value_reporter_suite, test_sensor_id_not_in_range)
{
    uint8_t sensor_id = -1;
    uint8_t reg_addr = FREQ;
    uint32_t period_ms = 1000;

    // 執行要測試的函式
    uint32_t period = value_reporter_set_report_period(sensor_id, reg_addr, period_ms);

    // 驗證回傳值
    zassert_equal(period, PERIOD_NOT_VALIDE, "Return value should be the same as input");

    sensor_id = 3;

    // 執行要測試的函式
    period = value_reporter_set_report_period(sensor_id, reg_addr, period_ms);

    // 驗證回傳值
    zassert_equal(period, PERIOD_NOT_VALIDE, "Return value should be the same as input");
}

ZTEST_SUITE(value_reporter_suite, NULL, NULL, value_reporter_test_before, NULL, NULL);
