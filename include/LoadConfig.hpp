/**
 * @file LoadConfig.hpp
 */
#pragma once

static_assert(__cplusplus >= 202302L, "C++23 standard or later required.");

// Imports
#include <Adafruit_INA260.h>
#include <cstddef>
#include <cstdint>
#include <etl/array.h>
#include <etl/combinations.h>
#include <etl/vector.h>
#include <utility>

/**
 * @brief Debugging Setup
 * @see
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
 */
#pragma region Debugging Setup

#define PROJECT_ID "WT26L" // CONFIG - Project ID to use with logger

#pragma endregion // Debugging Setup

// MARK: Boards
// Make sure the hardware pins are imported
#include <pins_arduino.h>
#define ESP32S3 2
#define BOARD ESP32S3
#if not(BOARD == ESP32S3)
#    warning "Not using production load board!"
#endif

namespace UM_PROS3 {
    // Onboard LED
    constexpr uint_fast8_t LED_DATA_PIN = RGB_DATA;

    // ESTOP
    constexpr uint_fast8_t ESTOP_PIN = 5;

    // I2C
    constexpr uint_fast8_t I2C_SDA_PIN = SDA;
    constexpr uint_fast8_t I2C_SCL_PIN = SCL;

    // SPI
    constexpr uint_fast8_t SPI_CIPO_PIN = MISO; // todo
    constexpr uint_fast8_t SPI_CLK_PIN = SCK;   // todo
    constexpr uint_fast8_t SPI_COPI_PIN = MOSI; // todo
    constexpr uint_fast8_t SPI_CS_PIN = SS;     // todo

    // UART
    constexpr uint_fast8_t UART_TX_PIN = TX;
    constexpr uint_fast8_t UART_RX_PIN = RX;

    // JTAG
    constexpr uint_fast8_t JTAG_MTCK_PIN = 39;
    constexpr uint_fast8_t JTAG_MTDO_PIN = 40;
    constexpr uint_fast8_t JTAG_MTDI_PIN = 41;
    constexpr uint_fast8_t JTAG_MTMS_PIN = 42;
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

// MARK: Run
namespace RUN {
    // Task Execution Intervals
    enum TASK_INTERVALS : uint32_t {
        TI_FSM_mS = 100,        // CONFIG - 100 ms (10 Hz)
        TI_POLL_SENSORS_mS = 2, // CONFIG - 2 ms (500 Hz)
        TI_ADJUST_LOAD_mS = 10, // CONFIG - 10 ms (100 Hz)
        TI_RECV_ms = 100,       // CONFIG - 100 ms (10 Hz)
        TI_SEND_ms = 100,        // CONFIG - 10 ms (100 Hz) // FIXME
        TI_CFG_ms = 1000,       // CONFIG - 1000 ms (1 Hz)
        TI_TELNET_ms = 500,     // CONFIG - 500 ms (2 Hz)
        TI_OTA_ms = 1000,       // CONFIG - 1000 ms (1 Hz)
        TI_LOG_DATA_ms = 4000,  // CONFIG - 4000 ms (0.25 Hz)
        FAIL_BACKOFF_BASE_FACTOR =
            10, // CONFIG - Multiplier for task delay on failure
        FAIL_BACKOFF_MULTIPLIER =
            2 // CONFIG - Multiplier for backoff factor on subsequent failures
        // TODO: Consider one or two fixed, precalculated delays instead or add max backoff
    };

    // todo: What was this for?
    // constexpr uint32_t SLEEP_TIME_MINS = 10;
    // constexpr uint32_t SLEEP_TIME_SECS = SLEEP_TIME_MINS *
    // CONSTS::SECS_PER_MIN; constexpr uint32_t SLEEP_TIME_MILLIS =
    //     SLEEP_TIME_SECS * CONSTS::MILLIS_PER_SEC;
} // namespace RUN

// MARK: PSENSOR
namespace PSENSOR {
    constexpr uint8_t I2C_ADDRESS = 0x41; // 7-bit address for Adafruit INA260
    // CONFIG - Averaging count for sensor readings
    constexpr INA260_AveragingCount AVG_COUNT = INA260_COUNT_1;     // CONFIG
    constexpr INA260_ConversionTime CONV_TIME = INA260_TIME_588_us; // CONFIG

    constexpr uint_fast16_t m_TO_BASE = 1000;

