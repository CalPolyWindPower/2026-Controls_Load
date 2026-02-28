// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

/* Includes */
// #include "2026Core/Net/Net-Application/Telnet.hpp"
// #include "2026Core/Net/Net-Link/AdapterUHCI.hpp"
// #include "2026Core/Net/NetAdapter_A.hpp"
#include "2026Core/CommonConfig.hpp"
#include "2026Core/Net/Net-Application/NTP.hpp"
#include "2026Core/Net/Net-Link/AdapterESPNow.hpp"
#include "2026Core/Net/Net-Phy/AdapterWLAN.hpp"
#include "LoadConfig.hpp"
#include "MCP23008T.hpp"
// #include <2026C0re/Net/Net-Application/OTA.hpp>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <esp_log.h>
#include <temperature_sensor.h>

/* Config */
static constexpr char *TAG = "LoMa";

// MARK: Function Prototypes
// Main Tasks
void vTaskUpdateFSM(void *pvParameters);
void vTaskPollSensors(void *pvParameters);
void vTaskAdjustLoad(void *pvParameters);
void vTaskRecvData(void *pvParameters);

void vTaskSendData(void *pvParameters);
void vTaskConfigure(void *pvParameters);
void vTaskStatusLED(void *pvParameters);
void vTaskLogData(void *pvParameters);

// Optional Tasks
void vTaskTelnet(void *pvParameters);
void vTaskOTA(void *pvParameters);

// Helper Functions
bool configureLoad();

// MARK:  Global Objects
struct TaskInfo {
    const TaskFunction_t function;
    const char *const name;
    const configSTACK_DEPTH_TYPE stackSize_bytes = 1024;
    void *const pvParameters = nullptr;
    const UBaseType_t priority; // Note: Task priority must be <25 for some
                                // reason, possible bug
    TaskHandle_t pxCreatedTask = nullptr;
    UBaseType_t minFreeStack_Bytes = 0;
};
constexpr uint_fast8_t NUM_USR_TASKS = 8; // Must match number of entires!
// Arduino Loop has priority 1
// TODO: Note: Task priority must be <25
etl::array<TaskInfo, NUM_USR_TASKS> taskDescriptions = {
    TaskInfo{vTaskUpdateFSM, "FSM", 256, nullptr, 25, nullptr, 0},
    TaskInfo{vTaskPollSensors, "Poll", 2048, nullptr, 20, nullptr, 0},
    TaskInfo{vTaskAdjustLoad, "ALD", 4096, nullptr, 20, nullptr, 0},
    TaskInfo{vTaskRecvData, "Recv", 2048, nullptr, 15, nullptr, 0},

    TaskInfo{vTaskSendData, "Send", 2048, nullptr, 15, nullptr, 0},
    TaskInfo{vTaskConfigure, "Cfg", 512, nullptr, 10, nullptr, 0},
    TaskInfo{vTaskStatusLED, "LED", 256, nullptr, 2, nullptr, 0},
    TaskInfo{vTaskLogData, "Log", 4096, nullptr, 1, nullptr, 0}};
enum TASK_IDS : uint_fast8_t {
    TID_FSM = 0,
    TID_POLL,
    TID_ADJLD,
    TID_RECV,
    TID_SEND,
    TID_CFG,
    TID_LED,
    TID_LOG
};

AdapterWLAN adapterWLAN = AdapterWLAN();
AdapterESPNow adapterESPNow = AdapterESPNow();
SyncedClock netClock = SyncedClock(adapterESPNow); // todo
Adafruit_NeoPixel leds(1, UM_PROS3::LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
MCP23008T loadDevice(LOAD::I2C_ADDRESS, &Wire);
bool loadConfigured = false;

// todo: move
bool configureLoad() {
    if (!loadDevice.begin()) {
        ESP_LOGE(TAG, "Failed to initialize MCP23008T device at 0x%02X",
                 loadDevice.getAddress());
        return false;
    }

    // First six pins outputs, made last two inputs as that's the default
    if (!loadDevice.setIODir(0b1100'0000)) {
        ESP_LOGE(TAG, "Failed to set IODIR on MCP23008T at 0x%02X",
                 loadDevice.getAddress());
        return false;
    }

    // Disable sequential operation, other settings default
    if (!loadDevice.setConfig(0b0010'0000)) {
        ESP_LOGW(TAG, "Failed to set IOCON on MCP23008T at 0x%02X",
                 loadDevice.getAddress());
    }

    // Set all outputs low, note that pin 0 is inverted
    if (!loadDevice.setGPIO(0b0000'0000)) {
        ESP_LOGE(TAG, "Failed to set GPIO on MCP23008T at 0x%02X",
                 loadDevice.getAddress());
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
    // Configure Devices
    if (!leds.begin()) {
        ESP_LOGE(TAG, "Failed to initialize LED");
        leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // Orange
    }
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
        leds.setPixelColor(0, 0x00, 0xFF, 0x00); // Green
        delay(LED::BLINK_ON_MILLIS);
        leds.clear();
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
