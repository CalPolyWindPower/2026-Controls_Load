// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// https://pvs-studio.com

/* Includes */
// System Includes
#include <esp_log.h>
#include <temperature_sensor.h>

// Library Includes
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// Include LoadConfig first
#include "LoadConfig.hpp"
// Project Includes
#include "2026Core/CommonConfig.hpp" // Include after NacelleConfig due to macro precednece
#include "INA260.hpp"
#include "LoadComms.hpp"
#include "LoadContainer.hpp"
#include "LoadFSM.hpp"
#include "LoadTasks.hpp"
#include "MCP23008T.hpp"
#define COMMS_STRATEGY_OLD 0
#define COMMS_STRATEGY_NEW 1
#define COMMS_STRATEGY_UHCI 2
#define COMMS_STRATEGY COMMS_STRATEGY_NEW
#if COMMS_STRATEGY == COMMS_STRATEGY_OLD
#    include "2026Core/Net/Net-Application/NTP.hpp"
#    include "2026Core/Net/Net-Application/OTA.hpp"
#    include "2026Core/Net/Net-Application/Telnet.hpp"
#    include "2026Core/Net/Net-Link/AdapterESPNow.hpp"
#    include "2026Core/Net/Net-Phy/AdapterWLAN.hpp"
#    include "2026Core/Net/NetAdapter_A.hpp"
#elif COMMS_STRATEGY == COMMS_STRATEGY_NEW
#    include "2026Core/TurbinePacket/TurbinePacket.hpp"
#elif COMMS_STRATEGY == COMMS_STRATEGY_UHCI
#    include "2026Core/Net/Net-Link/AdapterUHCI.hpp"
#else
#    error "Invalid COMMS_STRATEGY"
#endif

/* Config */
static constexpr const char *TAG = "LoMa";

// MARK: Function Prototypes

// Helper Functions
bool configureLoad();

// MARK:  Global Objects

LoadComms loadComms;
// AdapterWLAN adapterWLAN = AdapterWLAN();
// AdapterWLAN adapterWLAN;
// AdapterESPNow adapterESPNow = AdapterESPNow();
// AdapterESPNow adapterESPNow;
// SyncedClock netClock = SyncedClock(adapterESPNow); // todo
// SyncedClock netClock(adapterESPNow); // todo

