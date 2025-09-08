#include <stdint.h>
#include "value_reporter.h"

uint32_t report_periods[TOTAL_SENSOR_ID][TOTAL_REG_NUM];

static int value_reporter_make_report(char buff, uint8_t sensorId, uint8_t reg, uint32_t period);
static int atm90e26_get_register_index(enum ATM90E26_ENG_REGSTERS reg);

static int atm90e26_get_register_index(enum ATM90E26_ENG_REGSTERS reg) {
    switch (reg) {
        case APENG:   return 0;
        case ANENG:   return 1;
        case ATENG:   return 2;
        case RPENG:   return 3;
        case RNENG:   return 4;
        case RTENG:   return 5;
        case ENSTAT:  return 6;
        case IRMS:    return 7;
        case URMS:    return 8;
        case PMEAN:   return 9;
        case QMEAN:   return 10;
        case FREQ:    return 11;
        case POWERF:  return 12;
        case PANG:    return 13;
        case SMEAN:   return 14;
        case IRMS2:   return 15;
        case PMEAN2:  return 16;
        case QMEAN2:  return 17;
        case POWERF2: return 18;
        case PANG2:   return 19;
        case SMEAN2:  return 20;
        default:      return -1; // 代表錯誤或未知的位址
    }
}

uint32_t value_reporter_set_report_period(uint8_t sensorId, uint8_t reg, uint32_t period)
{
    int reg_index = 0;
    reg_index = atm90e26_get_register_index(reg);
    if(sensorId >= TOTAL_SENSOR_ID || sensorId < 0)
    {
        return PERIOD_NOT_VALIDE;
    }

    if(reg_index == -1)
    {
        return PERIOD_NOT_VALIDE;
    }
    report_periods[sensorId][reg_index] = period;

    return report_periods[sensorId][reg_index];
}