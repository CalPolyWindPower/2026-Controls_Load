#pragma once

#include <cstdint>

#include "2026Core/TurbinePacket/TurbinePacket.hpp"
#include "INA260.hpp"
#include "LoadConfig.hpp"
#include "LoadTasks.hpp"
#include "MCP23008T.hpp"
#include <Adafruit_INA260.h>
#include <atomic>

/**
 * @brief Class to manage the container for load data
 */
class LoadContainer {
  public:
    static constexpr const char *TAG = "LC";
    // constexpr uint_fast8_t NUM_MAIN_TASKS = 1;
    // Arduino Loop has priority 1
    // TODO: Note: Task priority must be < 25

    TaskInfo tFSM{vTaskUpdateFSM, "FSM", 2048, nullptr, 20, nullptr, 0, false};
    TaskInfo tPoll{vTaskPollSensors, "Poll", 4096, nullptr, 20,
                   nullptr,          0,      false};
    TaskInfo tAdjustLoad{vTaskAdjustLoad, "AdLd", 2048, nullptr, 20,
                         nullptr,         0,      false};
    TaskInfo tRecv{vTaskRecvData, "Recv", 2048, nullptr, 15, nullptr, 0, false};

    TaskInfo tSend{vTaskSendData, "Send", 4096, nullptr, 15, nullptr, 0, false};
    TaskInfo tCfg{vTaskConfigure, "Cfg", 1024, nullptr, 10, nullptr, 0, false};
    TaskInfo tLED{vTaskStatusLED, "LED", 4096, nullptr, 2, nullptr, 0, false};
    TaskInfo tLog{vTLog, "Log", 4096, nullptr, 2, nullptr, 0, false};
    // namespace TaskInfo

    etl::array<TaskInfo *, NUM_MAIN_TASKS> mainTaskDescriptions = {
        &tFSM, &tPoll, &tAdjustLoad, &tRecv, &tSend, &tCfg, &tLED, &tLog};

    TaskInfo tTelnet{vTaskTelnet, "Telnet", 4096, nullptr,
                     1,           nullptr,  1,    false};
    TaskInfo tOTA{vTaskOTA, "OTA", 4096, nullptr, 1, nullptr, 0, false};

    etl::array<TaskInfo *, NUM_OPTIONAL_TASKS> optionalTaskDescriptions = {
        &tTelnet, &tOTA};

    LoadContainer(MCP23008T &loadDevice, LoadComms &loadComms)
        : loadDevice(loadDevice), loadComms(loadComms) {}
    ~LoadContainer() = default;

    inline ESTOP_TYPE_FAST getSafetyFlag() const { return (safetyFlag); }
    inline bool isPowerPositive() const { return (INA260::current_mA > 0); }
    inline bool isSteadyRPM() const {
        return (angularAccel_RPMPS < 20);
    } // todo - steady power is actually more important
    inline bool isTargetRPMExceeded() const {
        constexpr uint_fast16_t TARGET_RPM = 2200; // todo
        return (currentRPM > TARGET_RPM);
    } // todo

    inline void updateSafetyFlag() {
        ESP_LOGD(TAG, "ESTOP: %d", digitalRead(UM_PROS3::ESTOP_PIN));
        if (digitalRead(UM_PROS3::ESTOP_PIN) == HIGH) {
            this->safetyFlag = ESTOP_TYPE_FAST::BUTTON;
        } else if (abs(INA260::current_mA) <
                   PSENSOR::LOAD_SHED_I_THRESHOLD_mA) {
            this->safetyFlag = ESTOP_TYPE_FAST::LOAD_DISCONNECT;
        } else {
            this->safetyFlag = ESTOP_TYPE_FAST::NONE;
        }
    }
    // inline void updatePowerPositive(bool powerPositive) {
    //     this->powerPositive = powerPositive;
    // }
    inline void setLoadGPIO(uint_fast8_t value) {
        bool result = loadDevice.setGPIO(static_cast<uint32_t>(value));
        if (!result) {
            ESP_LOGE(TAG, "Failed to set load GPIO");
        }
    }
    inline void setRPM(int_fast16_t rpm) { this->currentRPM = rpm; }
    inline int_fast16_t getRPM() const { return this->currentRPM; }
    inline void setAngularAccel_RPMPS(int_fast16_t angularAccel_RPMPS) {
        this->angularAccel_RPMPS = angularAccel_RPMPS;
    }
    inline int_fast16_t getAngularAccel_RPMPS() const {
        return this->angularAccel_RPMPS;
    }

