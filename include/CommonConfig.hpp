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
     * @details Second Prompt: hanks, that seems to work perfectly! Could you
     * add support for deduplicating the results? ie: a constexpr variable with
     * the number of unique combinations, a consteval function to compute this,
     * and a constexpr array of the deduplicated values?
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

        // --- helpers for deduplication ---

        template <std::size_t N>
        consteval etl::array<uint_fast16_t, N>
        sorted_copy(const etl::array<uint_fast16_t, N> &input) {
            auto result = input;
            for (std::size_t i = 0; i < N; ++i) {
                for (std::size_t j = i + 1; j < N; ++j) {
                    if (result[j] < result[i]) {
                        std::swap(result[i], result[j]);
                    }
                }
            }
            return result;
        }

        template <std::size_t N>
        consteval std::size_t
        count_unique(const etl::array<uint_fast16_t, N> &input) {
            auto sorted = sorted_copy(input);
            std::size_t count = 0;
            bool first = true;
            uint_fast16_t last = 0;

            for (std::size_t i = 0; i < N; ++i) {
                const auto v = sorted[i];
                if (first || v != last) {
                    ++count;
                    last = v;
                    first = false;
                }
            }
            return count;
        }

        template <std::size_t InN, std::size_t OutN>
        consteval etl::array<uint_fast16_t, OutN>
        unique_sorted(const etl::array<uint_fast16_t, InN> &input) {
            auto sorted = sorted_copy(input);
            etl::array<uint_fast16_t, OutN> out{};
            std::size_t out_i = 0;
            bool first = true;
            uint_fast16_t last = 0;

            for (std::size_t i = 0; i < InN; ++i) {
                const auto v = sorted[i];
                if (first || v != last) {
                    out[out_i++] = v;
                    last = v;
                    first = false;
                }
            }
            return out;
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

    // number of unique combination values
    constexpr std::size_t NUM_UNIQUE_COMBINATIONS =
        detail::count_unique(ALL_COMBINATIONS_mOhms);

    static_assert(NUM_UNIQUE_COMBINATIONS <= NUM_COMBINATIONS,
                  "NUM_UNIQUE_COMBINATIONS must be <= NUM_COMBINATIONS");

    // deduplicated, sorted combination values
    constexpr etl::array<uint_fast16_t, NUM_UNIQUE_COMBINATIONS>
        UNIQUE_COMBINATIONS_mOhms =
            detail::unique_sorted<NUM_COMBINATIONS, NUM_UNIQUE_COMBINATIONS>(
                ALL_COMBINATIONS_mOhms);

    // Pin 6 is connected by default in hardware
    constexpr uint_fast8_t INVERTED_PIN_INDEX = 5;
    constexpr uint8_t INVERTED_PIN_MASK = (1 << INVERTED_PIN_INDEX);
    static_assert(INVERTED_PIN_MASK == 0b0010'0000,
                  "INVERTED_PIN_MASK value incorrect");
    constexpr uint8_t DEFAULT_LOAD_STATE = INVERTED_PIN_MASK;

} // namespace LOAD
