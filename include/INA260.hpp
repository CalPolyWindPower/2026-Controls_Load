#pragma once

#include "LoadConfig.hpp"
#include <Adafruit_INA260.h>
#include <atomic>
#include <etl/format_spec.h>
#include <etl/string.h>
#include <etl/to_string.h>

namespace INA260 {
    constexpr uint_fast16_t m_TO_BASE = 1000;
    static constexpr int_fast8_t MIN_VOLTAGE_V = -32;
    static constexpr int_fast16_t MIN_VOLTAGE_mV = MIN_VOLTAGE_V * m_TO_BASE;
    static constexpr int_fast8_t MAX_VOLTAGE_V = 32;
    static constexpr int_fast16_t MAX_VOLTAGE_mV = MAX_VOLTAGE_V * m_TO_BASE;
    static constexpr int_fast8_t MIN_CURRENT_A = -15;
    static constexpr int_fast16_t MIN_CURRENT_mA = MIN_CURRENT_A * m_TO_BASE;
    static constexpr int_fast8_t MAX_CURRENT_A = 15;
    static constexpr int_fast16_t MAX_CURRENT_mA = MAX_CURRENT_A * m_TO_BASE;
    static constexpr int_fast16_t MIN_POWER_W =
        MIN_VOLTAGE_V * MIN_CURRENT_A * -1;
    static constexpr int_fast16_t MIN_POWER_mW = MIN_POWER_W * m_TO_BASE;
    static constexpr int_fast16_t MAX_POWER_W = MAX_VOLTAGE_V * MAX_CURRENT_A;
    static constexpr int_fast16_t MAX_POWER_mW = MAX_POWER_W * m_TO_BASE;

    static constexpr const char *TAG = "PS";
    Adafruit_INA260 powerSensor;

    std::atomic<int_fast16_t> voltage_mV = 0;
    std::atomic<int_fast16_t> current_mA = 0;
    std::atomic<int_fast32_t> power_mW = 0;

    bool begin(uint8_t i2c_addr, INA260_AveragingCount avgCount,
               INA260_ConversionTime convTime) {
        if (!powerSensor.begin(i2c_addr)) {
            ESP_LOGE("TAG", "Failed to initialize INA260 at 0x%02X", i2c_addr);
            return false;
        }
        powerSensor.setAveragingCount(avgCount);
        powerSensor.setVoltageConversionTime(convTime);

        return true;
    }

    /**
     * @brief Update the sensor readings and check if they are within the
     * defined bounds.
     * @return Whether the readings are valid (within bounds)
     */
    bool updateReadings() {
        bool result = true;

        auto vTemp_mV = (uint_fast32_t)powerSensor.readBusVoltage();
        if ((vTemp_mV > MAX_VOLTAGE_mV) ||
            (vTemp_mV < PSENSOR::MIN_VOLTAGE_mV)) {
            ESP_LOGE("TAG", "V reading out of bounds: %u mV", vTemp_mV);
            result &= false;
        } // Else: Valid reading
        voltage_mV = vTemp_mV;

        auto iTemp = (uint_fast32_t)powerSensor.readCurrent();
        if ((iTemp > MAX_CURRENT_mA) || (iTemp < MIN_CURRENT_mA)) {
            ESP_LOGE("TAG", "C reading out of bounds: %d mA", iTemp);
            result &= false;
        } // Else: Valid reading
        current_mA = iTemp;

        auto pTemp = (uint_fast32_t)powerSensor.readPower();
        if ((pTemp > (MAX_VOLTAGE_mV * MAX_CURRENT_A)) ||
            (pTemp < (PSENSOR::MIN_VOLTAGE_mV * MIN_CURRENT_A))) {
            ESP_LOGE("TAG", "P out of bounds: %d mW", pTemp);
            result &= false;
        } // Else: Valid reading
        power_mW = pTemp;

        return result;
    }

    // TODO - improve this and null terminator may not be needed
    static constexpr uint_fast8_t LOG_STRING_SIZE =
        3 + 6 + 5 + 6 + 5 + 5 + 5 + 1;
    /**
     * @brief Get at string that describes the current state of the PID instance
     * @returns the current state of the PID instance as a string
     */
    etl::string<LOG_STRING_SIZE> getLogString() {
        etl::string<LOG_STRING_SIZE> logString(TAG); // 3 chars
        logString.append(": mV: ");                  // 6 chars

        etl::format_spec decFormatA;
        decFormatA.width(5).fill('0'); // [5 chars]
        /**
         * @details I don't think we need strong guarantees on logging data
         * @see
         * https://stackoverflow.com/questions/12346487/what-do-each-memory-order-mean
         * @see https://en.cppreference.com/cpp/atomic/memory_order
         */
        etl::to_string(voltage_mV.load(std::memory_order::relaxed), logString,
                       decFormatA, true); // 5 chars
        logString.append(", mA: ");       // 6 chars
        etl::to_string(current_mA.load(std::memory_order::relaxed), logString,
                       decFormatA, true); // 5 chars
        logString.append(", W: ");        // 5 chars
        etl::to_string(power_mW.load(std::memory_order::relaxed) / m_TO_BASE,
                       logString, decFormatA, true); // 5 chars

        return logString;
    }
} // namespace INA260