    constexpr int16_t MIN_VOLTAGE_V = -1; // CONFIG
    // CONFIG
    constexpr int_fast16_t MIN_VOLTAGE_mV = MIN_VOLTAGE_V * m_TO_BASE;
    constexpr int_fast16_t MAX_VOLTAGE_V = 48; // CONFIG
    // CONFIG
    constexpr uint_fast16_t MAX_VOLTAGE_mV = MAX_VOLTAGE_V * m_TO_BASE;
    constexpr int_fast16_t MIN_CURRENT_A = -20;                        // CONFIG
    constexpr int_fast16_t MIN_CURRENT_mA = MIN_CURRENT_A * m_TO_BASE; // CONFIG
    constexpr int_fast16_t MAX_CURRENT_A = 20;                         // CONFIG
    constexpr int_fast16_t MAX_CURRENT_mA = MAX_CURRENT_A * m_TO_BASE; // CONFIG
    // CONFIG
    constexpr int_fast32_t MIN_POWER_mW = MIN_VOLTAGE_mV * MIN_CURRENT_mA;
    // CONFIG
    constexpr int_fast32_t MAX_POWER_mW = MAX_VOLTAGE_mV * MAX_CURRENT_mA;
} // namespace PSENSOR

// MARK: Load
namespace LOAD {
    constexpr uint8_t I2C_ADDRESS = 0x4E; // A0-A2 all high

    /**
     * @brief Bitmask for the load control pins.
     * @details Only the first 6 pins (0-5) are used for load control.
     */
    constexpr uint8_t PINS_Msk = 0b0011'1111;

    /**
     * @brief Number of load pins available.
     */
    constexpr uint_fast8_t NUM_PINS = 6;
    static_assert(NUM_PINS <= 8, "We only have 8 pins available");

    /**
     * @brief Total number of possible pin combinations (2^NUM_PINS).
     *
     * @details Computed using etl::combinations
     */
    constexpr size_t NUM_COMBINATIONS_sizeT =
        etl::combinations<NUM_PINS, 0>::value +
        etl::combinations<NUM_PINS, 1>::value +
        etl::combinations<NUM_PINS, 2>::value +
        etl::combinations<NUM_PINS, 3>::value +
        etl::combinations<NUM_PINS, 4>::value +
        etl::combinations<NUM_PINS, 5>::value +
        etl::combinations<NUM_PINS, 6>::value;
    constexpr uint_fast8_t NUM_COMBINATIONS =
        static_cast<uint_fast8_t>(NUM_COMBINATIONS_sizeT);

    static_assert(UINT8_MAX >= NUM_COMBINATIONS_sizeT,
                  "NUM_COMBINATIONS value too large for uint8_t");

    // According to GitHub Copilot, GPT-5.1, Ask mode
    static_assert(NUM_COMBINATIONS == (1u << NUM_PINS),
                  "NUM_COMBINATIONS must be 2^NUM_PINS");

    /**
     * @brief Resistor values, in milli‑ohms, corresponding to each pin.  Note
     * that the resistors will be connected in series, so the total resistance
     * is the sum of the selected resistors.
     */
    constexpr etl::array<uint_fast16_t, NUM_PINS> PIN_VALUES_mOhms = {
        500, 1000, 2000, 2200, 5000, 10000}; // in mOhms, in series // TODO
    static_assert(PIN_VALUES_mOhms.size() == NUM_PINS,
                  "PIN_VALUES size mismatch");
    static_assert(PIN_VALUES_mOhms.back() < UINT16_MAX,
                  "PIN_VALUES value too large");

    /**
     * MARK: detail
     * @brief Detail namespace for combination sum calculations
     * @details Contains compile‑time helpers used to build and deduplicate the
     * combination sums derived from PIN_VALUES_mOhms.
     *
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
        /**
         * @brief Compute the sum of selected pins for a given bitmask.
         *
         * @param mask Bitmask over NUM_PINS
         * @param values Per‑pin resistance values.
         * @returns Sum of selected values in milli‑ohms.
         */
        consteval uint_fast16_t
        sumForMask(uint_fast8_t mask,
                   const etl::array<uint_fast16_t, NUM_PINS> &values) {

            uint_fast16_t sum = 0;
            for (std::size_t i = 0; i < NUM_PINS; ++i) {
                if (mask & (1u << i)) {
                    sum += values[i];
                }
            }
            return sum;
        }

