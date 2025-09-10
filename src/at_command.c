#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/ring_buffer.h>
#include "hmi_uart.h"
#include "cat.h"
#include "value_reporter.h"

LOG_MODULE_REGISTER(at_parser_app, LOG_LEVEL_INF);

// --- Zephyr 相關定義 ---
#define STACK_SIZE 8192
#define THREAD_PRIORITY 7
#define AT_CMD_UART DT_ALIAS(atcmduart)
RING_BUF_DECLARE(uart_at_ringbuf, 1024);
struct hmi_uart_data at_cmd_uart_instance_data = {.dev = DEVICE_DT_GET(AT_CMD_UART), .rx_rbuf = &uart_at_ringbuf};
K_MUTEX_DEFINE(cat_mutex);

// --- 全局變數定義 ---
static char g_manufacture_id[32] = "MyCompany";
static char g_model_id[32] = "AT_Parser_Demo";
static char g_revision[32] = "v1.0.0";
static char g_serial_number[32] = "1234567890";
static char g_hardware_revision[32] = "HW_A1";
static uint8_t g_sysreg_rw = 0;
static uint32_t g_sysreg_reg = 0;
static uint32_t g_sysreg_sensor_id = 0;
static uint32_t g_sysreg_value = 0;
static uint32_t g_sysreg_interval = 0;
static char g_mqtt_client_id[64] = "cat_parser_client";
static uint16_t g_mqtt_keep_alive = 60;
static uint8_t g_mqtt_clean_session = 0;

static char g_working_buffer[256];
static bool g_quit_flag = false;

// --- 命令處理函式宣告 ---
static cat_return_state cmd_help_run(const struct cat_command *cmd);
static cat_return_state cmd_cgmi_run(const struct cat_command *cmd);
static cat_return_state cmd_cgmi_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_cgmm_run(const struct cat_command *cmd);
static cat_return_state cmd_cgmm_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_cgmr_run(const struct cat_command *cmd);
static cat_return_state cmd_cgmr_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_cgsn_run(const struct cat_command *cmd);
static cat_return_state cmd_cgsn_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_cgmh_run(const struct cat_command *cmd);
static cat_return_state cmd_cgmh_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_sysreg_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num);
static cat_return_state cmd_sysreg_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_xmqttcfg_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);
static cat_return_state cmd_xmqttcfg_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num);
static cat_return_state cmd_xmqttcfg_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size);

// --- 命令變數描述符定義 ---
static struct cat_variable g_sysreg_vars[] = {
    { .name = "r/w", .type = CAT_VAR_UINT_DEC, .data = &g_sysreg_rw, .data_size = sizeof(g_sysreg_rw), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "sensor_id", .type = CAT_VAR_UINT_DEC, .data = &g_sysreg_sensor_id, .data_size = sizeof(g_sysreg_sensor_id), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "reg", .type = CAT_VAR_BUF_HEX, .data = &g_sysreg_reg, .data_size = sizeof(g_sysreg_reg), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "value", .type = CAT_VAR_UINT_DEC, .data = &g_sysreg_value, .data_size = sizeof(g_sysreg_value), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "interval", .type = CAT_VAR_UINT_DEC, .data = &g_sysreg_interval, .data_size = sizeof(g_sysreg_interval), .access = CAT_VAR_ACCESS_READ_WRITE },
};

static struct cat_variable g_mqtt_vars[] = {
    { .name = "client_id", .type = CAT_VAR_BUF_STRING, .data = &g_mqtt_client_id, .data_size = sizeof(g_mqtt_client_id), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "keep_alive", .type = CAT_VAR_UINT_DEC, .data = &g_mqtt_keep_alive, .data_size = sizeof(g_mqtt_keep_alive), .access = CAT_VAR_ACCESS_READ_WRITE },
    { .name = "clean_session", .type = CAT_VAR_UINT_DEC, .data = &g_mqtt_clean_session, .data_size = sizeof(g_mqtt_clean_session), .access = CAT_VAR_ACCESS_READ_WRITE },
};

// --- 命令描述符定義 ---
static struct cat_command g_cmds[] = {
    { .name = "+CGMI", .description = "Requests manufacture identification.", .run = cmd_cgmi_run, .test = cmd_cgmi_test },
    { .name = "+CGMM", .description = "Requests model identification.", .run = cmd_cgmm_run, .test = cmd_cgmm_test },
    { .name = "+CGMR", .description = "Requests revision identification.", .run = cmd_cgmr_run, .test = cmd_cgmr_test },
    { .name = "+CGSN", .description = "Requests product serial number identification.", .run = cmd_cgsn_run, .test = cmd_cgsn_test },
    { .name = "+CGMH", .description = "Requests hardware revision.", .run = cmd_cgmh_run, .test = cmd_cgmh_test },
    {
        .name = "+SYSREG",
        .description = "System register R/W operation.",
        .write = cmd_sysreg_write,
        .test = cmd_sysreg_test,
        .var = g_sysreg_vars,
        .var_num = sizeof(g_sysreg_vars) / sizeof(g_sysreg_vars[0]),
    },
    {
        .name = "#XMQTTCFG",
        .description = "MQTT client configuration.",
        .write = cmd_xmqttcfg_write,
        .read = cmd_xmqttcfg_read,
        .test = cmd_xmqttcfg_test,
        .var = g_mqtt_vars,
        .var_num = sizeof(g_mqtt_vars) / sizeof(g_mqtt_vars[0]),
    },
    { .name = "#HELP", .description = "Prints a list of all available commands.", .run = cmd_help_run },
};

