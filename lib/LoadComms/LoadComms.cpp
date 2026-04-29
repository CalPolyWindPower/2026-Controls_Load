/**
 * @file LoadComms.cpp
 * @brief ESP-NOW communication module for load box controller.
 *
 * Handles wireless communication between load box and nacelle.
 * Sends state, E-stop, and actuator position data while receiving RPM data.
 */

#include "LoadComms.hpp"
#include <esp_log.h>

// Initialization of static members
QueueHandle_t LoadComms::priorityDataQueue = nullptr;

/**
 * @brief MAC address of the nacelle controller.
 */
const uint8_t NACELLE_MAC[] = {0x30, 0xED, 0xA0, 0xE0, 0x6B, 0x78};

/**
 * @brief Pointer to instance for static callbacks.
 */
static LoadComms *s_instance = nullptr;

LoadComms::LoadComms()
    : lastSendTime_(0),
      lastRxTime_(0),
      linkAlive_(false) //,
      // nacelleRPM_(0.0f) 
      {
  s_instance = this;
}

bool LoadComms::begin() {
  if (!WiFi.mode(WIFI_STA)) {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return false;
    }

    if (!WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY)) {
        ESP_LOGE(TAG, "Failed to set WiFi band mode");
        return false;
    }

    if (WiFi.STA.bandwidth(WIFI_BW_HT20)) {
        ESP_LOGE(TAG, "Failed to set WiFi bandwidth");
        return false;
    }

    if (WiFi.setChannel(wiFiChannel) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi channel");
        return false;
    }

  if (esp_now_init() != ESP_OK) {
    ESP_LOGE(TAG, "ESP-NOW init failed");
    return false;
  }

  if(esp_now_register_send_cb(onDataSent_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register tx cb");
    return false;
  }

  if(esp_now_register_recv_cb(onDataRecv_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register rx cb");
    return false;
  }

  if( setupPeer_() != ESP_OK) {
    // Logging already handled
    return false;
  }

  Serial.println("Loadbox ready");
  return true;
}

esp_err_t LoadComms::setupPeer_() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, NACELLE_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add peer: %d at %02x:%02x:%02x:%02x:%02x:%02x", result,
             NACELLE_MAC[0], NACELLE_MAC[1], NACELLE_MAC[2],
             NACELLE_MAC[3], NACELLE_MAC[4], NACELLE_MAC[5]);
  } else{
    ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
           NACELLE_MAC[0], NACELLE_MAC[1], NACELLE_MAC[2],
           NACELLE_MAC[3], NACELLE_MAC[4], NACELLE_MAC[5]);
  }

  return result;
}

void LoadComms::onDataSent_(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  // (void)tx_info;
  // Serial.print("Send status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  // This is check elsewhere
}

void LoadComms::onDataRecv_(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (s_instance == nullptr) {
    ESP_LOGE(TAG, "Rx CB: Invalid instance");
    return;
  }

  const uint8_t *mac = recv_info->src_addr;

  Serial.printf("Packet received from: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (len == sizeof(NacellePacket)) {
    // memcpy(&s_instance->incomingPacket_, data, sizeof(NacellePacket));

    s_instance->lastRxTime_ = millis();
    s_instance->linkAlive_ = true;

    // s_instance->nacelleRPM_ = s_instance->incomingPacket_.rpm;

    // Serial.println("Received NacellePacket:");
    // printNacellePacket(s_instance->incomingPacket_, Serial);

    (void)xQueueOverwrite(priorityDataQueue, data); // Allegedly cannot fail
  } else {
    ESP_LOGE(TAG, "Rx invalid len: %d", len);
  }
}

bool LoadComms::sendLoadboxData(uint8_t estop) {
  // if (now - lastSendTime_ >= LOAD_COMMS_SEND_PERIOD_MS) {
  makeLoadboxPacket(outgoingPacket_, estop);
  esp_err_t result = esp_now_send(NACELLE_MAC, (uint8_t *)&outgoingPacket_, sizeof(outgoingPacket_));
  if(result == ESP_OK) {
    lastSendTime_ = millis();
    linkAlive_ = true;
  } else {
    linkAlive_ = false;
    ESP_LOGE(TAG, "Tx failed");
  }

  // }
  // if (now - lastRxTime_ > LOAD_COMMS_TIMEOUT_MS) {
  //   linkAlive_ = false;
  // }

  // if (!linkAlive_) {
  //   Serial.println("WARNING: nacelle comms timeout");
  // }
  return linkAlive_;
}

// void LoadComms::process() {
//   // This method can be used for future expansion (e.g., periodic checks)
// }

bool LoadComms::isLinkAlive() const {
  return linkAlive_;
}

// float LoadComms::getNacelleRPM() const {
//   return nacelleRPM_;
// }
