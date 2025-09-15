#ifndef VALUE_REPORTER_H__
#define VALUE_REPORTER_H__
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "m90e26_reg.h"

#define TOTAL_SENSOR_ID 3
#define REG_NOT_VALIDE  (-1)
#define PERIOD_DISABLE_REPORT (0x00000000)
#define PERIOD_NOT_VALIDE (0xFFFFFFFF)
#define MAX_REPORT_LEN  (26U)
#define MAX_PERIOD  (9999)


uint32_t value_reporter_set_report_period(uint8_t sensorId, uint8_t reg, uint32_t period);
bool value_reporter_start(void);
#endif // VALUE_REPORTER_H__