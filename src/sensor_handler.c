#include <stdint.h>
#include "sensor_handler.h"

int sensor_handler_update_value(void);

uint16_t sensor_handler_get_reg_value(uint8_t sensorId, uint8_t reg)
{
    static uint16_t reg_val = 0x1010;
    reg_val++;
    return reg_val;
}