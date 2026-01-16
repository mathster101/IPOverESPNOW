#include "ESPNowWrapper.h"
#include <cstring>

std::atomic<bool> ESPNowWrapper::readyToSend{true};
std::queue<ESPMessage> ESPNowWrapper::receiveQueue;
std::mutex ESPNowWrapper::queueMutex;
int ESPNowWrapper::lastRSSI = -99;

void dataRecvCB(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len)
{
    ESPMessage newMessage;
    memcpy(&newMessage, incomingData, len);
    std::lock_guard<std::mutex> lock(ESPNowWrapper::queueMutex); // unlocks by itself when out of scope
    if (ESPNowWrapper::receiveQueue.size() < RECV_QUEUE_SIZE)
    {
        ESPNowWrapper::receiveQueue.push(newMessage);
    }
    else
    {
        //drop the packet
        
        // ESPNowWrapper::receiveQueue.pop();
        // ESPNowWrapper::receiveQueue.push(newMessage);
    }
    ESPNowWrapper::lastRSSI = esp_now_info->rx_ctrl->rssi;
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
