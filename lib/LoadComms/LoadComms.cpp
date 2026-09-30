/**
 * @file LoadComms.cpp
 * @brief ESP-NOW communication module for load box controller.
 *
 * Handles wireless communication between load box and nacelle.
 * Sends state, E-stop, and actuator position data while receiving RPM data.
 */

#include "LoadComms.hpp"
#include <esp_log.h>
#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
#    include <esp_wifi.h>
#elif COMMS_STRATEGY == COMMS_STRATEGY_UHCI
#    include <etl/vector.h>
#endif
#include <etl/array.h>

// Initialization of static members
QueueHandle_t LoadComms::priorityDataQueue = nullptr;
std::atomic<uint_fast32_t> LoadComms::txEvents =
    0; // DONE: check against last years code
std::atomic<uint_fast32_t> LoadComms::bytesSent = 0;
std::atomic<uint_fast32_t> LoadComms::bytesNotSent = 0;
std::atomic<uint_fast32_t> LoadComms::rxEvents = 0;
std::atomic<uint_fast32_t> LoadComms::bytesReceived = 0;

#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
/**
 * @brief MAC address of the nacelle controller.
 */
const uint8_t CPWP_DF2C5_A[] = {0x30, 0xED, 0xA0, 0xE0, 0x6B, 0x78};
// Slightly questionable controller (?)
const uint8_t BSI_DF2C5[] = {0xD0, 0xCF, 0x13, 0xEA, 0x4A, 0x08};

const uint8_t *NACELLE_MAC = CPWP_DF2C5_A;
#endif

/**
 * @brief Pointer to instance for static callbacks.
 */
static LoadComms *s_instance = nullptr;

LoadComms::LoadComms()
    : lastSendTime_(0), lastRxTime_(0), linkAlive_(false) //,
// nacelleRPM_(0.0f)
{
    s_instance = this;
    priorityDataQueue = xQueueCreate(1, sizeof(NacellePacket));
}

bool LoadComms::begin() {
#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
    if (!WiFi.mode(WIFI_STA)) {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return false;
    }

    if (!WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY)) {
        ESP_LOGE(TAG, "Failed to set WiFi band mode");
        return false;
    }

    etl::array<uint8_t, 6> MACAddress = {0};
    // TODO: Conversions between pointers and integer types should not be
    // performed. Consider inspecting the second argument of the
    // 'esp_wifi_get_mac' function call.
    esp_err_t opStatus = esp_wifi_get_mac(WIFI_IF_STA, MACAddress.data());
    if (opStatus != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address: %d", opStatus);
        return false;
    } else {
        ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 MACAddress[0], MACAddress[1], MACAddress[2], MACAddress[3],
                 MACAddress[4], MACAddress[5]);
    }

    // if (!WiFi.STA.bandwidth(WIFI_BW_HT20)) {
    //     ESP_LOGE(TAG, "Failed to set WiFi bandwidth");
    //     return false;
    // }

    if (WiFi.setChannel(wiFiChannel) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi channel");
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed");
        return false;
    }

    if (esp_now_register_send_cb(onDataSent_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register tx cb");
        return false;
    }

    if (esp_now_register_recv_cb(onDataRecv_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register rx cb");
        return false;
    }

    if (setupPeer_() != ESP_OK) {
        // Logging already handled
        return false;
    }
#elif COMMS_STRATEGY == COMMS_STRATEGY_UHCI
    if (!adapterUHCI.begin()) {
        ESP_LOGE(TAG, "Failed to initialize UHCI adapter");
        return false;
    }
#endif

    Serial.println("Loadbox ready");
    return true;
}

#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
esp_err_t LoadComms::setupPeer_() {
    esp_now_peer_info_t peerInfo = {};
    (void)memcpy(peerInfo.peer_addr, NACELLE_MAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add peer: %d at %02x:%02x:%02x:%02x:%02x:%02x",
                 result, NACELLE_MAC[0], NACELLE_MAC[1], NACELLE_MAC[2],
                 NACELLE_MAC[3], NACELLE_MAC[4], NACELLE_MAC[5]);
    } else {
        ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
                 NACELLE_MAC[0], NACELLE_MAC[1], NACELLE_MAC[2], NACELLE_MAC[3],
                 NACELLE_MAC[4], NACELLE_MAC[5]);
    }

    return result;
}
#endif