// --- Zephyr I/O 介面實現 ---
static int write_char(char ch) {
    LOG_DBG("Sent: %c\n",(ch));
    hmi_uart_send(at_cmd_uart_instance_data.dev, &ch, 1);
    return 1;
}

static int read_char(char *ch) {
    int ret = 0;
    ret = ring_buf_get(&uart_at_ringbuf, ch, 1);
    if(ret != 0)
    {
        LOG_DBG("Get: %c\n",(uint8_t)(*ch));
        //LOG_INF("head:%d,tail:%d,base:%d\n",uart_at_ringbuf.get_head,uart_at_ringbuf.get_tail,uart_at_ringbuf.get_base);
        
        return 1;
    }
    return 0;
}

// --- 互斥鎖介面實現 ---
static int mutex_lock(void) {
    return k_mutex_lock(&cat_mutex, K_FOREVER);
}

static int mutex_unlock(void) {
    return k_mutex_unlock(&cat_mutex);
}

static struct cat_io_interface g_iface = {
    .read = read_char,
    .write = write_char
};

static struct cat_mutex_interface g_mutex = {
    .lock = mutex_lock,
    .unlock = mutex_unlock
};

static cat_return_state cmd_help_run(const struct cat_command *cmd) {
    LOG_INF("Execute Help");
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

static cat_return_state cmd_cgmi_run(const struct cat_command *cmd) {
    printk("%s\n", g_manufacture_id);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_cgmi_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_cgmm_run(const struct cat_command *cmd) {
    printk("%s\n", g_model_id);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_cgmm_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_cgmr_run(const struct cat_command *cmd) {
    printk("%s\n", g_revision);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_cgmr_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_cgsn_run(const struct cat_command *cmd) {
    printk("%s\n", g_serial_number);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_cgsn_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_cgmh_run(const struct cat_command *cmd) {
    printk("%s\n", g_hardware_revision);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_cgmh_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_sysreg_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num) {
    uint32_t result = 0;

    LOG_INF("+SYSREG:%d, %u, %02X,%u,%u\n", g_sysreg_rw, g_sysreg_sensor_id, g_sysreg_reg, g_sysreg_value, g_sysreg_interval);
    result = value_reporter_set_report_period(g_sysreg_sensor_id, g_sysreg_reg, g_sysreg_interval);
    if(result != g_sysreg_interval)
    {
        return CAT_RETURN_STATE_ERROR;
    }

    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_sysreg_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_xmqttcfg_read(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    int written = snprintf((char*)data, max_data_size, "+XMQTTCFG:\"%s\",%u,%u", g_mqtt_client_id, g_mqtt_keep_alive, g_mqtt_clean_session);
    if (written > 0) {
        *data_size = written;
    }
    return CAT_RETURN_STATE_DATA_OK;
}
static cat_return_state cmd_xmqttcfg_write(const struct cat_command *cmd, const uint8_t *data, const size_t data_size, const size_t args_num) {
    LOG_INF("MQTT config updated: client_id=\"%s\", keep_alive=%u, clean_session=%u", g_mqtt_client_id, g_mqtt_keep_alive, g_mqtt_clean_session);
    return CAT_RETURN_STATE_OK;
}
static cat_return_state cmd_xmqttcfg_test(const struct cat_command *cmd, uint8_t *data, size_t *data_size, const size_t max_data_size) {
    return CAT_RETURN_STATE_OK;
}

// --- AT 解析器執行緒 ---
static void at_parser_thread(void *p1, void *p2, void *p3) {
    struct cat_object at;
    
    struct cat_command_group g_cmd_group_obj = {
        .cmd = g_cmds,
        .cmd_num = sizeof(g_cmds) / sizeof(g_cmds[0]),
    };
    
    struct cat_command_group *g_cmd_desc[] = {
        &g_cmd_group_obj
    };

    struct cat_descriptor g_desc = {
        .cmd_group = g_cmd_desc,
        .cmd_group_num = sizeof(g_cmd_desc) / sizeof(g_cmd_desc[0]),
        .buf = (uint8_t *)g_working_buffer,
        .buf_size = sizeof(g_working_buffer),
    };

    if (hmi_uart_init_instance(&at_cmd_uart_instance_data, 115200)) {
        LOG_ERR("UART device not ready!");
        return;
    }
    
    cat_init(&at, &g_desc, &g_iface, &g_mutex);
    LOG_INF("AT Command Parser Thread Started");
    
    // Initial banner
    LOG_INF("AT Command Parser Demo\n");
    LOG_INF("Type AT#HELP to see the command list.\n");

    while (!g_quit_flag) {
        cat_service(&at);
        k_msleep(1);
    }
}

K_THREAD_DEFINE(at_parser_tid, STACK_SIZE, at_parser_thread, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);