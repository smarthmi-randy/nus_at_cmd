#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "value_reporter.h"

LOG_MODULE_REGISTER(value_reporter, LOG_LEVEL_INF);

uint32_t report_periods[TOTAL_SENSOR_ID][TOTAL_REG_NUM];

static int value_reporter_make_report(char *buff, size_t buff_size, uint8_t sensorId, uint8_t reg, uint16_t val, uint32_t period);
static int atm90e26_get_register_index(enum ATM90E26_ENG_REGSTERS reg);
static bool value_reporter_check_reg_need_report(uint8_t sensor_id, uint8_t reg, uint32_t last_tick, uint32_t current_tick);

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
        default:      return REG_NOT_VALIDE; // 代表錯誤或未知的位址
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

    if(reg_index == REG_NOT_VALIDE)
    {
        return PERIOD_NOT_VALIDE;
    }
    report_periods[sensorId][reg_index] = period;

    return report_periods[sensorId][reg_index];
}

static int value_reporter_make_report(char *buff, size_t buff_size, uint8_t sensorId, uint8_t reg, uint16_t val, uint32_t period)
{
    int ret = -1;
    //+SYSREG:<r/w (r:0, w:1)>,<SensorID (0~2)>,<reg (hex)>,<value (hex)>,<interval (s)>
    //+SYSREG:1,2,4E,32ED,9999\n
    if((buff == NULL) || (buff_size < MAX_REPORT_LEN))
    {
        return -1;
    }

    if(period > MAX_PERIOD || period < 0)
    {
        return -1;
    }

    ret = snprintf(buff, MAX_REPORT_LEN
        , "+SYSREG:0,%01u,%02X,%04X,%04u\n"
        ,sensorId, reg, val, period);

    if(ret <= 0)
    {
        return -1;
    }

    return 0;
}

static bool value_reporter_check_reg_need_report(uint8_t sensor_id, uint8_t reg, uint32_t last_tick, uint32_t current_tick)
{
    uint32_t period = 0;
    int reg_idx = REG_NOT_VALIDE;

    reg_idx = atm90e26_get_register_index(reg);

    if(reg_idx == REG_NOT_VALIDE)
    {
        return false;
    }

    if(sensor_id >= TOTAL_SENSOR_ID || sensor_id < 0)
    {
        return false;
    }

    period = report_periods[sensor_id][atm90e26_get_register_index(reg)];

    if(current_tick - last_tick >= period)
    {
        return true;
    }else
    {
        return false;
    }

    return false;
}

static void value_reporter_thread(void *p1, void *p2, void *p3) {
}