Adafruit_NeoPixel leds(1, UM_PROS3::LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
MCP23008T
loadDevice(static_cast<MCP23008T::I2C_ADDRESS>(LOAD::I2C_ADDRESS),
           &Wire); // AI: Does not protect against invalid addresses, but
                   // will fail to initialize if address is not valid`
bool loadConfigured = false;

LoadContainer load(loadDevice, loadComms);
LoadFSM loadFSM(load);

// static uint_fast32_t txEvents = 0; // DONE: check against last years code
// static uint_fast32_t bytesSent = 0;
// static uint_fast32_t bytesNotSent = 0;
// static uint_fast32_t rxEvents = 0;
// static uint_fast32_t bytesReceived = 0;

// todo: move
bool configureLoad() {
    if (!INA260::begin(PSENSOR::I2C_ADDRESS, PSENSOR::AVG_COUNT,
                       PSENSOR::CONV_TIME)) {
        ESP_LOGE(TAG, "Failed to initialize INA260 power sensor at 0x%02X",
                 PSENSOR::I2C_ADDRESS);
        return false;
    } else {
        ESP_LOGI(TAG, "INA260 power sensor init at 0x%02X",
                 PSENSOR::I2C_ADDRESS);
    }

    if (!loadDevice.begin()) {
        ESP_LOGE(TAG, "Failed to initialize MCP23008T device at 0x%02X",
                 loadDevice.getAddress());
        return false;
    } else {
        ESP_LOGI(TAG, "MCP23008T IO device init at 0x%02X",
                 loadDevice.getAddress());
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

// todo: INA260 addresses: INA260:	0x40, 0x41, 0x44, 0x45

/**
 * @brief Helper - Checks canShow() and then shows them.
 * @return Whether the LEDs were updated
 */
inline bool showLEDsIfReady() {
    if (leds.canShow()) {
        leds.show();
        return true;
    } else {
        return false;
    }
}

/**
 * MARK: Setup
 * put your setup code here, to run once:
 */
void setup() {
    static bool serialInitialized = false;
    if (!serialInitialized) {
        Serial.begin(115200);
        size_t txBuffer = Serial.setTxBufferSize(1024);
        ESP_LOGI(TAG, "Serial initialized @ %d baud w/ buffer size %d",
                 Serial.baudRate(), txBuffer);
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
    (void)showLEDsIfReady();

    // Configure ESTOP pin
    pinMode(UM_PROS3::ESTOP_PIN, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(UM_PROS3::ESTOP_PIN),
        []() {
            // Note: This ISR is not guaranteed to trigger on every ESTOP press,
            // but that's acceptable as the main safety check is in the FSM
            // task, and this is just a backup. Also, we want to avoid doing too
            // much in the ISR to prevent potential issues.
            load.updateSafetyFlag();
        },
        CHANGE);

    // Configure New ESP-NOW + WiFI implementation

    // Configure WiFi
    // static bool wifiInitialized = false;
    // if (!wifiInitialized) {
    //     leds.setPixelColor(0, 0x00, 0x00, 0xFF); // blue
    //     (void)showLEDsIfReady();
    //     uint8_t optimalChannel = adapterWLAN.identifyOptimalChannel();
    //     leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    //     (void)showLEDsIfReady();
    //     ESP_LOGI(TAG, "Optimal WiFi Channel: %d", optimalChannel);
    //     if (adapterWLAN.begin(optimalChannel)) {
    //         ESP_LOGI(TAG, "WiFi initialized");
    //         wifiInitialized = true;
    //     } else {
    //         ESP_LOGE(TAG, "Failed to initialize WiFi");
    //     }
    // }
    // leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    // (void)showLEDsIfReady();

    // Configure ESP-NOW
    static bool espNowInitalized = false;
    if (loadComms.begin()) {
        espNowInitalized = true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize load comms");
    }
    // if (!espNowInitalized) {
    //     if (adapterESPNow.begin()) {
    //         ESP_LOGI(TAG, "ESP-NOW initialized.");
    //         espNowInitalized = true;
    //     } else {
    //         ESP_LOGE(TAG, "Failed to initialize ESP-NOW");
    //     }
    // }
    // leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    // (void)showLEDsIfReady();

    // Configure ESP-NOW Peers
    // static bool peerRegistered = false;
    // if (!peerRegistered) {
    //     if (adapterESPNow.registerPeer(WTbNetConfig::NACELLE_MAC)) {
    //         ESP_LOGI(TAG, "Registered peer");
    //         peerRegistered = true;
    //     } else {
    //         ESP_LOGE(TAG, "Failed to register peer");
    //     }
    // }
    // leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    // (void)showLEDsIfReady();

    // Sync Time // FIXME! - Load accesses fault
    // static bool timeSynced = false;
    // if (!timeSynced) {
    //     if (netClock.initTimeSync(WTbNetConfig::LOAD_MAC)) {
    //         ESP_LOGI(TAG, "Time sync initialized successfully");
    //         timeSynced = true;
    //     } else {
    //         ESP_LOGE(TAG, "Failed to initialize time sync");
    //     }
    // }
    // leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    // (void)showLEDsIfReady();

    // Print MAC Address // todo - verify
    // ESP_LOGI(
    //     TAG, "MAC Address: %s",
    //     AdapterWLAN::formatMACAddress(adapterWLAN.getMACAddress()).c_str());
    // leds.setPixelColor(0, 0xFF, 0xA5, 0x00); // orange
    // (void)showLEDsIfReady();

    // TODO: Check ESP-NOW impl against last years
    // TODO: Configure response handler, load server

    // Configure I2C
    if (!Wire.begin(UM_PROS3::I2C_SDA_PIN, UM_PROS3::I2C_SCL_PIN)) {
        ESP_LOGE(TAG, "Failed to initialize I2C on pins %d (SDA) and %d (SCL)",
                 UM_PROS3::I2C_SDA_PIN, UM_PROS3::I2C_SCL_PIN);
    } else {
        // Constant error spam when set to 1
        Wire.setTimeOut(2);
    }

    loadConfigured = configureLoad();
    leds.setPixelColor(0, 0x00, 0xFF, 0x00); // green
    (void)showLEDsIfReady();

    // Set up tasks
    static bool tasksSetup = false;
    if (!tasksSetup) {
        ESP_LOGI(TAG, "Setting up tasks...");
        for (TaskInfo *taskDesc : load.mainTaskDescriptions) {
            if (taskDesc == nullptr) {
                ESP_LOGE(TAG, "Caught null task description pointer!");
                continue;
            }

            if (taskDesc->function == nullptr) {
                ESP_LOGE(TAG, "Caught null task function!");
                continue;
            }

            /**
             * @details PVS-Studio: "The modulo by 1 operation is meaningless.
             * The result will always be zero."
             * @see https://pvs-studio.com/en/docs/warnings/v1063/
             * @details PVS=Studio: "Expression is always false."
             * @see https://pvs-studio.com/en/docs/warnings/v547/
             */
            if (taskDesc->stackSize_bytes % sizeof(StackType_t) != 0) {
                ESP_LOGE(TAG, "Stack size not aligned");
                continue;
            }

            /**
             * @details For efficiency, stacks should be aligned to the fastest
             * data type available
             */
            if (taskDesc->stackSize_bytes % sizeof(uint_fast8_t) != 0) {
                ESP_LOGW(TAG, "Stack size not word aligned");
            }
            // Syntax: xTaskCreate(Task function, Name of the task (for
            // debugging), Stack size (in bytes, not words), Task input
            // parameter, Priority of the task, Task handle)
            static portMUX_TYPE taskSetupLock = portMUX_INITIALIZER_UNLOCKED;
            taskENTER_CRITICAL(&taskSetupLock);
            BaseType_t result =
                xTaskCreate(taskDesc->function, taskDesc->name,
                            taskDesc->stackSize_bytes, taskDesc->pvParameters,
                            taskDesc->priority, &(taskDesc->pxHandle));
            if (result == pdPASS && taskDesc->initSuspended) {
                vTaskSuspend(taskDesc->pxHandle);
            }
            taskEXIT_CRITICAL(&taskSetupLock);

            if (result != pdPASS) {
                ESP_LOGE(TAG, "Failed to create task %s", taskDesc->name);
            } else {
                ESP_LOGD(TAG,
                         "Created task %s: Priority %u, Stack size %u bytes, "
                         "Suspended: %d",
                         taskDesc->name, taskDesc->priority,
                         taskDesc->stackSize_bytes, taskDesc->initSuspended);
            }
        }
        vTaskResume(load.tFSM.pxHandle);
        vTaskResume(load.tCfg.pxHandle);
        // vTaskResume(load.tPoll.pxHandle); // TODO: Verify that this isn't
        // needed
        tasksSetup = true;

        // pitchPIDController.enable(
        //     0.0f, 1500.0f); // todo - just a quick performances test

        // Configure FSM
        // PVS-Studio: One of these is always true...
        LoadFSM::UPDATE_RESULT result = loadFSM.updateState();
        if (result == LoadFSM::UPDATE_RESULT::ERROR) {
            // PVS=Studio: "Expression [...] is always false."
            ESP_LOGE(TAG, "Error during FSM init., %d",
                     static_cast<uint_fast8_t>(result));
        } else if (result == LoadFSM::UPDATE_RESULT::STATE_CHANGED) {
            ESP_LOGI(TAG, "Initialized FSM to state %d",
                     static_cast<uint_fast8_t>(loadFSM.getCurrentState()));
        } else if (result == LoadFSM::UPDATE_RESULT::NO_CHANGE) {
            ESP_LOGE(TAG, "Failed to enter a valid state");
        } else {
            ESP_LOGE(TAG, "Unknown FSM init. result: %d",
                     static_cast<uint_fast8_t>(result));
        }

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
[[noreturn]] void
vTaskUpdateFSM([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        static TickType_t xLastWakeTime = xTaskGetTickCount();

        LoadFSM::UPDATE_RESULT result = loadFSM.updateState();
        if (result == LoadFSM::UPDATE_RESULT::STATE_CHANGED) {
            ESP_LOGI(TAG, "FSM State Changed: %d", loadFSM.getCurrentState());
        } else if (result == LoadFSM::UPDATE_RESULT::ERROR) {
            // PVS-Studio says this is always false
            ESP_LOGE(TAG, "Error updating FSM state");
        } else {
            // No change, nothing to log
        }

        BaseType_t xWasDelayed = xTaskDelayUntil(
            &xLastWakeTime, pdMS_TO_TICKS(RUN::TASK_INTERVALS::TI_FSM_mS));
        if (xWasDelayed == pdFALSE) {
            ESP_LOGE(TAG, "Timing");
        }
    }
}

/**
 * @brief Task to poll high priority sensors
 */
[[noreturn]] void vTPollS([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        static TickType_t xLastWakeTime = xTaskGetTickCount();

        // load.updateSafetyFlag(); // Moved to interrupts before noticing that
        // the backoff was the problem, might as well leave it that way

        // static uint32_t backoffFactor =
        //     RUN::TASK_INTERVALS::FAIL_BACKOFF_BASE_FACTOR;
        // uint32_t delay_ms = 0;
        // if (INA260::updateReadings()) {
        //     backoffFactor = RUN::TASK_INTERVALS::FAIL_BACKOFF_BASE_FACTOR;
        //     delay_ms = RUN::TASK_INTERVALS::TI_POLL_SENSORS_mS;
        // } else {
        //     // Logging already handled
        //     delay_ms = RUN::TASK_INTERVALS::TI_POLL_SENSORS_mS *
        //     backoffFactor; backoffFactor *=
        //     RUN::TASK_INTERVALS::FAIL_BACKOFF_MULTIPLIER;
        // }
        INA260::updateReadings();

        BaseType_t xWasDelayed = xTaskDelayUntil(
            &xLastWakeTime,
            pdMS_TO_TICKS(RUN::TASK_INTERVALS::TI_POLL_SENSORS_mS));
        // BaseType_t xWasDelayed =
        //     xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(delay_ms));
        if (xWasDelayed == pdFALSE) {
            ESP_LOGE(TAG, "Timing"); // FIXME!
        }
    }
}

/**
 * @brief Task to control the pitch actuator
 */
[[noreturn]] void
vTaskAdjustLoad([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        static int i = 0;
        // ESP_LOGI(TAG, "Pitch PID Output: %f",
        //          pitchPIDController.compute(
        //              i)); // todo - just a quick performances test
        // i += 20;
        static int_fast32_t lastPower = INT_FAST32_MIN;
        static int_fast8_t powerIndex = 46;
        // if (INA260::current_mA > 0) {
        /** @deprecated first check */
        if (loadFSM.getCurrentState() != FSMCommon::States::sRunLoad) {
            // Don't run at startup
            delay(RUN::TASK_INTERVALS::TI_ADJUST_LOAD_mS);
            continue;
        } else if (abs(INA260::dPower_mWPS) > (abs(INA260::power_mW) * 0.05)) {
            // Wait for power to stabilize
        } else if ((lastPower == INT_FAST32_MIN) && (powerIndex > 0)) {
            powerIndex--;
            load.setLoadGPIO(LOAD::RES_INDEX_TABLE[powerIndex]);
            ESP_LOGD(TAG, "Last power: %d mW, current power: %d mW",
                     static_cast<int>(lastPower),
                     static_cast<int>(INA260::power_mW));
            ESP_LOGI(TAG, "Initial load adjustment, setpoint: %d",
                     LOAD::RES_INDEX_TABLE[powerIndex]);
        } else if ((INA260::power_mW > lastPower) && (powerIndex > 0)) {
            powerIndex--;
            load.setLoadGPIO(LOAD::RES_INDEX_TABLE[powerIndex]);
            ESP_LOGD(TAG, "Last power: %d mW, current power: %d mW",
                     static_cast<int>(lastPower),
                     static_cast<int>(INA260::power_mW));
            ESP_LOGI(TAG, "Decreasing load to %d",
                     LOAD::RES_INDEX_TABLE[powerIndex]);
        } else if ((INA260::power_mW < lastPower) && (powerIndex < 46)) {
            powerIndex++;
            load.setLoadGPIO(LOAD::RES_INDEX_TABLE[powerIndex]);
            ESP_LOGI(TAG, "Last power: %d mW, current power: %d mW",
                     static_cast<int>(lastPower),
                     static_cast<int>(INA260::power_mW));
            ESP_LOGI(TAG, "Increasing load to %d",
                     LOAD::RES_INDEX_TABLE[powerIndex]);
            delay(20 *
                  1000); // todo RUN::TASK_INTERVALS::TI_ADJUST_LOAD_mS * 100
        } else { // todo: change trigger to when rpm changes or when power
                 // changes
            // No change
        }
        lastPower = INA260::power_mW;

        delay(RUN::TASK_INTERVALS::TI_ADJUST_LOAD_mS);
    }
}

/**
 * @brief Task to control run the variable load
 */
// [[noreturn]] void vTaskVarLoad([[maybe_unused]] void
// *pvParameters) { // NOSONAR
//     while (true) {
//     }
// }

// MARK: Network Tasks

/**
 * @brief Task to handle inbound data that has been queued
 */
[[noreturn]] void
vTaskRecvData([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        static TickType_t xLastWakeTime = xTaskGetTickCount();

        NacellePacket packet;
        if (xQueueReceive(LoadComms::priorityDataQueue, &packet,
                          RUN::TASK_INTERVALS::TI_RECV_ms) == pdPASS) {
            ESP_LOGV(TAG, "Received packet: rpm=%u", packet.rpm);
            load.setRPM(packet.rpm);
            load.setAngularAccel_RPMPS(packet.angularAccel_RPMPS);
        }

        // BaseType_t xWasDelayed = xTaskDelayUntil(
        //     &xLastWakeTime, pdMS_TO_TICKS(RUN::TASK_INTERVALS::TI_RECV_ms));
        // if (xWasDelayed == pdFALSE) {
        //     ESP_LOGE(TAG, "Timing");
        // }

        // Never need to suspend on the load
    }
}

/**
 * @brief Task to handle inbound data that has been queued
 */
// [[noreturn]] void vTaskHandleInboundData([[maybe_unused]] void
// *pvParameters) { // NOSONAR
//     while (true) {
//     }
// }

/**
 * @brief Task to handle outbound data that has been queued
 */
// [[noreturn]] void vTaskHandleOutboundData([[maybe_unused]] void
// *pvParameters) { // NOSONAR
//     while (true) {
//     }
// }

/**
 * @brief Task to handle outbound data
 */
[[noreturn]] void
vTaskSendData([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        static TickType_t xLastWakeTime = xTaskGetTickCount();
        static uint32_t backoffFactor =
            RUN::TASK_INTERVALS::FAIL_BACKOFF_BASE_FACTOR;

        uint32_t delay_ms = 0;
        if (loadComms.sendLoadboxData(
                INA260::dVoltage_mVPS, INA260::current_mA,
                INA260::dCurrent_mAPS,
                static_cast<ESTOP_TYPE_NET>(load.getSafetyFlag()))) {
            backoffFactor = RUN::TASK_INTERVALS::FAIL_BACKOFF_BASE_FACTOR;
            delay_ms = RUN::TASK_INTERVALS::TI_SEND_ms;
        } else {
            // Logging already handled
            delay_ms = RUN::TASK_INTERVALS::TI_SEND_ms * backoffFactor;
            backoffFactor *= RUN::TASK_INTERVALS::FAIL_BACKOFF_MULTIPLIER;
        }

        BaseType_t xWasDelayed =
            xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(delay_ms));
        if (xWasDelayed == pdFALSE) {
            ESP_LOGE(TAG, "Timing");
        }
    }
}

// MARK: Utility Tasks

[[noreturn]] void
vTaskConfigure([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        // if (!loadConfigured) {
        //     if (loadDevice.begin()) { // FIXME!
        //         ESP_LOGI(TAG, "MCP23008T initialized successfully.");
        //         loadConfigured = true;
        //     } else {
        //         ESP_LOGE(
        //             TAG,
        //             "Failed to initialize MCP23008T. Retrying in 5
        //             seconds...");
        //         delay(5000);
        //     }
        // } else {
        // Sleep for 30 seconds
        delay(RUN::TASK_INTERVALS::TI_CFG_ms);
        // }
    }
}

/**
 * @brief Task to handle Telnet connections
 */
[[noreturn]] void vTaskTelnet([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        // TELNET::loop(); // todo
        delay(RUN::TASK_INTERVALS::TI_TELNET_ms);
    }
}

/**
 * @brief Task to handle ElegantOTA connections
 * @deprecated Just use a USB cable if possible
 */
[[noreturn]] void vTaskOTA([[maybe_unused]] void *pvParameters) { // NOSONAR
    while (true) {
        delay(RUN::TASK_INTERVALS::TI_OTA_ms);
    }
}

// MARK: Status Tasks

[[noreturn]] void
vTaskStatusLED([[maybe_unused]] void *pvParameters) { // NOSONAR
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

// constexpr uint32_t ITEMS_TO_LOG = 4;
constexpr uint32_t LOG_ITEM_INTERVAL_MS = RUN::TASK_INTERVALS::TI_LOG_DATA_ms;
/**
 * @brief Task to log data
 */
[[noreturn]] void vTLog([[maybe_unused]] void *pvParameters) { // NOSONAR
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

        ESP_LOGI(TAG, "FreeRTOS Tasks: %u", uxTaskGetNumberOfTasks());

        constexpr uint_fast8_t REC_BYTES_PER_TASK = 40;
        constexpr uint_fast8_t NUM_ESP_TASKS =
            11; // TODO: Why was this set to 8 and why did I need to increase it
                // by 3?
        constexpr uint_fast16_t STATS_BUFFER_SIZE =
            REC_BYTES_PER_TASK * (NUM_MAIN_TASKS + NUM_ESP_TASKS);
        char statsBuffer[STATS_BUFFER_SIZE] = {'\0'};
        if (uxTaskGetNumberOfTasks() > NUM_MAIN_TASKS + NUM_ESP_TASKS) {
            ESP_LOGE(TAG,
                     "Task count (%d) > (%d), skipping to prevent memory "
                     "corruption",
                     uxTaskGetNumberOfTasks(), NUM_MAIN_TASKS + NUM_ESP_TASKS);
            // delay(LOG_ITEM_INTERVAL_MS);
        } else {
            // } else if (statsBuffer[0] == '\0') {
            // Refresh stats buffer

            // TODO: Not recommended in production
            vTaskGetRunTimeStats(statsBuffer);
            statsBuffer[STATS_BUFFER_SIZE - 1] =
                '\0'; // hard cap, avoid over-read
            // uxTaskGetSystemState();
            size_t usedBytes = strnlen(statsBuffer, STATS_BUFFER_SIZE);
            ESP_LOGD(TAG, "Task Buff U (%): %d",
                     usedBytes * 100 / sizeof(statsBuffer));
            ESP_LOGD(TAG, "Stats Buff U: %d bytes", usedBytes);
            ESP_LOGD(TAG, "Stats Buff F: %d bytes",
                     sizeof(statsBuffer) - usedBytes);
            ESP_LOGD(TAG, "Stats Buff Sz: %d", sizeof(statsBuffer));
            ESP_LOGI(TAG, "Task RT Stats:\n%s", statsBuffer);
            // Serial.flush();
            // delay(LOG_ITEM_INTERVAL_MS);

            for (TaskInfo *taskDesc : load.mainTaskDescriptions) {
                if (taskDesc == nullptr) {
                    ESP_LOGE(TAG, "Caught null task desc. ptr!");
                    continue;
                }

                if (taskDesc->pxHandle == nullptr) {
                    ESP_LOGE(TAG, "Caught null task handle!");
                    continue;
                }

                taskDesc->minFreeStack_Bytes =
                    uxTaskGetStackHighWaterMark(taskDesc->pxHandle);
                ESP_LOGI(TAG, "T: %s, U: %u, F: %u", taskDesc->name,
                         taskDesc->stackSize_bytes -
                             taskDesc->minFreeStack_Bytes,
                         taskDesc->minFreeStack_Bytes);
            }
            // Serial.flush();
            // delay(LOG_ITEM_INTERVAL_MS);
        }

        ESP_LOGI(TAG, "Min, free heap: %u b", esp_get_minimum_free_heap_size());
        // delay(LOG_ITEM_INTERVAL_MS);

        // Enable temperature sensor
        ESP_ERROR_CHECK(temperature_sensor_enable(tempSensHandle));
        // Get converted sensor data
        float tsens_out;
        ESP_ERROR_CHECK(
            temperature_sensor_get_celsius(tempSensHandle, &tsens_out));
        auto tempTrunc_C = static_cast<int32_t>(tsens_out);
        constexpr int32_t MAX_EXT_TEMP = 105;
        constexpr int32_t MIN_EXT_TEMP = -40;
        if (tempTrunc_C > MAX_EXT_TEMP || tempTrunc_C < MIN_EXT_TEMP) {
            ESP_LOGE(TAG, "Temp. out of bounds: %d dC", tempTrunc_C);
        } else {
            ESP_LOGI(TAG, "Temp.: %d dC", tempTrunc_C);
        }
        // Disable the temperature sensor if it is not needed and save the power
        ESP_ERROR_CHECK(temperature_sensor_disable(tempSensHandle));
        // delay(LOG_ITEM_INTERVAL_MS);

        ESP_LOGI(TAG, "Curr. State: %d", loadFSM.getCurrentState());
        // delay(LOG_ITEM_INTERVAL_MS);

        // TODO: Improve logging, check ESTOP logic

        ESP_LOGI(TAG, "%s", INA260::getLogString().c_str());
        // ESP_LOGI(TAG, "Curr. State: %d", loadFSM.getCurrentState());
        ESP_LOGI(TAG, "%S", load.getLogString().c_str());
        ESP_LOGI(TAG, "%s", loadDevice.getLogString().c_str());

        static unsigned int prevTime_us = 0;
        static LoadComms::LogData lastLogData = {0};
        unsigned int currentTime_us = micros();
        ESP_LOGI(TAG, "%s", loadComms.getLogString().c_str());
        LoadComms::LogData currentLogData = loadComms.getLogData();
        unsigned long deltaTime_us = currentTime_us - prevTime_us;
        uint_fast32_t deltaTxEvents =
            currentLogData.txEvents - lastLogData.txEvents;
        uint_fast32_t deltaBytesSent =
            currentLogData.bytesSent - lastLogData.bytesSent;
        uint_fast32_t deltaBytesFailed =
            currentLogData.bytesNotSent - lastLogData.bytesNotSent;
        uint_fast32_t deltaRxEvents =
            currentLogData.rxEvents - lastLogData.rxEvents;
        uint_fast32_t deltaBytesReceived =
            currentLogData.bytesReceived - lastLogData.bytesReceived;

        constexpr unsigned long m_TO_BASE = 1000;
        constexpr unsigned long u_TO_m = 1000;
        constexpr unsigned long u_TO_BASE = m_TO_BASE * u_TO_m;
        ESP_LOGI(TAG, "TxE/s: %u, TxBS/s: %u, TxBF/s: %u, RxE/s: %u, RxB/s: %u",
                 deltaTxEvents * u_TO_BASE / deltaTime_us,
                 deltaBytesSent * u_TO_BASE / deltaTime_us,
                 deltaBytesFailed * u_TO_BASE / deltaTime_us,
                 deltaRxEvents * u_TO_BASE / deltaTime_us,
                 deltaBytesReceived * u_TO_BASE / deltaTime_us);
        prevTime_us = currentTime_us;
        lastLogData = currentLogData;

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
