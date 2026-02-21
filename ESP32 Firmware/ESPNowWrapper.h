#pragma once

#include "Arduino.h"
#include <esp_now.h>
#include "WiFi.h"


#define MAXIMUM_MESSAGE_SIZE 1470 - 2

struct ESPMessage
{
    uint16_t size;
    char data[MAXIMUM_MESSAGE_SIZE];
};

class ESPNowWrapper
{
private:
    static uint8_t boundPeerAddress[6];

public:
    esp_err_t init();
    esp_err_t send(uint8_t *macAddress, ESPMessage &message, uint16_t length);
    esp_err_t sendWithRetries(uint8_t *macAddress, ESPMessage &message, uint16_t length);
    esp_err_t receive(ESPMessage *recvdMessage);
    bool areRadioRecvPacketsAvailable();
    int getRSSI();
    ////////////////////////////////
    static uint8_t *getBoundPeerAddress();
    static void setBoundPeerAddress(const uint8_t *address);
};