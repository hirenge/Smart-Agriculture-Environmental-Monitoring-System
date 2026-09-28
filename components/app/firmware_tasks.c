#include "firmware_tasks.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "board.h"
#include "relay.h"

#define TELEMETRY_QUEUE_LENGTH  5
#define COMMAND_QUEUE_LENGTH    5
#define LOG_QUEUE_LENGTH        20

static const char *TAG = "FIRMWARE";

static QueueHandle_t s_telemetry_queue;
static QueueHandle_t s_command_queue;
static QueueHandle_t s_log_queue;

static void sensor_task(void *arg)
{
    (void)arg;

    while (1) {
        sensor_snapshot_t snapshot = {0};
        esp_err_t ret = sensor_manager_sample(&snapshot);
        if (ret == ESP_OK) {
            telemetry_message_t msg = {0};
            msg.telemetry = snapshot;
            msg.seq = xTaskGetTickCount();
            xQueueSend(s_telemetry_queue, &msg, 0);

            log_message_t log = {0};
            log.level = LOG_LEVEL_INFO;
            snprintf(log.message, sizeof(log.message),
                     "sample ok seq=%lu", (unsigned long)msg.seq);
            log.tick_count = xTaskGetTickCount();
            xQueueSend(s_log_queue, &log, 0);
        } else {
            log_message_t log = {0};
            log.level = LOG_LEVEL_ERROR;
            snprintf(log.message, sizeof(log.message),
                     "sensor sample failed: %s", esp_err_to_name(ret));
            log.tick_count = xTaskGetTickCount();
            xQueueSend(s_log_queue, &log, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void mqtt_task(void *arg)
{
    (void)arg;

    log_message_t log = {0};
    log.level = LOG_LEVEL_INFO;
    snprintf(log.message, sizeof(log.message), "mqtt task started");
    log.tick_count = xTaskGetTickCount();
    xQueueSend(s_log_queue, &log, 0);

    while (1) {
        telemetry_message_t telemetry = {0};
        if (xQueueReceive(s_telemetry_queue, &telemetry, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Telemetry queued seq=%lu", (unsigned long)telemetry.seq);
        }

        command_message_t command = {0};
        if (xQueueReceive(s_command_queue, &command, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Command received: %s enabled=%d", command.command, command.enabled);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void control_task(void *arg)
{
    (void)arg;

    while (1) {
        command_message_t command = {0};
        if (xQueueReceive(s_command_queue, &command, pdMS_TO_TICKS(500)) == pdTRUE) {
            relay_set(command.enabled);

            log_message_t log = {0};
            log.level = LOG_LEVEL_INFO;
            snprintf(log.message, sizeof(log.message),
                     "relay set %s", command.enabled ? "ON" : "OFF");
            log.tick_count = xTaskGetTickCount();
            xQueueSend(s_log_queue, &log, 0);
        }
    }
}

static void log_task(void *arg)
{
    (void)arg;

    while (1) {
        log_message_t log = {0};
        if (xQueueReceive(s_log_queue, &log, portMAX_DELAY) == pdTRUE) {
            switch (log.level) {
                case LOG_LEVEL_INFO:
                    ESP_LOGI(TAG, "%s", log.message);
                    break;
                case LOG_LEVEL_WARN:
                    ESP_LOGW(TAG, "%s", log.message);
                    break;
                case LOG_LEVEL_ERROR:
                    ESP_LOGE(TAG, "%s", log.message);
                    break;
                default:
                    ESP_LOGI(TAG, "%s", log.message);
                    break;
            }
        }
    }
}

void firmware_task_init(void)
{
    s_telemetry_queue = xQueueCreate(TELEMETRY_QUEUE_LENGTH, sizeof(telemetry_message_t));
    s_command_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(command_message_t));
    s_log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_message_t));

    if (s_telemetry_queue == NULL || s_command_queue == NULL || s_log_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 4, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(control_task, "control_task", 4096, NULL, 4, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(log_task, "log_task", 2048, NULL, 3, NULL, tskNO_AFFINITY);
}
