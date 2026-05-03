#pragma once

#include "LoadConfig.hpp"
#include <Adafruit_INA260.h>
#include <atomic>
#include <etl/format_spec.h>
#include <etl/string.h>
#include <etl/to_string.h>

namespace INA260 {
    constexpr uint_fast16_t m_TO_BASE = 1000;
    constexpr uint_fast16_t u_TO_m = 1000;
    constexpr uint_fast32_t u_TO_BASE = m_TO_BASE * u_TO_m;
    static constexpr int_fast8_t MIN_VOLTAGE_V = -32;
    static constexpr int_fast16_t MIN_VOLTAGE_mV =
        MIN_VOLTAGE_V * static_cast<int_fast16_t>(m_TO_BASE);
    static constexpr int_fast8_t MAX_VOLTAGE_V = 32;
    static constexpr int_fast16_t MAX_VOLTAGE_mV = MAX_VOLTAGE_V * m_TO_BASE;
    static constexpr int_fast8_t MIN_CURRENT_A = -15;
    static constexpr int_fast16_t MIN_CURRENT_mA =
        MIN_CURRENT_A * static_cast<int_fast16_t>(m_TO_BASE);
    static constexpr int_fast8_t MAX_CURRENT_A = 15;
    static constexpr int_fast16_t MAX_CURRENT_mA = MAX_CURRENT_A * m_TO_BASE;
    static constexpr int_fast16_t MIN_POWER_W =
        MIN_VOLTAGE_V * MIN_CURRENT_A * -1;
    static constexpr int_fast16_t MIN_POWER_mW =
        MIN_POWER_W * static_cast<int_fast16_t>(m_TO_BASE);
    static constexpr int_fast16_t MAX_POWER_W = MAX_VOLTAGE_V * MAX_CURRENT_A;
    static constexpr int_fast16_t MAX_POWER_mW = MAX_POWER_W * m_TO_BASE;

    static constexpr const char *TAG = "PS";
    Adafruit_INA260 powerSensor;

    std::atomic<int_fast16_t> voltage_mV = 0;
    int_fast16_t prevVoltage_mV = 0;
    std::atomic<int_fast16_t> dVoltage_mVPS = 0;
    std::atomic<int_fast16_t> current_mA = 0;
    int_fast16_t prevCurrent_mA = 0;
    std::atomic<int_fast16_t> dCurrent_mAPS = 0;
    std::atomic<int_fast32_t> power_mW = 0;
    int_fast32_t prevPower_mW = 0;
    std::atomic<int_fast32_t> dPower_mWPS = 0;

    namespace detail {
        unsigned long lastUpdateTime_us = 0;
    }

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

        unsigned long currentTime_us = micros();
        unsigned long dTime_us = currentTime_us - detail::lastUpdateTime_us;

        auto vTemp_mV = static_cast<int_fast32_t>(powerSensor.readBusVoltage());
        if ((vTemp_mV > MAX_VOLTAGE_mV) ||
            (vTemp_mV < PSENSOR::MIN_VOLTAGE_mV)) {
            ESP_LOGE("TAG", "V reading out of bounds: %u mV", vTemp_mV);
            result = false;
        } // Else: Valid reading
        prevVoltage_mV = voltage_mV;
        voltage_mV = vTemp_mV;
        int_fast16_t dVoltage_mV = voltage_mV - prevVoltage_mV;
        dVoltage_mVPS = dVoltage_mV * u_TO_BASE / dTime_us;

        auto iTemp = static_cast<int_fast32_t>(powerSensor.readCurrent());
        if ((iTemp > MAX_CURRENT_mA) || (iTemp < MIN_CURRENT_mA)) {
            ESP_LOGE("TAG", "C reading out of bounds: %d mA", iTemp);
            result &= false;
        } // Else: Valid reading
        prevCurrent_mA = current_mA;
        current_mA = iTemp;
        int_fast16_t dCurrent_mA = current_mA - prevCurrent_mA;
        dCurrent_mAPS = dCurrent_mA * u_TO_BASE / dTime_us;

        auto pTemp = static_cast<int_fast32_t>(powerSensor.readPower());
        if ((pTemp > (MAX_VOLTAGE_mV * MAX_CURRENT_A)) ||
            (pTemp < (PSENSOR::MIN_VOLTAGE_mV * MIN_CURRENT_A))) {
            ESP_LOGE("TAG", "P out of bounds: %d mW", pTemp);
            result &= false;
        } // Else: Valid reading
        prevPower_mW = power_mW;
        power_mW = pTemp;
        int_fast16_t dPower_mW = power_mW - prevPower_mW;
        dPower_mWPS = dPower_mW * u_TO_BASE / dTime_us;

        return result;
    }

    // TODO - improve this and null terminator may not be needed
    static constexpr uint_fast8_t LOG_STRING_SIZE =
        3 + ((6 + 5) * 3) + ((8 + 5) * 3) + 1;
    /**
     * @brief Get at string that describes the current state of the PID instance
     * @returns the current state of the PID instance as a string
     */
    etl::string<LOG_STRING_SIZE> getLogString() {
        etl::string<LOG_STRING_SIZE> logString(TAG); // 3 chars
        (void)logString.append(": mV: ");            // 6 chars

        etl::format_spec decFormatA;
        (void)decFormatA.width(5).fill('0'); // [5 chars]
        /**
         * @details I don't think we need strong guarantees on logging data
         * @see
         * https://stackoverflow.com/questions/12346487/what-do-each-memory-order-mean
         * @see https://en.cppreference.com/cpp/atomic/memory_order
         */
        etl::to_string(voltage_mV.load(), logString, decFormatA,
                       true);             // 5 chars
        (void)logString.append(", mA: "); // 6 chars
        etl::to_string(current_mA.load(), logString, decFormatA,
                       true);             // 5 chars
        (void)logString.append(", mW: "); // 6 chars
        etl::to_string(power_mW.load() / m_TO_BASE, logString, decFormatA,
                       true);               // 5 chars
        (void)logString.append(", mV/s: "); // 8 chars
        etl::to_string(dVoltage_mVPS.load(), logString, decFormatA,
                       true);               // 5 chars
        (void)logString.append(", mA/s: "); // 8 chars
        etl::to_string(dCurrent_mAPS.load(), logString, decFormatA,
                       true);               // 5 chars
        (void)logString.append(", mW/s: "); // 8 chars
        etl::to_string(dPower_mWPS.load(), logString, decFormatA,
                       true); // 5 chars

        return logString;
    }
} // namespace INA260
