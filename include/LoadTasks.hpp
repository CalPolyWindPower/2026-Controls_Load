#pragma once

// Includes
#include <cstdint>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <portmacro.h>

// MARK: Datatypes & Constants
struct TaskInfo {
    const TaskFunction_t function = nullptr;
    const char *const name = nullptr;
    const configSTACK_DEPTH_TYPE stackSize_bytes = 1024;
    void *const pvParameters = nullptr;
    const UBaseType_t priority; // Note: Task priority must be <25 for some
                                // reason, possible bug
    TaskHandle_t pxHandle = nullptr;
    UBaseType_t minFreeStack_Bytes = 0;
    bool initialized = false;
};

// MARK: Constants
#define DBG_NUM_DEL_TASKS 0
#if DBG_NUM_DEL_TASKS > 0
#warning "Debug Tasks list will be partially skipped
#endif
constexpr uint_fast8_t NUM_MAIN_TASKS = 8 - DBG_NUM_DEL_TASKS;     // Must match number of entires!
constexpr uint_fast8_t NUM_OPTIONAL_TASKS = 2; // Must match number of entires!

// MARK: Function Prototypes
// Main Tasks
/**
 * @brief Task to control run the FSM
 */
[[noreturn]] void vTaskUpdateFSM([[maybe_unused]] void *pvParameters);
/**
 * @brief Task to poll high priority sensors
 */
[[noreturn]] void vTaskPollSensors([[maybe_unused]] void *pvParameters);
/**
 * @brief Task to control the load
 */
[[noreturn]] void vTaskAdjustLoad([[maybe_unused]] void *pvParameters);
/**
 * @brief Task to handle inbound data that has been queued
 */
[[noreturn]] void vTaskRecvData([[maybe_unused]] void *pvParameters);

/**
 * @brief Task to handle outbound data that has been queued
 */
[[noreturn]] void vTaskSendData([[maybe_unused]] void *pvParameters);
[[noreturn]] void vTaskConfigure([[maybe_unused]] void *pvParameters);
[[noreturn]] void vTaskStatusLED([[maybe_unused]] void *pvParameters);
/**
 * @brief Task to log data
 */
[[noreturn]] void vTLog([[maybe_unused]] void *pvParameters);

// Optional Tasks
/**
 * @brief Task to handle Telnet connections
 * @deprecated Just use a USB cable if possible
 */
[[noreturn]] void vTaskTelnet([[maybe_unused]] void *pvParameters);
/**
 * @brief Task to handle ElegantOTA connections
 * @deprecated Just use a USB cable if possible
 */
[[noreturn]] void vTaskOTA([[maybe_unused]] void *pvParameters);