etl::string<LoadComms::LOG_STRING_SIZE> LoadComms::getLogString() const {
    etl::string<LOG_STRING_SIZE> logString(TAG); // 3 chars
    logString.append(": TxE: ");                 // 7 chars

    etl::format_spec decFormatA;
    (void)decFormatA.width(6).fill(
        '0'); // [6 chars, expecting up to 30 mins * (1/(2ms)) = 900,000 events]
    /**
     * @details I don't think we need strong guarantees on logging data
     * @see
     * https://stackoverflow.com/questions/12346487/what-do-each-memory-order-mean
     * @see https://en.cppreference.com/cpp/atomic/memory_order
     */
    etl::to_string(txEvents.load(std::memory_order_relaxed), logString,
                   decFormatA, true); // 6 chars
    logString.append(", TxBS: ");     // 8 chars

    etl::format_spec decFormatB;
    (void)decFormatB.width(7).fill('0'); // [7 chars]
    etl::to_string(bytesSent.load(std::memory_order_relaxed), logString,
                   decFormatB, true);   // 7 chars
    (void)logString.append(", TxBF: "); // 8 chars
    etl::to_string(bytesNotSent.load(std::memory_order_relaxed), logString,
                   decFormatB, true); // 7 chars

    (void)logString.append(", RxE: "); // 7 chars
    etl::to_string(rxEvents.load(std::memory_order_relaxed), logString,
                   decFormatA, true);  // 6 chars
    (void)logString.append(", RxB: "); // 7 chars
    etl::to_string(bytesReceived.load(std::memory_order_relaxed), logString,
                   decFormatB, true); // 7 chars

    return logString;
}

LoadComms::LogData LoadComms::getLogData() const {
    return LogData{txEvents, bytesSent, bytesNotSent, rxEvents, bytesReceived};
}

#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
void LoadComms::onDataSent_(const wifi_tx_info_t *tx_info,
                            esp_now_send_status_t status) {
    // (void)tx_info;
    // Serial.print("Send status: ");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    // This is check elsewhere
    txEvents++;
    if (status == ESP_NOW_SEND_SUCCESS) {
        bytesSent += tx_info->data_len;
    } else {
        bytesNotSent += tx_info->data_len;
    }
}

void LoadComms::onDataRecv_(const esp_now_recv_info_t *recv_info,
                            const uint8_t *data, int len) {
    rxEvents++;
    bytesReceived += len;

    if (s_instance == nullptr) {
        ESP_LOGE(TAG, "Rx CB: Invalid instance");
        return;
    }

    const uint8_t *mac = recv_info->src_addr;

    // Serial.printf("Packet received from: %02X:%02X:%02X:%02X:%02X:%02X\n",
    //               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

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
#endif

bool LoadComms::sendLoadboxData(int16_t d_mVPS, int16_t current_mA,
                                int16_t dIPS, uint16_t powerIfWholeNum_mW,
                                ESTOP_TYPE_NET safety) {
    // if (now - lastSendTime_ >= LOAD_COMMS_SEND_PERIOD_MS) {
    makeLoadboxPacket(outgoingPacket_, d_mVPS, current_mA, dIPS,
                      powerIfWholeNum_mW, safety);
#if COMMS_STRATEGY == COMMS_STRATEGY_NEW
    esp_err_t result =
        esp_now_send(NACELLE_MAC, reinterpret_cast<uint8_t *>(&outgoingPacket_),
                     sizeof(outgoingPacket_));
    if (result == ESP_OK) {
        lastSendTime_ = millis();
        linkAlive_ = true;
    } else {
        linkAlive_ = false;
        ESP_LOGE(TAG, "Tx fail w/ %d", result);
    }
#elif COMMS_STRATEGY == COMMS_STRATEGY_UHCI
    etl::vector<uint8_t, AdapterUHCI::MAX_DATA_LEN> dataToSend;
    memcpy(dataToSend.data(), &outgoingPacket_, sizeof(NacellePacket));
    adapterUHCI.transmit(dataToSend);
#endif

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

bool LoadComms::isLinkAlive() const { return linkAlive_; }

// float LoadComms::getNacelleRPM() const {
//   return nacelleRPM_;
// }
