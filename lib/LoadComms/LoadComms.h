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

#ifndef LOAD_COMMS_H
#define LOAD_COMMS_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <TurbinePacket.h>

/**
 * @brief MAC address of the nacelle controller.
 */
extern const uint8_t NACELLE_MAC[];

/**
 * @brief Communication timeout threshold in milliseconds.
 */
const unsigned long LOAD_COMMS_TIMEOUT_MS = 1500;

/**
 * @brief Transmission period in milliseconds.
 */
const unsigned long LOAD_COMMS_SEND_PERIOD_MS = 100;

/**
 * @class LoadComms
 * @brief ESP-NOW communication handler for load box controller.
 */
class LoadComms {
public:
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
   * @param state State value to send.
   * @param estop E-stop value to send.
   * @param actuatorPos Actuator position to send.
   */
  void sendLoadboxData(int8_t state, int8_t estop, int16_t actuatorPos);

  /**
   * @brief Process communication - call in main loop.
   *        Handles periodic sending and link health monitoring.
   */
  void process();

  /**
   * @brief Check if communication link is active.
   * @return true if link is alive, false otherwise.
   */
  bool isLinkAlive() const;

  /**
   * @brief Get the latest received RPM from nacelle.
   * @return Current RPM value.
   */
  float getNacelleRPM() const;

private:
  NacellePacket incomingPacket_;  ///< Received packet from nacelle.
  LoadboxPacket outgoingPacket_;   ///< Outgoing packet to send.
  unsigned long lastSendTime_;    ///< Timestamp of last transmission.
  unsigned long lastRxTime_;      ///< Timestamp of last received packet.
  bool linkAlive_;                ///< Link health status.
  float nacelleRPM_;                     ///< Cached RPM value.

  /**
   * @brief Configure ESP-NOW peer.
   */
  void setupPeer_();

  /**
   * @brief Callback executed after data is sent.
   * @param tx_info Transmission metadata.
   * @param status Transmission result.
   */
  static void onDataSent_(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);

  /**
   * @brief Callback executed when data is received.
   * @param recv_info Receive metadata including sender MAC.
   * @param data Pointer to received data buffer.
   * @param len Length of received data.
   */
  static void onDataRecv_(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
};

#endif // LOAD_COMMS_H