        /**
         * @brief Build the array of all combination sums for the given values.
         *
         * @tparam Sequence of indices [0, NUM_COMBINATIONS).
         * @param values array of per‑pin resistance values.
         * @returns Array where index i holds the sum for bitmask i.
         */
        template <std::size_t... Is>
        consteval etl::array<uint_fast16_t, NUM_COMBINATIONS>
        makeAllCombinations(const etl::array<uint_fast16_t, NUM_PINS> &values,
                            std::index_sequence<Is...>) {
            return {sumForMask(static_cast<uint_fast8_t>(Is), values)...};
        }

        /* Helpers for deduplication */

        /**
         * @brief Return a sorted copy of the input array (ascending).
         *
         * @details Implements a "simple" O(N^2) sort that is usable in
         * consteval context.
         *
         * @tparam N Number of elements.
         * @param input Input array.
         * @returns Sorted copy of @p input.
         */
        template <std::size_t N>
        consteval etl::array<uint_fast16_t, N>
        makeSortedCopy(const etl::array<uint_fast16_t, N> &input) {
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

        /**
         * @brief Count the number of unique values in the input array.
         *
         * @tparam N Number of elements.
         * @param input Input array (not required to be sorted).
         * @returns Number of distinct values in @p input.
         */
        template <std::size_t N>
        consteval std::size_t
        countUnique(const etl::array<uint_fast16_t, N> &input) {
            auto sorted = makeSortedCopy(input);
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

        /**
         * @brief Create a sorted array containing only the unique values.
         *
         * @tparam InN Size of the input array.
         * @tparam OutN Size of the output array (must equal countUnique()).
         * @param input Input array (not required to be sorted).
         * @returns Sorted array of unique values from @p input.
         */
        template <std::size_t InN, std::size_t OutN>
        consteval etl::array<uint_fast16_t, OutN>
        deduplicateSortedArray(const etl::array<uint_fast16_t, InN> &input) {
            auto sorted = makeSortedCopy(input);
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
     * @brief Compute all possible combination sums for the given pin values.
     *
     * @param values Per‑pin resistance values.
     * @returns Array of size NUM_COMBINATIONS with sum for each bitmask.
     */
    consteval etl::array<uint_fast16_t, NUM_COMBINATIONS>
    getAllCombinations(const etl::array<uint_fast16_t, NUM_PINS> &values) {
        return detail::makeAllCombinations(
            values, std::make_index_sequence<NUM_COMBINATIONS>{});
    }

    /**
     * @brief All combination sums derived from PIN_VALUES_mOhms.
     *
     * Index i corresponds to bitmask i over the NUM_PINS pins.
     */
    constexpr etl::array<uint_fast16_t, NUM_COMBINATIONS>
        ALL_COMBINATIONS_mOhms = getAllCombinations(PIN_VALUES_mOhms);

    /**
     * @brief Number of unique combination sums in ALL_COMBINATIONS_mOhms.
     */
    constexpr uint_fast8_t NUM_UNIQUE_COMBINATIONS =
        detail::countUnique(ALL_COMBINATIONS_mOhms);

    static_assert(NUM_UNIQUE_COMBINATIONS <= NUM_COMBINATIONS,
                  "NUM_UNIQUE_COMBINATIONS must be <= NUM_COMBINATIONS");

    /**
     * @brief Sorted array of unique combination sums (deduplicated).
     */
    constexpr etl::array<uint_fast16_t, NUM_UNIQUE_COMBINATIONS>
        UNIQUE_COMBINATIONS_mOhms =
            detail::deduplicateSortedArray<NUM_COMBINATIONS,
                                           NUM_UNIQUE_COMBINATIONS>(
                ALL_COMBINATIONS_mOhms);
    static_assert(NUM_UNIQUE_COMBINATIONS > 0,
                  "There must be at least one unique combination");
    static_assert(UNIQUE_COMBINATIONS_mOhms[0] == 0,
                  "Zero-resistance combination (mask 0) must be present "
                  "and the minimum value");

    /**
     * @brief Index of the pin that is connected (logical 1) by default.
     *
     * Used to build the default load state mask.
     */
    constexpr uint_fast8_t INVERTED_PIN_INDEX = 5;

    /**
     * @brief Bitmask for the default‑connected pin.
     */
    constexpr uint8_t INVERTED_PIN_MASK = (1 << INVERTED_PIN_INDEX);
    static_assert(INVERTED_PIN_MASK == 0b0010'0000,
                  "INVERTED_PIN_MASK value incorrect");

    /**
     * @brief Default load state bitmask applied at startup.
     */
    constexpr uint8_t DEFAULT_LOAD_STATE = INVERTED_PIN_MASK;

} // namespace LOAD
