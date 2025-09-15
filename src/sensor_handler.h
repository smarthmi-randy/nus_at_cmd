#ifndef SENSOR_HANDLER_H_
#define SENSOR_HANDLER_H_

#include <stdint.h>

/**
 * @file
 * @brief Sensor Handler 模組的公開 API。
 *
 * 此模組負責在背景中定期從多個 M90E26 感測器讀取數據，
 * 並提供一個執行緒安全 (thread-safe) 的 API 來存取最新的快取感測器數值。
 */

/**
 * @brief 由此處理程序管理的 M90E26 感測器實例數量。
 *
 * 應用層可以使用此常數來遍歷所有可用的感測器。
 */
#define SENSOR_HANDLER_NUM_INSTANCES 3

/**
 * @brief 初始化 sensor_handler 模組。
 *
 * 此函式必須在啟動時呼叫一次。它會初始化所有感測器設備實例、
 * 設定內部 RTIO 上下文，並啟動週期性的背景輪詢計時器。
 *
 * @retval 0 成功。
 * @retval -ENODEV 如果一個或多個 M90E26 感測器設備未就緒。
 */
int sensor_handler_init(void);

/**
 * @brief 從指定感測器的快取中獲取特定暫存器的值。
 *
 * 此函式擷取由背景輪詢任務取得的最新數值。它從內部快取讀取，
 * 本身不執行任何硬體 I/O 操作。此函式是非阻塞的，並且是執行緒安全的。
 *
 * @param sensorId 要查詢的感測器實例索引 (範圍從 0 到 SENSOR_HANDLER_NUM_INSTANCES - 1)。
 * @param reg 要讀取的 8 位元暫存器位址。該暫存器必須是背景任務
 * 正在輪詢的暫存器之一 (例如，來自 m90e26_reg.h 的 URMS, IRMS)。
 * @param val 一個指向 uint16_t 變數的指標，用於儲存暫存器的值。
 *
 * @retval 0 成功。
 * @retval -EINVAL 如果提供的 sensorId 超出範圍。
 * @retval -ENOTSUP 如果請求的暫存器 `reg` 不在背景輪詢列表中，因此在快取中不可用。
 */
int sensor_handler_get_m90e26_reg_value(uint8_t sensorId, uint8_t reg, uint16_t *val);

#endif /* SENSOR_HANDLER_H_ */