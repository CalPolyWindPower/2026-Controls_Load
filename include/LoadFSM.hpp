#pragma once

// Standard Library Includes
#include <atomic>
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
        UPDATE_RESULT result = updateState();
        if (result == UPDATE_RESULT::ERROR) {
            ESP_LOGE(TAG, "Error during FSM init., %d", (uint_fast8_t)result);
        } else if (result == UPDATE_RESULT::STATE_CHANGED) {
            ESP_LOGI(TAG, "Initialized FSM to state %d",
                     (uint_fast8_t)currentState);
        } else if (result == UPDATE_RESULT::NO_CHANGE) {
            ESP_LOGE(TAG, "Failed to enter a valid state");
        } else {
            ESP_LOGE(TAG, "Unknown FSM init. result: %d", (uint_fast8_t)result);
        }
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
        ERROR = (uint_fast8_t)(uint8_t)-1
    };

    /**
     * @brief Check the inputs and update the FSM state accordingly
     * @return The result of the update, indicating if the state changed or if
     * an error occurred
     */
    UPDATE_RESULT updateState() {
        // Check safety task / E-Stop conditions
        if ((currentState != FSMCommon::States::sESTOP) &&
            load.getSafetyFlag()) {
            // * -> sESTOP
            currentState = FSMCommon::States::sESTOP;

            // TODO: Signal nacelle to ESTOP (setSafetyFlag)
            vTaskSuspend(load.mainTaskDescriptions
                             [LoadContainer::TASK_IDS::TID_ADJUST_LOAD]
                                 .pxHandle);
            load.setLoadGPIO(0b0000'0000); // todo: one inverted?

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sESTOP) &&
                   load.getSafetyFlag()) {
            // Nothing to do
            return UPDATE_RESULT::NO_CHANGE;
        } // else: ~safetyTask

        // Check reset conditions
        if ((currentState != FSMCommon::States::sRST) &&
            !load.isPowerPositive()) {
            // * -> sRST
            currentState = FSMCommon::States::sRST;

            // Signal Nacelle (unset safetyFlag)
            vTaskSuspend(load.mainTaskDescriptions
                             [LoadContainer::TASK_IDS::TID_ADJUST_LOAD]
                                 .pxHandle);
            load.setLoadGPIO(0b0000'0000); // todo: one inverted?

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sRST) &&
                   !load.isPowerPositive()) {
            // Nothing to do
            return UPDATE_RESULT::NO_CHANGE;
        } // else: producingPositivePower

        // Check other transition conditions
        if (currentState == FSMCommon::States::sRST) {
            // sRST -> sStartLoad
            currentState = FSMCommon::States::sStartLoad;

            // Signal nacelle (set producingPositivePower)
            // Load is already off

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sStartLoad) &&
                   load.isSteadyRPM()) {
            // sStartLoad -> sRunLoad
            // Note: The producing positive power condition is handled by the
            // reset logic
            currentState = FSMCommon::States::sRunLoad;

            // Nacelle can detect this on it's own
            vTaskResume( // todo: check load logic
                load.mainTaskDescriptions
                    [LoadContainer::TASK_IDS::TID_ADJUST_LOAD]
                        .pxHandle); // todo: better way to signal load task?

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sRunLoad) &&
                   load.isTargetRPMExceeded()) {
            // sRunLoad -> sCurtail
            currentState = FSMCommon::States::sCurtail;

            // Nacelle can detect this on it's own
            // Load is already on

            return UPDATE_RESULT::STATE_CHANGED;
        } else if ((currentState == FSMCommon::States::sCurtail) &&
                   !load.isTargetRPMExceeded()) {
            // sCurtail -> sRunLoad
            currentState = FSMCommon::States::sRunLoad;

            // Nacelle can detect this on it's own
            // Load is already on

            return UPDATE_RESULT::STATE_CHANGED;
        } else {
            return UPDATE_RESULT::NO_CHANGE;
        }

        return UPDATE_RESULT::ERROR;
    }

  private: // MARK: Private
    LoadContainer load;

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
