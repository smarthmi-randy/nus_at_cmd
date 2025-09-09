#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>
#include <string.h>
//#include "value_reporter.h"
#include "value_reporter.c"

static void value_reporter_test_before(void *fixture)
{
}

/* -------------------------------------------------------------------------- */
/* 測試案例 (Test Cases)                                                  */
/* -------------------------------------------------------------------------- */
ZTEST_SUITE(value_reporter_suite, NULL, NULL, value_reporter_test_before, NULL, NULL);
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


ZTEST_SUITE(make_report_suite, NULL, NULL, NULL, NULL, NULL);

ZTEST(make_report_suite, test_message_format)
{
    char target[] = "+SYSREG:0,2,4E,32ED,9999\n";
    char msg_buff[MAX_REPORT_LEN] = {0};
    int ret = value_reporter_make_report(msg_buff, sizeof(msg_buff), 2, 0x4E, 0x32ED, 9999);
    printk("result: %s\r\n",msg_buff);
    zassert_equal(ret, 0);
    zassert_str_equal(target, msg_buff);

}

ZTEST(make_report_suite, test_period_out_of_bound)
{
    char msg_buff[MAX_REPORT_LEN] = {0};
    int ret = value_reporter_make_report(msg_buff, sizeof(msg_buff), 2, 0x4E, 0x32ED, 10000);
    zassert_equal(ret, -1);
    ret = value_reporter_make_report(msg_buff, sizeof(msg_buff), 2, 0x4E, 0x32ED, -1);
    zassert_equal(ret, -1);
    ret = value_reporter_make_report(msg_buff, sizeof(msg_buff), 2, 0x4E, 0x32ED, 5000);
    zassert_equal(ret, 0);
}