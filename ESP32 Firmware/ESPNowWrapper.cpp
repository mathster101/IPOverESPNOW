#include "ESPNowWrapper.h"
#include <cstring>

std::atomic<bool> ESPNowWrapper::readyToSend{true};
std::queue<ESPMessage> ESPNowWrapper::receiveQueue;
std::mutex ESPNowWrapper::queueMutex;
int ESPNowWrapper::lastRSSI = -99;
bool ESPNowWrapper::isBound = false;
uint8_t ESPNowWrapper::boundPeerAddress[6];
static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void printDebug2(const char *dbgMessage)
{
    Serial.printf("<DEBUG>%s", dbgMessage);
    Serial.write('\0');
}

void dataRecvCB(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len)
{
    ESPNowWrapper::lastRSSI = esp_now_info->rx_ctrl->rssi;

    ESPMessage newMessage;
    memcpy(&newMessage, incomingData, len);
    if (strcmp("BROADCAST BEACON", newMessage.data) == 0)
    {
        printDebug2("got a bcast beacon!");
        if (ESPNowWrapper::isBound)
        {
            return;
        }
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, esp_now_info->src_addr, 6);
        peer.channel = esp_now_info->rx_ctrl->channel;
        peer.encrypt = false;
        esp_err_t result = esp_now_add_peer(&peer);
        if (result == ESP_OK)
        {
            Serial.printf("Bound to MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          esp_now_info->src_addr[0], esp_now_info->src_addr[1],
                          esp_now_info->src_addr[2], esp_now_info->src_addr[3],
                          esp_now_info->src_addr[4], esp_now_info->src_addr[5]);
            ESPNowWrapper::isBound = true;
            memcpy(ESPNowWrapper::boundPeerAddress, esp_now_info->src_addr, 6);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(ESPNowWrapper::queueMutex); // unlocks by itself when out of scope
    if (ESPNowWrapper::receiveQueue.size() < RECV_QUEUE_SIZE)
    {
        ESPNowWrapper::receiveQueue.push(newMessage);
    }
    else
        ; //just drop the packet
}

void dataSendCB(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    ESPNowWrapper::readyToSend = true;
}

esp_err_t ESPNowWrapper::init()
{
    WiFi.mode(WIFI_MODE_STA);
    WiFi.setTxPower(WIFI_POWER_21dBm);

    esp_err_t result = esp_now_init();
    if (result != ESP_OK)
        return result;
    result = esp_now_register_recv_cb(dataRecvCB);
    if (result != ESP_OK)
        return result;
    result = esp_now_register_send_cb(dataSendCB);
     if (result != ESP_OK)
        return result;
    result = addPeer(broadcastAddress, 0);
    if (result != ESP_OK)
        return result;
    isBound = false;
    xTaskCreate(
        ESPNowWrapper::broadcastMACBeacon,
        "BeaconTask",
        4096,
        this,
        1,
        &beaconTaskHandle);
    return result;
}

esp_err_t ESPNowWrapper::addPeer(uint8_t *macAddress, uint8_t channel = 0)
{
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, macAddress, 6);
    peer.channel = channel;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

esp_err_t ESPNowWrapper::send(uint8_t *macAddress, ESPMessage &message)
{
    return this->send(macAddress, message, sizeof(ESPMessage));
}

esp_err_t ESPNowWrapper::send(uint8_t *macAddress, ESPMessage &message, uint16_t length)
{
    if (length > sizeof(ESPMessage))
        return ESP_ERR_INVALID_SIZE;

    uint32_t entryTime = millis();
    message.size = length;
    while (!readyToSend && (millis() - entryTime) < 10)
    {
        vTaskDelay(1);
    };
    esp_err_t result = esp_now_send(macAddress, (uint8_t *)&message, length + 2); // 2 bytes for size field
    readyToSend = false;
    return result;
}

esp_err_t ESPNowWrapper::receive(ESPMessage *recvdMessage)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    if (receiveQueue.empty())
    {
        return -1;
    }
    else
    {
        *recvdMessage = receiveQueue.front();
        receiveQueue.pop();
        return ESP_OK;
    }
}

int ESPNowWrapper::getReceiveQueueLength()
{
    return this->receiveQueue.size();
}

int ESPNowWrapper::getRSSI()
{
    return lastRSSI;
}

void ESPNowWrapper::broadcastMACBeacon(void *parameter)
{
    ESPNowWrapper *instance = static_cast<ESPNowWrapper *>(parameter);

    ESPMessage bcastBeacon;
    strcpy(bcastBeacon.data, "BROADCAST BEACON");
    bcastBeacon.size = strlen(bcastBeacon.data) + 1;

    while (true)
    {
        esp_now_send(broadcastAddress, (uint8_t *)&bcastBeacon, bcastBeacon.size + 2);
        printDebug2("sending beacon!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}