#pragma once

// Standard Library Includes
// #include <atomic>
#include <cstdint>

// Project Includes
#include "2026Core/FSMStates.hpp"
#include "LoadConfig.hpp"
#include "LoadContainer.hpp"

/**
 * @brief Class to manage the finite state machine for the load
 */
class LoadFSM {
  public: // MARK: Public
    static constexpr const char *TAG = "LFSM";

    /**
     * @brief Construct a new Load FSM object
     * @param load The LoadContainer object that tracks the overall state
     * of the load
     */
    LoadFSM(LoadContainer &load) : load(load) {
        // if (!currentState.is_lock_free()) {
        //     ESP_LOGE(TAG,
        //              "Atomic operations on uint_fast8_t are not lock-free on
        //              " "this platform.");
        // }
    }
    ~LoadFSM() = default;

    // MARK: Getters
    /**
     * @brief Get the current state of the FSM
     * @return The current state
     */
    inline FSMCommon::States getCurrentState() const { return currentState; }

    // MARK: State Logic
    /**
     * @brief result of an FSM update/ input check
     */
    enum class UPDATE_RESULT : uint_fast8_t {
        NO_CHANGE = 0,
        STATE_CHANGED = 1,
        /**
         * @details Trim -1 to an 8-bit unsigned integer(255), unsigned extend
         * to uint_fast8_t
         */
        ERROR = static_cast<uint_fast8_t>(static_cast<uint8_t>(-1))
    };

    /**
     * @brief Check the inputs and update the FSM state accordingly
     * @return The result of the update, indicating if the state changed or if
     * an error occurred
     */
    UPDATE_RESULT updateState() {
        // ESP_LOGI(TAG, "AWDAWDAWDA");
        // Check safety task / E-Stop conditions
        if ((currentState != FSMCommon::States::sESTOP) &&
            (load.getSafetyFlag() != ESTOP_TYPE_FAST::NONE)) {
            // * -> sESTOP
            currentState = FSMCommon::States::sESTOP;
            // ESP_LOGI(TAG, "ANTIBACKWARDS = %s", antiBackwards);
            // DONE: Signal nacelle to ESTOP (setSafetyFlag)
            vTaskSuspend(load.tAdjustLoad.pxHandle);
            // Don't adjust load

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sESTOP) &&
                   (load.getSafetyFlag() != ESTOP_TYPE_FAST::NONE)) {
                    // INA260::antiBackwards = 0;
            // sESTOP -> sESTOP: Nothing to do
            return UPDATE_RESULT::NO_CHANGE;
        } else {
            // else: ~safetyTask
        }

        // Check reset conditions
        constexpr uint_fast16_t START_RUN_2_RPM = 200; // todo
        if ((currentState != FSMCommon::States::sRST) &&
            (load.getRPM() <=
             static_cast<int_fast16_t>(START_RUN_2_RPM))) { // todo check me!
            // * -> sRST
            currentState = FSMCommon::States::sRST;

            // Signal Nacelle (unset safetyFlag)
            vTaskSuspend(load.tAdjustLoad.pxHandle);
            /**
             * @details None inverted, set to max for easiest cut in
             */
            load.setLoadGPIO(LOAD::PINS_Msk);

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sRST) &&
                   load.getRPM() <=
                       static_cast<int_fast16_t>(START_RUN_2_RPM)) {
            // sRST -> sRST: Nothing to do
            return UPDATE_RESULT::NO_CHANGE;
        } else {
            // else: producingPositivePower or maybe just still starting up
        }
        // todo cycling between 2 and 3
        // Check other transition conditions
        constexpr int_fast16_t MIN_LOAD_VOLTAGE_mV = 10000;
        if ((currentState == FSMCommon::States::sRST) &&
            (load.getRPM() > static_cast<int_fast16_t>(START_RUN_2_RPM))) {
            // sRST -> sStartRun
            currentState = FSMCommon::States::sStartRun;

            // Signal nacelle (set producingPositivePower)
            // Load is already off

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((INA260::voltage_mV >= MIN_LOAD_VOLTAGE_mV) &&
                   (currentState == FSMCommon::States::sStartRun) &&
                   load.isSteadyRPM()) { // FIXME - steady ROM condition
            // sStartRun -> sRunLoad
            // Note: The producing positive power condition is handled by the
            // reset logic
            currentState = FSMCommon::States::sRunLoad;

            // Nacelle can detect this on it's own
            // todo: check load logic
            // todo: better way to signal load task?
            vTaskResume(load.tAdjustLoad.pxHandle);

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sRunLoad) &&
                   load.isTargetRPMExceeded()) {
            // sRunLoad -> sCurtail
            currentState = FSMCommon::States::sCurtail;

            // Nacelle can detect this on it's own
            vTaskSuspend(load.tAdjustLoad.pxHandle);

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sCurtail) &&
                   !load.isTargetRPMExceeded()) {
            // sCurtail -> sRunLoad
            currentState = FSMCommon::States::sRunLoad;

            // Nacelle can detect this on it's own
            vTaskResume(load.tAdjustLoad.pxHandle);

            return UPDATE_RESULT::STATE_CHANGED;
        } else {
            return UPDATE_RESULT::NO_CHANGE;
        }

        return UPDATE_RESULT::ERROR;
    }

    // int antiBackwards = 0;
  private:               // MARK: Private
    LoadContainer &load; // todo switch to by reference

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
    //     static_assert(sizeof(int) == sizeof(uint_fast8_t),
    //                   "Atomic Lock-free check issue");
    // #if (ATOMIC_INT_LOCK_FREE == 0)
    // #    error "Atomic operations on int are not lock-free on this platform."
    // #elif (ATOMIC_INT_LOCK_FREE == 1)
    // #    warning \
//         "Atomic operations on int are only sometimes lock-free on this
    //         platform."
    // #endif

    /**
     * @brief Check if std::atomic<uint_fast8_t> is an acceptable (lock free)
     * solution for shared variables
     * @see https://www.reddit.com/r/embedded/comments/zn23of/comment/j0fav6o/
     * @see
     * https://stackoverflow.com/questions/63471387/should-volatile-still-be-used-for-sharing-data-with-isrs-in-modern-c
     * @see https://en.cppreference.com/w/c/language/atomic.html
     * @see https://en.cppreference.com/w/cpp/atomic/atomic.html
     * @see https://stackoverflow.com/a/16783513
     */
    // static_assert(std::atomic<uint_fast8_t>::is_always_lock_free,
    //               "Atomic operations on uint_fast8_t are not lock-free on "
    //               "this platform.");

    /**
     * @details Store the FSM state as an atomic variable such that it can be
     * safely accessed from multiple tasks
     * @details Actually, this will not be used for critical IPC, unlike in the
     * nacelle.  `std::atomic` isn't guaranteed to be lock free anyways.
     * @see
     * https://stackoverflow.com/questions/21756457/how-can-i-create-an-atomic-enum-in-c
     * @see
     * https://stackoverflow.com/questions/31978324/what-exactly-is-stdatomic
     * @see https://en.cppreference.com/w/cpp/atomic/atomic.html
     */
    // std::atomic<FSMCommon::States> currentState = FSMCommon::States::sINIT;
    FSMCommon::States currentState = FSMCommon::States::sINIT;
};
