#include "ESPNowWrapper.h"
#include <cstring>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdio>
#define ENABLE_LED
#ifdef ENABLE_LED
#include <FastLED.h>
#define LED_DATA_PIN 48
#define NUM_LEDS 1
static CRGB leds[NUM_LEDS];
#endif

#define RECV_QUEUE_SIZE 5
#define MAX_RETRIES 5

uint8_t ESPNowWrapper::boundPeerAddress[6];

static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static std::queue<ESPMessage> receiveQueue;
static std::mutex queueMutex;
static int lastRSSI;
static std::atomic<bool> readyToSend{true};
static TaskHandle_t beaconTaskHandle;
static TaskHandle_t ledFlashTaskHandle;
static std::atomic<bool> isBound{false};

static void broadcastMACBeacon(void *parameter);
static void ledFlashTask(void *parameter);

void dataRecvCB(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len)
{
    lastRSSI = esp_now_info->rx_ctrl->rssi;

    ESPMessage newMessage;
    memcpy(&newMessage, incomingData, len);
    if (strncmp("BROADCAST BEACON", newMessage.data, 15) == 0)
    {
        int peerRSSI;
        if (sscanf(newMessage.data, "BROADCAST BEACON RSSI:%d", &peerRSSI) == 1)
        {
            wifi_power_t newPower;
            if (peerRSSI > -40)
                newPower = WIFI_POWER_13dBm;
            else if (peerRSSI > -60)
                newPower = WIFI_POWER_17dBm;
            else
                newPower = WIFI_POWER_21dBm;
            WiFi.setTxPower(newPower);
        }
#ifdef ENABLE_LED
        xTaskNotifyGive(ledFlashTaskHandle);
#endif
        if (isBound)
            return;

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, esp_now_info->src_addr, 6);
        peer.channel = esp_now_info->rx_ctrl->channel;
        peer.encrypt = false;
        esp_err_t result = esp_now_add_peer(&peer);
        if (result == ESP_OK)
        {
            isBound = true;
            ESPNowWrapper::setBoundPeerAddress(esp_now_info->src_addr);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(queueMutex); // unlocks by itself when out of scope
    if (receiveQueue.size() < RECV_QUEUE_SIZE)
    {
        receiveQueue.push(newMessage);
    }
    else
        ; //just drop the packet
}

void dataSendCB(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    readyToSend = true;
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
    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, broadcastAddress, 6);
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    result = esp_now_add_peer(&broadcastPeer);
    if (result != ESP_OK)
        return result;
    isBound.store(false);
#ifdef ENABLE_LED
    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, NUM_LEDS);
    leds[0] = CRGB::Black;
    FastLED.show();
    xTaskCreate(
        ledFlashTask,
        "LEDFlashTask",
        2048,
        nullptr,
        1,
        &ledFlashTaskHandle);
#endif
    xTaskCreate(
        broadcastMACBeacon,
        "BeaconTask",
        4096,
        nullptr,
        1,
        &beaconTaskHandle);
    return result;
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
    readyToSend = false;
    esp_err_t result = esp_now_send(macAddress, (uint8_t *)&message, length + 2); // 2 bytes for size field
    return result;
}

esp_err_t ESPNowWrapper::sendWithRetries(uint8_t *macAddress, ESPMessage &message, uint16_t length)
{
    esp_err_t result;

    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++)
    {
        result = send(macAddress, message, length);

        if (result == ESP_OK)
        {
            unsigned long waitStart = millis();
            while (!readyToSend && (millis() - waitStart < 50))
            {
                vTaskDelay(1);
            }

            if (readyToSend)
            {
                return ESP_OK;
            }
        }

        if (attempt < MAX_RETRIES - 1)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    return ESP_FAIL;
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

bool ESPNowWrapper::areRadioRecvPacketsAvailable()
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return !receiveQueue.empty();
}

int ESPNowWrapper::getRSSI()
{
    return lastRSSI;
}

uint8_t *ESPNowWrapper::getBoundPeerAddress()
{
    return boundPeerAddress;
}

void ESPNowWrapper::setBoundPeerAddress(const uint8_t *address)
{
    memcpy(boundPeerAddress, address, 6);
}

#ifdef ENABLE_LED
static void ledFlashTask(void *parameter)
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        leds[0] = CRGB::Green;
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(10));
        leds[0] = CRGB::Black;
        FastLED.show();
    }
}
#endif

void broadcastMACBeacon(void *parameter)
{
    ESPMessage bcastBeacon;
    snprintf(bcastBeacon.data, sizeof(bcastBeacon.data), "BROADCAST BEACON RSSI:%d", lastRSSI);
    bcastBeacon.size = strlen(bcastBeacon.data) + 1;

    while (true)
    {
        esp_now_send(broadcastAddress, (uint8_t *)&bcastBeacon, bcastBeacon.size + 2);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}