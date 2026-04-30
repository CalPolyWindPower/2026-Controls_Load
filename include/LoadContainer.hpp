#pragma once

#include <cstdint>

#include "INA260.hpp"
#include "LoadConfig.hpp"
#include "LoadTasks.hpp"
#include "MCP23008T.hpp"
#include <Adafruit_INA260.h>

/**
 * @brief Class to manage the container for load data
 */
class LoadContainer {
  public:
    // constexpr uint_fast8_t NUM_MAIN_TASKS = 1;
    // Arduino Loop has priority 1
    // TODO: Note: Task priority must be < 25

    TaskInfo tFSM{vTaskUpdateFSM, "FSM", 1024, nullptr, 24, nullptr, 0, false};
    TaskInfo tPoll{vTaskPollSensors, "Poll", 4096, nullptr, 20,
                   nullptr,          0,      false};
    TaskInfo tAdjustLoad{vTaskAdjustLoad, "AdLd", 2048, nullptr, 20,
                         nullptr,         0,      false};
    TaskInfo tRecv{vTaskRecvData, "Recv", 2048, nullptr, 15, nullptr, 0, false};

    TaskInfo tSend{vTaskSendData, "Send", 4096, nullptr, 15, nullptr, 0, false};
    TaskInfo tCfg{vTaskConfigure, "Cfg", 1024, nullptr, 10, nullptr, 0, false};
    TaskInfo tLED{vTaskStatusLED, "LED", 2048, nullptr, 2, nullptr, 0, false};
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

    inline bool getSafetyFlag() const { return (safetyFlag); }
    inline bool isPowerPositive() const { return (INA260::current_mA > 0); }
    inline bool isSteadyRPM() const {
        return false;
    } // todo - steady power is actually more important
    inline bool isTargetRPMExceeded() const { return false; } // todo

    inline void updateSafetyFlag(bool safetyFlag) {
        this->safetyFlag =
            (digitalRead(UM_PROS3::ESTOP_PIN) == LOW) || (!isPowerPositive());
    }
    // inline void updatePowerPositive(bool powerPositive) {
    //     this->powerPositive = powerPositive;
    // }
    inline void setLoadGPIO(uint_fast8_t value) {
        loadDevice.setGPIO((uint32_t)value);
    }
    inline void setRPM(int_fast16_t rpm) { this->currentRPM = rpm; }

  private:
    MCP23008T &loadDevice;
    bool safetyFlag = false; // todo - make atomic?
    LoadComms &loadComms;
    // bool powerPositive = false;
    int_fast16_t currentRPM = 0; // todo

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
    // std::atomic<int_fast16_t> currentRPM = 0;
};
