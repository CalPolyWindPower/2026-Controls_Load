#pragma once

static_assert(__cplusplus >= 202302L, "C++23 standard or later required.");

// Imports
#include <cstddef>
#include <cstdint>
#include <etl/array.h>
#include <etl/combinations.h>
#include <etl/vector.h>
#include <utility>

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

namespace LOAD {
    constexpr uint8_t PINS_MASK = 0b0011'1111; // First six pins
    constexpr uint_fast8_t NUM_PINS = 6;
    constexpr uint_fast8_t NUM_COMBINATIONS =
        etl::combinations<NUM_PINS, 0>::value +
        etl::combinations<NUM_PINS, 1>::value +
        etl::combinations<NUM_PINS, 2>::value +
        etl::combinations<NUM_PINS, 3>::value +
        etl::combinations<NUM_PINS, 4>::value +
        etl::combinations<NUM_PINS, 5>::value +
        etl::combinations<NUM_PINS, 6>::value;

    // According to GitHub Copilot, GPT-5.1, Ask mode
    static_assert(NUM_COMBINATIONS == (1u << NUM_PINS),
                  "NUM_COMBINATIONS must be 2^NUM_PINS");

    constexpr etl::array<uint_fast16_t, NUM_PINS> PIN_VALUES_mOhms = {
        500, 1000, 2000, 3000, 5000, 10000}; // in mOhms, in series // TODO
    static_assert(PIN_VALUES_mOhms.size() == NUM_PINS,
                  "PIN_VALUES size mismatch");
    static_assert(PIN_VALUES_mOhms.back() < UINT16_MAX,
                  "PIN_VALUES value too large");

    /**
     * @brief Detail namespace for combination sum calculations
     * @author GitHub Copilot, GPT-5.1, Ask mode
     * @author BobSaidHi
     * @details Prompt: Using C++23 consteval functions and templates, I would
     * like to create an array of all possible combinations of values in
     * PIN_VALUES_mOhms. (included some relevant lines of my own attempts as
     * context)
     */
    namespace detail {
        // Sum of selected pins for a given bitmask
        consteval uint_fast16_t
        sum_for_mask(uint_fast8_t mask,
                     const etl::array<uint_fast16_t, NUM_PINS> &values) {

            uint_fast16_t sum = 0;
            for (std::size_t i = 0; i < NUM_PINS; ++i) {
                if (mask & (1u << i)) {
                    sum += values[i];
                }
            }
            return sum;
        }

        template <std::size_t... Is>
        consteval etl::array<uint_fast16_t, NUM_COMBINATIONS>
        make_all_combinations(const etl::array<uint_fast16_t, NUM_PINS> &values,
                              std::index_sequence<Is...>) {

            return {sum_for_mask(static_cast<uint_fast8_t>(Is), values)...};
        }
    } // namespace detail

    /**
     * @brief get all combinations of values
     * @author GitHub Copilot, GPT-5.1, Ask mode
     * @author BobSaidHi
     */
    consteval etl::array<uint_fast16_t, NUM_COMBINATIONS>
    getAllCombinations(const etl::array<uint_fast16_t, NUM_PINS> &values) {
        return detail::make_all_combinations(
            values, std::make_index_sequence<NUM_COMBINATIONS>{});
    }

    constexpr etl::array<uint_fast16_t, NUM_COMBINATIONS>
        ALL_COMBINATIONS_mOhms = getAllCombinations(PIN_VALUES_mOhms);

    // Pin 6 is connected by default in hardware
    constexpr uint_fast8_t INVERTED_PIN_INDEX = 5;
    constexpr uint8_t INVERTED_PIN_MASK = (1 << INVERTED_PIN_INDEX);
    static_assert(INVERTED_PIN_MASK == 0b0010'0000,
                  "INVERTED_PIN_MASK value incorrect");
    constexpr uint8_t DEFAULT_LOAD_STATE = INVERTED_PIN_MASK;

} // namespace LOAD
