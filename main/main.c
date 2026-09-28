#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "firmware_tasks.h"
#include "sensor_manager.h"
#include "relay.h"

static const char *TAG = "APP";

void app_main(void)
{
    ESP_LOGI(TAG, "Booting Smart Agriculture firmware");

    esp_err_t ret = board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "board_init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = sensor_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sensor_manager_init failed: %s", esp_err_to_name(ret));
        return;
    }

    relay_set(false);
    firmware_task_init();

    log_message_t boot_log = {0};
    boot_log.level = LOG_LEVEL_INFO;
    snprintf(boot_log.message, sizeof(boot_log.message), "firmware started");
    boot_log.tick_count = xTaskGetTickCount();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}