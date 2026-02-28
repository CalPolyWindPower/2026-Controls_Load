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
    static bool serialInitialized = false;
    if (!serialInitialized) {
        Serial.begin(115200);
        ESP_LOGI(TAG, "Serial initialized");
        serialInitialized = true;
    }

    // Configure Hardware
    static bool LEDInitialized = false;
    if (!LEDInitialized) {
        if (!leds.begin()) {
            ESP_LOGE(TAG, "Failed to initialize LED");
            leds.setPixelColor(0, 0xFF, 0x00, 0x00); // Red
        } else {
            LEDInitialized = true;
            leds.setPixelColor(0, 0x00, 0xFF, 0x00); // Green
            ESP_LOGI(TAG, "LED initialized");
        }
    } else {
        // No need to save power here
        leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    }
    if (leds.canShow()) {
        leds.show();
    }

    // Configure WiFi
    static bool wifiInitialized = false;
    if (!wifiInitialized) {
        leds.setPixelColor(0, 0x00, 0x00, 0xFF); // blue
        if (leds.canShow()) {
            leds.show();
        }
        uint8_t optimalChannel = adapterWLAN.identifyOptimalChannel();
        leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
        if (leds.canShow()) {
            leds.show();
        }
        ESP_LOGI(TAG, "Optimal WiFi Channel: %d", optimalChannel);
        if (adapterWLAN.begin(optimalChannel)) {
            ESP_LOGI(TAG, "WiFi initialized");
            wifiInitialized = true;
        } else {
            ESP_LOGE(TAG, "Failed to initialize WiFi");
        }
    }
    leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    if (leds.canShow()) {
        leds.show();
    }

    // Configure ESP-NOW
    static bool espNowInitalized = false;
    if (!espNowInitalized) {
        if (adapterESPNow.begin()) {
            ESP_LOGI(TAG, "ESP-NOW initialized.");
            espNowInitalized = true;
        } else {
            ESP_LOGE(TAG, "Failed to initialize ESP-NOW");
        }
    }
    leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    if (leds.canShow()) {
        leds.show();
    }

    // Configure ESP-NOW Peers
    static bool peerRegistered = false;
    if (!peerRegistered) {
        if (adapterESPNow.registerPeer(WTbNetConfig::NACELLE_MAC)) {
            ESP_LOGI(TAG, "Registered peer");
            peerRegistered = true;
        } else {
            ESP_LOGE(TAG, "Failed to register peer");
        }
    }
    leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    if (leds.canShow()) {
        leds.show();
    }

    // Sync Time // FIXME! - Load accesses fault
    static bool timeSynced = false;
    if (!timeSynced) {
        if (netClock.initTimeSync(WTbNetConfig::LOAD_MAC)) {
            ESP_LOGI(TAG, "Time sync initialized successfully");
            timeSynced = true;
        } else {
            ESP_LOGE(TAG, "Failed to initialize time sync");
        }
    }
    leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    if (leds.canShow()) {
        leds.show();
    }

    // Print MAC Address // todo - verify
    ESP_LOGI(
        TAG, "MAC Address: %s",
        AdapterWLAN::formatMACAddress(adapterWLAN.getMACAddress()).c_str());
    leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    if (leds.canShow()) {
        leds.show();
    }

    // TODO: Check ESP-NOW impl against last years
    // TODO: Configure response handler, load server

    loadConfigured = configureLoad();
    leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    if (leds.canShow()) {
        leds.show();
    }

    // Set up tasks
    static bool tasksSetup = false;
    if (!tasksSetup) {
        for (TaskInfo &taskDesc : taskDescriptions) {
            if (taskDesc.stackSize_bytes % sizeof(uint_fast8_t) != 0) {
                ESP_LOGW(TAG, "Stack size not word aligned");
            }
            // Syntax: xTaskCreate(Task function, Name of the task (for
            // debugging), Stack size (in words, not bytes), Task input
            // parameter, Priority of the task, Task handle)
            BaseType_t result =
                xTaskCreate(taskDesc.function, taskDesc.name,
                            taskDesc.stackSize_bytes, taskDesc.pvParameters,
                            taskDesc.priority, &(taskDesc.pxCreatedTask));
            if (result != pdPASS) {
                ESP_LOGE(TAG, "Failed to create task %s", taskDesc.name);
            } else {
                ESP_LOGV(
                    TAG,
                    "Created task %s with priority %u and stack size %u bytes",
                    taskDesc.name, taskDesc.priority, taskDesc.stackSize_bytes);
            }
        }
        tasksSetup = true;

        // pitchPIDController.enable(
        //     0.0f, 1500.0f); // todo - just a quick performances test

        ESP_LOGI(TAG, "Setup complete!");
    }
}

