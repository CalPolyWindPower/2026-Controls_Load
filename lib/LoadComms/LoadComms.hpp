/**
 * @file LoadComms.h
 * @brief ESP-NOW communication module for load box controller.
 *
 * Handles wireless communication between load box and nacelle.
 * Sends state, E-stop, and actuator position data while receiving RPM data.
 */
/**
 * @file LoadComms.h
 * @brief ESP-NOW communication module for load box controller.
 *
 * Handles wireless communication between load box and nacelle.
 * Sends state, E-stop, and actuator position data while receiving RPM data.
 */

#ifndef LOAD_COMMS_HPP
#define LOAD_COMMS_HPP

#include <2026Core/TurbinePacket/TurbinePacket.hpp>
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cstdint>
#include <esp_now.h>
#include <etl/format_spec.h>
#include <etl/string.h>
#include <etl/to_string.h>

/**
 * @brief MAC address of the nacelle controller.
 */
extern const uint8_t *NACELLE_MAC;

/**
 * @brief Communication timeout threshold in milliseconds.
 */
// const unsigned long LOAD_COMMS_TIMEOUT_MS = 1500;

/**
 * @brief Transmission period in milliseconds.
 */
// const unsigned long LOAD_COMMS_SEND_PERIOD_MS = 100;

/**
 * @class LoadComms
 * @brief ESP-NOW communication handler for load box controller.
 */
class LoadComms {
  public:
    static constexpr char *TAG = "LCO";
    static constexpr uint8_t wiFiChannel = 6;

    static QueueHandle_t priorityDataQueue;

    /**
     * @brief Construct a new LoadComms object.
     */
    LoadComms();

    /**
     * @brief Initialize ESP-NOW communication.
     * @return true if initialization successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Send load box data to nacelle.
     * @param safety Safety value to send.
     */
    bool sendLoadboxData(int16_t d_mVPS, int16_t current_mA, int16_t dIPS,
                         uint16_t powerIfWholeNum_mW, ESTOP_TYPE_NET safety);

    /**
     * @brief Process communication - call in main loop.
     *        Handles periodic sending and link health monitoring.
     * @deprecated WIll call sendLoadboxData directly from main
     */
    // void process();

    /**
     * @brief Check if communication link is active.
     * @return true if link is alive, false otherwise.
     */
    bool isLinkAlive() const;

    /**
     * @brief Get the latest received RPM from nacelle.
     * @return Current RPM value.
     * @deprecated Giving deferred processing in main a try
     */
    // float getNacelleRPM() const;

    // TODO - improve this and null terminator may not be needed
    static constexpr uint_fast8_t LOG_STRING_SIZE =
        3 + 7 + 6 + ((8 + 7) * 2) + 7 + 6 + 7 + 7 + 1;
    /**
     * @brief Get at string that describes the current state of the PID instance
     * @returns the current state of the PID instance as a string
     */
    etl::string<LOG_STRING_SIZE> getLogString() const;

    struct LogData {
        uint_fast32_t txEvents;
        uint_fast32_t bytesSent;
        uint_fast32_t bytesNotSent;
        uint_fast32_t rxEvents;
        uint_fast32_t bytesReceived;
    };

    LogData getLogData() const;

  private:
    NacellePacket incomingPacket_ = {0}; ///< Received packet from nacelle.
    LoadboxPacket outgoingPacket_ = {0}; ///< Outgoing packet to send.
    unsigned long lastSendTime_;         ///< Timestamp of last transmission.
    unsigned long lastRxTime_;           ///< Timestamp of last received packet.
    static std::atomic<uint_fast32_t>
        txEvents; // DONE: check against last years code
    static std::atomic<uint_fast32_t> bytesSent;
    static std::atomic<uint_fast32_t> bytesNotSent;
    static std::atomic<uint_fast32_t> rxEvents;
    static std::atomic<uint_fast32_t> bytesReceived;
    bool linkAlive_; ///< Link health status.
    // float nacelleRPM_;                     ///< Cached RPM value.

    /**
     * @brief Configure ESP-NOW peer.
     * @returns ESP_OK if peer setup successful, error code otherwise.
     */
    esp_err_t setupPeer_();

    /**
     * @brief Callback executed after data is sent.
     * @param tx_info Transmission metadata.
     * @param status Transmission result.
     */
    static void onDataSent_(const wifi_tx_info_t *tx_info,
                            esp_now_send_status_t status);

    /**
     * @brief Callback executed when data is received.
     * @param recv_info Receive metadata including sender MAC.
     * @param data Pointer to received data buffer.
     * @param len Length of received data.
     */
    static void onDataRecv_(const esp_now_recv_info_t *recv_info,
                            const uint8_t *data, int len);
};

#endif // LOAD_COMMS_HPP