    static constexpr uint_fast8_t LOG_STRING_SIZE =
        3 + 7 + 5 + 5 + 10 + 5 + 6 +
        1; // TODO - improve this and null terminator may not be needed
    /**
     * @brief Get at string that describes the current state of the PID instance
     * @returns the current state of the PID instance as a string
     */
    etl::string<LOG_STRING_SIZE> getLogString() {
        etl::string<LOG_STRING_SIZE> logString(TAG); // 3 chars
        (void)logString.append(": RPM: ");           // 7 chars

        etl::format_spec decFormatA;
        (void)decFormatA.width(5).fill('0'); // [5 chars]
        /**
         * @details I don't think we need strong guarantees on logging data
         * @see
         * https://stackoverflow.com/questions/12346487/what-do-each-memory-order-mean
         * @see https://en.cppreference.com/cpp/atomic/memory_order
         */
        etl::to_string(currentRPM.load(std::memory_order::relaxed), logString,
                       decFormatA, true);     // 5 chars
        (void)logString.append(", dRPM/s: "); // 10 chars

        etl::to_string(getAngularAccel_RPMPS(), logString, decFormatA,
                       true); // 5 chars

        (void)logString.append(", SF: "); // 6 chars

        etl::format_spec decFormatB;
        decFormatB.width(1).fill('0'); // [1 chars]
        etl::to_string(static_cast<uint_fast8_t>(safetyFlag.load()), logString,
                       decFormatB, true); // 1 char

        return logString;
    }

  private:
    MCP23008T &loadDevice;
    std::atomic<ESTOP_TYPE_FAST> safetyFlag =
        ESTOP_TYPE_FAST::NONE; // todo - make atomic?
    LoadComms &loadComms;
    // bool powerPositive = false;
    int_fast16_t angularAccel_RPMPS = 0; // todo

    /**
     * @brief Check for C++17 support, which allows us to verify if std::atomic
     * is a acceptable (lock free) solution for shared variables
     * @see https://stackoverflow.com/a/49915536
     */
    static_assert(
        (__cplusplus >= 201703L),
        "C++17 or higher is required for std::atomic is_always_lock_free");

    /**
     * @brief Do some basic checks regarding std::atomic and data types
     * From C++14.2.0 atomic.h:
     * Check Lock-free property.
     *
     * 0 indicates that the types are never lock-free.
     * 1 indicates that the types are sometimes lock-free.
     * 2 indicates that the types are always lock-free.
     */
    static_assert(sizeof(int) == sizeof(int_fast16_t),
                  "Atomic Lock-free check issue");
#if (ATOMIC_INT_LOCK_FREE == 0)
#    error "Atomic operations on int are not lock-free on this platform."
#elif (ATOMIC_INT_LOCK_FREE == 1)
#    warning                                                                   \
        "Atomic operations on int are only sometimes lock-free on this platform."
#endif

    /**
     * @brief Check if std::atomic<int_fast16_t> is an acceptable (lock free)
     * solution for shared variables
     * @see https://www.reddit.com/r/embedded/comments/zn23of/comment/j0fav6o/
     * @see
     * https://stackoverflow.com/questions/63471387/should-volatile-still-be-used-for-sharing-data-with-isrs-in-modern-c
     * @see https://en.cppreference.com/w/c/language/atomic.html
     * @see https://en.cppreference.com/w/cpp/atomic/atomic.html
     * @see https://stackoverflow.com/a/16783513
     */
    // static_assert(std::atomic<int_fast16_t>::is_always_lock_free,
    //               "Atomic operations on int_fast16_t are not lock-free on "
    //               "this platform.");
    std::atomic<int_fast16_t> currentRPM = 0; // todo
};
