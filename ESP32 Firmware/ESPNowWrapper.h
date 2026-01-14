#pragma once

#include "Arduino.h"
#include <esp_now.h>
#include "WiFi.h"

#include <queue>
#include <mutex>
#include <atomic>

#define RECV_QUEUE_SIZE 10
#define MAXIMUM_MESSAGE_SIZE 1460

struct ESPMessage
{
    uint16_t size;
    char data[MAXIMUM_MESSAGE_SIZE];
};

class ESPNowWrapper
{
private:
    static int lastRSSI;
    static std::atomic<bool> readyToSend;
    static std::queue<ESPMessage> receiveQueue;
    static std::mutex queueMutex;
    friend void dataRecvCB(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len);
    friend void dataSendCB(const wifi_tx_info_t *info, esp_now_send_status_t status);

public:
    esp_err_t init();
    esp_err_t addPeer(uint8_t *macAddress, uint8_t channel);
    esp_err_t send(uint8_t *macAddress, ESPMessage &message);
    esp_err_t send(uint8_t *macAddress, ESPMessage &message, uint16_t length);
    esp_err_t receive(ESPMessage *recvdMessage);
    int getReceiveQueueLength();
    int getRSSI();
};
