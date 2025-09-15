/**
 * @file
 * @brief Atmel M90E26 Energy Metering IC RTIO UART Driver
 *
 * This driver provides an asynchronous, RTIO-based interface to the Atmel M90E26
 * energy metering chip over a UART bus.
 */

// 定義驅動程式的 compatible 字串，用於 DeviceTree 匹配
#define DT_DRV_COMPAT atmel_m90e26_uart

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

// 註冊一個日誌模組，方便除錯
LOG_MODULE_REGISTER(M90E26_UART, CONFIG_SENSOR_LOG_LEVEL);

[cite_start]// M90E26 UART 協議相關定義 [cite: 439, 440, 489, 508]
#define M90E26_UART_START_BYTE 0xFE
#define M90E26_UART_READ_BIT   0x80

// 最大可能的回應長度 (讀取時為3位元組)
#define M90E26_MAX_RESP_LEN 3

// 函式原型宣告
static int m90e26_uart_rtio_io(const struct device *dev, struct rtio_sqe *sqe);

/**
 * @brief 驅動程式的固定設定結構
 * @param uart 指向所使用的 UART 硬體設備
 */
struct m90e26_uart_rtio_config {
	const struct device *uart;
};

/**
 * @brief 驅動程式的每個實例的資料結構
 */
struct m90e26_uart_rtio_data {
	struct rtio_c c; // RTIO 上下文
	struct k_work_q work_q; // 私有的工作佇列，用於處理來自中斷的回應
	struct k_work work; // 用於處理 UART 回應的工作項目

	// 用於 UART 回呼的接收緩衝區
	uint8_t rx_buf[M90E26_MAX_RESP_LEN];
	uint8_t rx_idx; // 目前接收到的位元組計數
	uint8_t expected_rx_len; // 本次操作預期的回應長度

	// 儲存當前正在處理的 RTIO 請求
	struct rtio_sqe *current_sqe;
	struct k_work_delayable timeout_work; // 用於處理操作超時
};

// 實作 RTIO 驅動程式 API
static const struct rtio_driver_api m90e26_uart_rtio_api = {
	.io = m90e26_uart_rtio_io,
};

/**
 * @brief UART 中斷回呼函式
 *
 * 當 UART 硬體接收到資料時，由中斷服務常式 (ISR) 呼叫此函式。
 */
static void m90e26_uart_cb(const struct device *uart_dev, void *user_data)
{
	const struct device *dev = user_data;
	struct m90e26_uart_rtio_data *data = dev->data;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (uart_irq_rx_ready(uart_dev)) {
		// 只要緩衝區還有空間，就持續讀取 FIFO 中的資料
		while (data->rx_idx < data->expected_rx_len) {
			int bytes_read = uart_fifo_read(uart_dev, &data->rx_buf[data->rx_idx],
							data->expected_rx_len - data->rx_idx);
			if (bytes_read <= 0) {
				break; // FIFO 已空
			}
			data->rx_idx += bytes_read;
		}

		// 如果已接收到完整的封包，則停用接收中斷並提交工作項目
		if (data->rx_idx >= data->expected_rx_len) {
			uart_rx_disable(uart_dev);
			k_work_submit_to_queue(&data->work_q, &data->work);
		}
	}
}

/**
 * @brief 處理已接收封包的工作函式
 *
 * 此函式在工作佇列的執行緒上下文中執行，避免在 ISR 中進行耗時操作。
 */
static void m90e26_process_work(struct k_work *item)
{
	struct m90e26_uart_rtio_data *data =
		CONTAINER_OF(item, struct m90e26_uart_rtio_data, work);
	struct rtio_sqe *sqe = data->current_sqe;

	if (!sqe) {
		return;
	}

	// 取消超時處理
	k_work_cancel_delayable(&data->timeout_work);

	if (sqe->op == RTIO_OP_READ) {
		[cite_start]// 驗證讀取操作的回應 [cite: 512]
		uint8_t checksum = data->rx_buf[0] + data->rx_buf[1];
		if (checksum != data->rx_buf[2]) {
			LOG_ERR("Read checksum error. Expected %02x, got %02x", checksum, data->rx_buf[2]);
			rtio_cqe_err(sqe, -EIO);
		} else {
			uint16_t val = sys_get_be16(data->rx_buf);
			memcpy(sqe->buf, &val, sizeof(val));
			rtio_cqe_ok(sqe, 0); // 標記請求成功完成
		}
	} else if (sqe->op == RTIO_OP_WRITE) {
		[cite_start]// 驗證寫入操作的回應 [cite: 510]
		uint16_t written_val;
		memcpy(&written_val, sqe->buf, sizeof(written_val));

		uint8_t sent_checksum = (uint8_t)(uintptr_t)sqe->iodev_data +
					(uint8_t)(written_val >> 8) +
					(uint8_t)(written_val & 0xFF);

		if (data->rx_buf[0] != sent_checksum) {
			LOG_ERR("Write checksum error. Expected %02x, got %02x", sent_checksum, data->rx_buf[0]);
			rtio_cqe_err(sqe, -EIO);
		} else {
			rtio_cqe_ok(sqe, 0); // 標記請求成功完成
		}
	}

	data->current_sqe = NULL; // 清除當前請求，準備接收下一個
}

