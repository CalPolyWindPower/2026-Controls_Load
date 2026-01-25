#pragma once

static_assert(__cplusplus >= 202302L,
              "C++23 standard or later required.");

// Imports
#include <cstdint>

// CommonConfig.hpp

// MARK: Boards
namespace UM_PROS3 {
    constexpr uint_fast8_t LED_DATA_PIN = 18;
} // namespace UM_PROS3

// MARK: Constants
namespace CONSTS {
    constexpr uint32_t MILLIS_PER_SEC = 1000;
    constexpr uint32_t SECS_PER_MIN = 60;
} // namespace CONSTS

// Mark: Application
namespace LED {
    /**
     * @see
     * https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6#5.2%20LED%20Blinking
     */
    constexpr uint_fast8_t LED_DATA_PIN = UM_PROS3::LED_DATA_PIN;
    constexpr uint32_t BLINK_OFF_SECS = 3;
    constexpr uint32_t BLINK_OFF_MILLIS =
        BLINK_OFF_SECS * CONSTS::MILLIS_PER_SEC;
    // constexpr uint32_t LED_BLINK_ON_SEC = 1;
    constexpr uint32_t BLINK_ON_MILLIS = CONSTS::MILLIS_PER_SEC / 3;

} // namespace LED

namespace RUN {
    constexpr uint32_t SLEEP_TIME_MINS = 10;
    constexpr uint32_t SLEEP_TIME_SECS = SLEEP_TIME_MINS * CONSTS::SECS_PER_MIN;
    constexpr uint32_t SLEEP_TIME_MILLIS =
        SLEEP_TIME_SECS * CONSTS::MILLIS_PER_SEC;
} // namespace RUN