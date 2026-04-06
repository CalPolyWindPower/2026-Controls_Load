#pragma once

#include <cstdint>

#include "LoadConfig.hpp"
#include "LoadTasks.hpp"

/**
 * @brief Class to manage the container for load data
 */
class LoadContainer {
  public:
    // constexpr uint_fast8_t NUM_MAIN_TASKS = 1;
    // Arduino Loop has priority 1
    // TODO: Note: Task priority must be < 25
    etl::array<TaskInfo, NUM_MAIN_TASKS> mainTaskDescriptions = {
        TaskInfo{vTaskUpdateFSM, "FSM", 256, nullptr, 24, nullptr, 0,
                 false}, // 0
        TaskInfo{vTaskPollSensors, "Poll", 2048, nullptr, 20, nullptr, 0,
                 false}, // 1
        TaskInfo{vTaskAdjustLoad, "AdLd", 2048, nullptr, 20, nullptr, 0,
                 false}, // 2
        TaskInfo{vTaskRecvData, "Recv", 2048, nullptr, 15, nullptr, 0,
                 false}, // 3

        TaskInfo{vTaskSendData, "Send", 2048, nullptr, 15, nullptr, 0,
                 false}, // 4
        TaskInfo{vTaskConfigure, "Cfg", 512, nullptr, 10, nullptr, 0,
                 false}, // 5
        TaskInfo{vTaskStatusLED, "LED", 256, nullptr, 2, nullptr, 0,
                 false},                                                   // 6
        TaskInfo{vTaskLogData, "Log", 4096, nullptr, 1, nullptr, 0, false} // 7
    };

    etl::array<TaskInfo, NUM_OPTIONAL_TASKS> optionalTaskDescriptions = {
        TaskInfo{vTaskTelnet, "Telnet", 4096, nullptr, 1, nullptr, 0,
                 false},                                                // 0
        TaskInfo{vTaskOTA, "OTA", 4096, nullptr, 1, nullptr, 0, false}, // 1
    };

    /**
     * @SupressWarnings("cpp:S3642") // Does not work
     */
    enum TASK_IDS : uint_fast8_t { // NOSONAR
        TID_POLL = 0,
        TID_ADJUST_LOAD,
        TID_RECV,
        TID_SEND,
        TID_CFG,
        TID_LED,
        TID_LOG
    };

    LoadContainer() = default;
    ~LoadContainer() = default;

    inline bool getSafetyFlag() const { return safetyFlag; }
    inline bool isPowerPositive() const { return powerPositive; } // todo
    inline bool isSteadyRPM() const { return false; }             // todo
    inline bool isTargetRPMExceeded() const { return false; }     // todo

    inline void updateSafetyFlag(bool safetyFlag) {
        this->safetyFlag = (digitalRead(UM_PROS3::ESTOP_PIN) ==
                            LOW) /*|| getPPCCurrent() < threhold*/; // todo
    }
    inline void updatePowerPositive(bool powerPositive) {
        this->powerPositive = powerPositive;
    }

  private:
    // INA260&
    // loa&

    bool safetyFlag = false; // todo
    bool powerPositive = false;
    int_fast16_t currentRPM = 0; // todo
};
