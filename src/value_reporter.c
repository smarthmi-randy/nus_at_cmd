#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "value_reporter.h"
#include "sensor_handler.h"

LOG_MODULE_REGISTER(value_reporter, LOG_LEVEL_INF);

static uint32_t report_last_tick[TOTAL_SENSOR_ID][TOTAL_REG_NUM];
static uint32_t report_periods[TOTAL_SENSOR_ID][TOTAL_REG_NUM];

static int value_reporter_make_report(char *buff, size_t buff_size, uint8_t sensorId, uint8_t reg, uint16_t val, uint32_t period);
static int atm90e26_get_register_index(enum ATM90E26_ENG_REGSTERS reg);
static bool value_reporter_check_reg_need_report(uint8_t sensor_id, uint8_t reg, uint32_t last_tick, uint32_t current_tick);
static void value_reporter_timer_handler(void *p1, void *p2, void *p3);

static const enum ATM90E26_ENG_REGSTERS register_map[] = {
    APENG,   ANENG,   ATENG,
    RPENG,   RNENG,   RTENG,
    ENSTAT,  IRMS,    URMS,
    PMEAN,   QMEAN,   FREQ,
    POWERF,  PANG,    SMEAN,
    IRMS2,   PMEAN2,  QMEAN2,
    POWERF2, PANG2,   SMEAN2
};


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
    LOG_DBG("Set period: %u, %02X, %u, %u", sensorId, reg, reg_index, period);
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
    if(period == 0)
    {
        return false;
    }

    if(current_tick - last_tick >= period)
    {
        return true;
    }else
    {
        return false;
    }

    return false;
}

static void value_reporter_timer_handler(void *p1, void *p2, void *p3) {

    static uint32_t current_tick = 0;
    char report_buff[MAX_REPORT_LEN] = {0};
    for(int id = 0; id < TOTAL_SENSOR_ID; id++)
    {
        for(int reg_idx = 0; reg_idx < TOTAL_REG_NUM; reg_idx++)
        {
            bool need_report = false;
            need_report = value_reporter_check_reg_need_report(id, register_map[reg_idx], report_last_tick[id][reg_idx], current_tick);

            if(need_report)
            {
                uint16_t reg_val = 0;
                int ret = sensor_handler_get_m90e26_reg_value(id, reg_idx, &reg_val);
                if(ret != 0)
                {
                    LOG_ERR("Get sensor value error!");
                }
                uint32_t period = report_periods[id][reg_idx];
                value_reporter_make_report(report_buff, MAX_REPORT_LEN, id, register_map[reg_idx], reg_val, period);
                report_last_tick[id][reg_idx] = current_tick;
                LOG_INF("Report: %s", report_buff);
            }
        }
    }
    current_tick++;
}
K_TIMER_DEFINE(value_reporter_timer, value_reporter_timer_handler, NULL);

bool value_reporter_start(void)
{
    k_timer_start(&value_reporter_timer, K_SECONDS(1), K_SECONDS(1));
    return true;
}