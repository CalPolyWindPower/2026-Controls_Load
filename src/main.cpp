// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

/* Includes */
#include "2026Core/Net/Net-Link/AdapterUHCI.hpp"
// #include "2026Core/Net/NetAdapter_A.hpp"
#include "esp_log.h"
#include <Arduino.h>
#include "MCP23008T.hpp"

/* Config */
static constexpr char *TAG = "LoMa";
static constexpr uint8_t LOAD_ADDR = 0x00; // I2C address

/* Global Objects */
MCP23008T loadDevice(LOAD_ADDR, &Wire);
bool loadConfigured = false;

/* Function Prototypes */
bool configureLoad() {
    if (!loadDevice.begin()) {
        ESP_LOGE(TAG, "Failed to initialize MCP23008T device at 0x%02X",
                 LOAD_ADDR);
        return false;
    }

    // First six pins outputs, made last two inputs as that's the default
    if (!loadDevice.setIODir(0b1100'0000)) {
        ESP_LOGE(TAG, "Failed to set IODIR on MCP23008T at 0x%02X", LOAD_ADDR);
        return false;
    }

    // Disable sequential operation, other settings default
    if (!loadDevice.setConfig(0b0010'0000)) {
        ESP_LOGW(TAG, "Failed to set IOCON on MCP23008T at 0x%02X", LOAD_ADDR);
    }

    // Set all outputs low, note that pin 0 is inverted
    if (!loadDevice.setGPIO(0b0000'0000)) {
        ESP_LOGE(TAG, "Failed to set GPIO on MCP23008T at 0x%02X", LOAD_ADDR);
        return false;
    }

    // Else: success
    return true;
}

/**
 * MARK: Setup
 * put your setup code here, to run once:
 */
void setup() {
    pinMode(LED::LED_PIN, OUTPUT);

    // Configure Devices
    loadConfigured = configureLoad();

    // Set up tasks
    xTaskCreate(vTaskStatusLED, // Task function
                "Status LED",   // Name of the task (for debugging)
                1024,           // Stack size (in words, not bytes)
                nullptr,        // Task input parameter
                1,              // Priority of the task
                nullptr         // Task handle
    );
    xTaskCreate(vTaskConfigure, "Cfg", 1024, nullptr, 50, nullptr);
}

/**
 * MARK: Tasks
 * @see
 * https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/05-Implementing-a-task
 * @see https://forum.arduino.cc/t/non-blocking-delay-actions/1044079
 */

void vTaskStatusLED(void *pvParameters) {
    while (true) {
        digitalWrite(LED::LED_PIN, HIGH);
        delay(LED::BLINK_ON_MILLIS);
        digitalWrite(LED::LED_PIN, LOW);
        delay(LED::BLINK_OFF_MILLIS);
    }
}

void vTaskConfigure(void *pvParameters) {
    while (true) {
        if (!loadConfigured) {
            if (loadDevice.begin()) {
                ESP_LOGI(TAG, "MCP23008T initialized successfully.");
                loadConfigured = true;
            } else {
                ESP_LOGE(
                    TAG,
                    "Failed to initialize MCP23008T. Retrying in 5 seconds...");
                delay(5000);
            }
        } else {
            // Sleep for 30 seconds
            delay(30000);
        }
    }
}

/**
 * @brief Task to handle Telnet connections
 */
void vTaskTelnet(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to handle ElegantOTA connections
 * @deprecated Just use a USB cable if possible
 */
void vTaskOTA(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to poll high priority sensors
 */
void vTaskPollSensors(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to control run the FSM
 */
void vTaskUpdateFSM(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to control run the variable load
 */
void vTaskVarLoad(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to handle inbound data that has been queued
 */
void vTaskHandleInboundData(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to handle outbound data that has been queued
 */
void vTaskHandleOutboundData(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to log data
 */
void vTaskLogData(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to track idle time
 */
void vTaskIdle(void *pvParameters) {
    while (true) {
    }
}

/**
 * MARK: loop
 * Arduino: put your main code here, to run repeatedly:
 */
void loop() {}