/**
 * @brief 操作超時處理函式
 */
static void m90e26_timeout_work(struct k_work *item)
{
	struct m90e26_uart_rtio_data *data =
		CONTAINER_OF(item, struct m90e26_uart_rtio_data, timeout_work);
	const struct device *dev = CONTAINER_OF(data, struct device, data);
	const struct m90e26_uart_rtio_config *config = dev->config;

	if (data->current_sqe) {
		uart_rx_disable(config->uart);
		LOG_ERR("Operation timed out");
		rtio_cqe_err(data->current_sqe, -ETIMEDOUT);
		data->current_sqe = NULL;
	}
}

/**
 * @brief RTIO 提交入口函式
 *
 * 當應用程式呼叫 rtio_submit() 時，RTIO 核心會呼叫此函式。
 */
static int m90e26_uart_rtio_io(const struct device *dev, struct rtio_sqe *sqe)
{
	const struct m90e26_uart_rtio_config *config = dev->config;
	struct m90e26_uart_rtio_data *data = dev->data;

	// 這個簡易驅動程式一次只處理一個請求
	if (data->current_sqe != NULL) {
		return -EBUSY;
	}

	uint8_t reg_addr = (uint8_t)(uintptr_t)sqe->iodev_data;
	data->current_sqe = sqe;
	data->rx_idx = 0;

	if (sqe->op == RTIO_OP_READ) {
		uint8_t cmd[3];
		cmd[0] = M90E26_UART_START_BYTE;
		cmd[1] = M90E26_UART_READ_BIT | reg_addr;
		cmd[2] = cmd[1]; [cite_start]// 讀取時的校驗和就是位址本身 [cite: 511]

		data->expected_rx_len = 3;

		uart_rx_enable(config->uart, data->rx_buf, data->expected_rx_len, 100);
		uart_tx(config->uart, cmd, sizeof(cmd), SYS_FOREVER_US);

	} else if (sqe->op == RTIO_OP_WRITE) {
		if (sqe->buf_len != 2) {
			LOG_ERR("Write operations require a 2-byte buffer");
			return -EINVAL; // 寫入操作必須提供 16-bit (2-byte) 的資料
		}
		uint8_t cmd[5];
		uint16_t val_to_write;
		memcpy(&val_to_write, sqe->buf, sizeof(val_to_write));

		cmd[0] = M90E26_UART_START_BYTE;
		cmd[1] = reg_addr; // 寫入時 R/W bit 為 0
		sys_put_be16(val_to_write, &cmd[2]);
		cmd[4] = cmd[1] + cmd[2] + cmd[3]; [cite_start]// 計算寫入時的校驗和 [cite: 509]

		data->expected_rx_len = 1;

		uart_rx_enable(config->uart, data->rx_buf, data->expected_rx_len, 100);
		uart_tx(config->uart, cmd, sizeof(cmd), SYS_FOREVER_US);

	} else {
		return -ENOTSUP; // 不支援其他操作類型
	}

	// 啟動超時偵測
	k_work_schedule_for_queue(&data->work_q, &data->timeout_work, K_MSEC(50));

	return 0;
}

/**
 * @brief 驅動程式初始化函式
 */
static int m90e26_uart_rtio_init(const struct device *dev)
{
	const struct m90e26_uart_rtio_config *config = dev->config;
	struct m90e26_uart_rtio_data *data = dev->data;
	char work_q_name[20];

	if (!device_is_ready(config->uart)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	// 初始化工作佇列
	snprintk(work_q_name, sizeof(work_q_name), "m90e26_wq_%p", dev);
	k_work_queue_start(&data->work_q, NULL, 0, K_PRIO_COOP(7), work_q_name);

	// 初始化工作項目
	k_work_init(&data->work, m90e26_process_work);
	k_work_init_delayable(&data->timeout_work, m90e26_timeout_work);

	// 設定 UART 回呼函式
	uart_callback_set(config->uart, m90e26_uart_cb, (void *)dev);

	// 初始化 RTIO 上下文
	rtio_c_init(&data->c);

	LOG_INF("Device %s initialized", dev->name);
	return 0;
}

// 使用 Zephyr 的巨集，為 DeviceTree 中每個 compatible = "atmel,m90e26-uart" 的節點
// 建立一個驅動程式實例。
#define M90E26_UART_RTIO_DEFINE(inst)                                           \
	static struct m90e26_uart_rtio_data m90e26_data_##inst = {};                  \
                                                                                \
	static const struct m90e26_uart_rtio_config m90e26_config_##inst = {         \
		.uart = DEVICE_DT_GET(DT_INST_PHANDLE(inst, uart)),                      \
	};                                                                          \
                                                                                \
	DEVICE_DT_INST_DEFINE(inst, &m90e26_uart_rtio_init, NULL,                    \
			      &m90e26_data_##inst, &m90e26_config_##inst,           \
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,             \
			      &m90e26_uart_rtio_api);

DT_INST_FOREACH_STATUS_OKAY(M90E26_UART_RTIO_DEFINE)