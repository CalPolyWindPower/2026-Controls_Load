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
    TaskInfo tPoll{vTaskPollSensors, "Poll", 2048, nullptr, 20,
                   nullptr,          0,      false};
    TaskInfo tAdjustLoad{vTaskAdjustLoad, "AdLd", 2048, nullptr, 20,
                         nullptr,         0,      false};
    TaskInfo tRecv{vTaskRecvData, "Recv", 2048, nullptr, 15, nullptr, 0, false};

    TaskInfo tSend{vTaskSendData, "Send", 2048, nullptr, 15, nullptr, 0, false};
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

    LoadContainer(MCP23008T &loadDevice) : loadDevice(loadDevice) {}
    ~LoadContainer() = default;

    inline bool getSafetyFlag() const { return safetyFlag; }
    inline bool isPowerPositive() const { return (INA260::current_mA > 0); }
    inline bool isSteadyRPM() const { return false; }         // todo
    inline bool isTargetRPMExceeded() const { return false; } // todo

    inline void updateSafetyFlag(bool safetyFlag) {
        this->safetyFlag = (digitalRead(UM_PROS3::ESTOP_PIN) ==
                            LOW) /*|| getPPCCurrent() < threshold*/; // todo
    }
    // inline void updatePowerPositive(bool powerPositive) {
    //     this->powerPositive = powerPositive;
    // }
    inline void setLoadGPIO(uint_fast8_t value) {
        loadDevice.setGPIO((uint32_t)value);
    }

  private:
    MCP23008T &loadDevice;

    bool safetyFlag = false; // todo
    // bool powerPositive = false;
    int_fast16_t currentRPM = 0; // todo
};
