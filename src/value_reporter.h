#ifndef VALUE_REPORTER_H__
#define VALUE_REPORTER_H__
#include <stdint.h>

#define TOTAL_SENSOR_ID 3
#define TOTAL_REG_NUM   21
#define PERIOD_DISABLE_REPORT (0x00000000)
#define PERIOD_NOT_VALIDE (0xFFFFFFFF)

enum ATM90E26_ENG_REGSTERS {
    APENG = 0x40,
    ANENG = 0x41,
    ATENG = 0x42,
    RPENG = 0x43,
    RNENG = 0x44,
    RTENG = 0x45,
    ENSTAT = 0x46,
    IRMS = 0x48,
    URMS = 0x49,
    PMEAN = 0x4A,
    QMEAN = 0x4B,
    FREQ = 0x4C,
    POWERF = 0x4D,
    PANG = 0x4E,
    SMEAN = 0x4F,
    IRMS2 = 0x68,
    PMEAN2 = 0x6A,
    QMEAN2 = 0x6B,
    POWERF2 = 0x6D,
    PANG2 = 0x6E,
    SMEAN2 = 0x6F
};

uint32_t value_reporter_set_report_period(uint8_t sensorId, uint8_t reg, uint32_t period);

#endif // VALUE_REPORTER_H__