/**
 * MARK: Tasks
 * @see
 * https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/05-Implementing-a-task
 * @see https://forum.arduino.cc/t/non-blocking-delay-actions/1044079
 */

/**
 * @brief Task to control run the FSM
 */
void vTaskUpdateFSM(void *pvParameters) {
    while (true) {
    }
}

/**
 * @brief Task to poll high priority sensors
 */
void vTaskPollSensors(void *pvParameters) {
    while (true) {
        delay(RUN::TASK_INTERVALS::TI_POLL_SENSORS_mS);
    }
}

/**
 * @brief Task to control the pitch actuator
 */
void vTaskAdjustLoad(void *pvParameters) {
    while (true) {
        static int i = 0;
        // ESP_LOGI(TAG, "Pitch PID Output: %f",
        //          pitchPIDController.compute(
        //              i)); // todo - just a quick performances test
        i += 20;
        delay(RUN::TASK_INTERVALS::TI_ADJUST_LOAD_mS);
    }
}

/**
 * @brief Task to control run the variable load
 */
// void vTaskVarLoad(void *pvParameters) {
//     while (true) {
//     }
// }

// MARK: Network Tasks

/**
 * @brief Task to handle inbound data that has been queued
 */
void vTaskRecvData(void *pvParameters) {
    while (true) {
        if (false) {
            delay(RUN::TASK_INTERVALS::TI_RECV_ms);
        } else {
            // Suspend until reenabled from interrupt
            vTaskSuspend(taskDescriptions[TASK_IDS::TID_RECV].pxCreatedTask);
        }
    }
}

/**
 * @brief Task to handle inbound data that has been queued
 */
// void vTaskHandleInboundData(void *pvParameters) {
//     while (true) {
//     }
// }

/**
 * @brief Task to handle outbound data that has been queued
 */
// void vTaskHandleOutboundData(void *pvParameters) {
//     while (true) {
//     }
// }

/**
 * @brief Task to handle outbound data that has been queued
 */
void vTaskSendData(void *pvParameters) {
    while (true) {
        if (false) {
            delay(RUN::TASK_INTERVALS::TI_SEND_ms);
        } else {
            // Suspend until reenabled
            vTaskSuspend(taskDescriptions[TASK_IDS::TID_SEND].pxCreatedTask);
        }
    }
}

// MARK: Utility Tasks

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
            delay(RUN::TASK_INTERVALS::TI_CFG_ms);
        }
    }
}

/**
 * @brief Task to handle Telnet connections
 */
void vTaskTelnet(void *pvParameters) {
    while (true) {
        // TELNET::loop(); // todo
        delay(RUN::TASK_INTERVALS::TI_TELNET_ms);
    }
}

/**
 * @brief Task to handle ElegantOTA connections
 * @deprecated Just use a USB cable if possible
 */
void vTaskOTA(void *pvParameters) {
    while (true) {
        delay(RUN::TASK_INTERVALS::TI_OTA_ms);
    }
}

// MARK: Status Tasks

void vTaskStatusLED(void *pvParameters) {
    while (true) {
        ESP_LOGV(TAG, "vTSL");
        leds.setPixelColor(0, 0x00, 0xFF, 0x00); // Green
        if (leds.canShow()) {
            leds.show();
        }
        delay(LED::BLINK_ON_MILLIS);
        leds.clear();
        delay(LED::BLINK_OFF_MILLIS);
        if (leds.canShow()) {
            leds.show();
        }
    }
}

/**
 * @brief Convert Celsius to Fahrenheit
 */
// consteval uint_fast8_t celsiusToFahrenheit(uint_fast8_t celsius) {
//     return (celsius * 9 / 5) + 32;
// }
// static_assert(celsiusToFahrenheit(0) == 32);
// static_assert(celsiusToFahrenheit(130) <= UINT8_MAX,
//               "Value exceeds uint8_t max");

/**
 * @brief Convert Fahrenheit to Celsius
 */
// consteval uint_fast8_t fahrenheitToCelsius(uint_fast8_t fahrenheit) {
//     return (fahrenheit - 32) * 5 / 9;
// }

constexpr uint32_t ITEMS_TO_LOG = 4;
constexpr uint32_t LOG_ITEM_INTERVAL_MS =
    RUN::TASK_INTERVALS::TI_LOG_DATA_ms / ITEMS_TO_LOG;
/**
 * @brief Task to log data
 */
void vTaskLogData(void *pvParameters) {
    /**
     * @See
     * https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c5/api-reference/peripherals/temp_sensor.html
     * TODO: The temp. sensor may use more power
     * TODO: Temp interput/ callback/ hardware monitoring
     */
    temperature_sensor_handle_t tempSensHandle = NULL;
    temperature_sensor_config_t tempSensConfig =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    ESP_ERROR_CHECK(
        temperature_sensor_install(&tempSensConfig, &tempSensHandle));

    while (true) {
        // if (!Serial.isConnected()) {
        //     delay(LOG_INTERVAL_MS);
        //     continue;
        // }

        ESP_LOGD(TAG, "Logging Data:");

        ESP_LOGI(TAG, "Num tasks reported by FreeRTOS: %u",
                 uxTaskGetNumberOfTasks());

        constexpr uint_fast8_t REC_BYTES_PER_TASK = 40;
        constexpr uint_fast8_t NUM_ESP_TASKS = 8;
        constexpr uint_fast16_t STATS_BUFFER_SIZE =
            REC_BYTES_PER_TASK * (NUM_USR_TASKS + NUM_ESP_TASKS);
        char statsBuffer[STATS_BUFFER_SIZE] = {'\0'};
        if (uxTaskGetNumberOfTasks() > NUM_USR_TASKS + NUM_ESP_TASKS) {
            ESP_LOGE(
                TAG,
                "Number of tasks (%d) exceeds expected max (%d), skipping to "
                "prevent memory corruption",
                uxTaskGetNumberOfTasks(), NUM_USR_TASKS + NUM_ESP_TASKS);
        } else {
            // TODO: Not recommended in production
            vTaskGetRunTimeStats(statsBuffer);
            statsBuffer[STATS_BUFFER_SIZE - 1] =
                '\0'; // hard cap, avoid over-read
            // uxTaskGetSystemState();
            ESP_LOGI(TAG, "Task Run Time Stats:\n%s", statsBuffer);
            delay(LOG_ITEM_INTERVAL_MS);

            for (TaskInfo &taskDesc : taskDescriptions) {
                taskDesc.minFreeStack_Bytes =
                    uxTaskGetStackHighWaterMark(taskDesc.pxCreatedTask);
                ESP_LOGI(TAG, "T: %s, U: %u, F: %u", taskDesc.name,
                         taskDesc.stackSize_bytes - taskDesc.minFreeStack_Bytes,
                         taskDesc.minFreeStack_Bytes);
            }
        }
        delay(LOG_ITEM_INTERVAL_MS);

        ESP_LOGI(TAG, "Minimum free heap: %u bytes",
                 esp_get_minimum_free_heap_size());
        delay(LOG_ITEM_INTERVAL_MS);

        // Enable temperature sensor
        ESP_ERROR_CHECK(temperature_sensor_enable(tempSensHandle));
        // Get converted sensor data
        float tsens_out;
        ESP_ERROR_CHECK(
            temperature_sensor_get_celsius(tempSensHandle, &tsens_out));
        int32_t tempTrunc_C = (int32_t)tsens_out;
        constexpr int32_t MAX_EXT_TEMP = 105;
        constexpr int32_t MIN_EXT_TEMP = -40;
        if (tempTrunc_C > MAX_EXT_TEMP || tempTrunc_C < MIN_EXT_TEMP) {
            ESP_LOGE(TAG, "Temperature out of bounds: %d dC", tempTrunc_C);
        } else {
            ESP_LOGI(TAG, "Temperature: %d dC", tempTrunc_C);
        }
        // Disable the temperature sensor if it is not needed and save the power
        ESP_ERROR_CHECK(temperature_sensor_disable(tempSensHandle));
        delay(LOG_ITEM_INTERVAL_MS);

        // esp_wifi_get_bandwidth
        // esp_wifi_sta_get_rssi
    }
}

/**
 * MARK: loop
 * Arduino: put your main code here, to run repeatedly:
 */
void loop() {
    // ESP_LOGI(TAG, "Time: %llu", SyncedClock::getSystemTimer());
    delay(1000);
}
