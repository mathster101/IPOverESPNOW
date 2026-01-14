#include "ESPNowWrapper.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstring>

#define BAUD_RATE_SERIAL 921600
#define SERIAL_DELIMITER '\0'
#define SERIAL_BUFFER_SIZE 20480
#define SCREEN_ADDRESS 0x3c
#define SDA 17
#define SCL 18

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char inputBuffer[MAXIMUM_MESSAGE_SIZE];
char debugBuffer[100];

ESPNowWrapper espnowwrapper;
ESPMessage espMessage;

void printDebug(const char *dbgMessage)
{
    sprintf(debugBuffer, "<DEBUG>%s\0", dbgMessage);
    Serial.print(debugBuffer);
}

class OLEDHandler
{
private:
    Adafruit_SSD1306 oledDisplay;
    TaskHandle_t handle;
    char oledBuffer[512];

public:
    OLEDHandler() : oledDisplay(128, 64, &Wire, 21)
    {
        handle = nullptr;
    }

    void init()
    {
        Wire.begin(SDA, SCL);
        if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
        {
            while (true)
            {
                printDebug("SSD1306 allocation failed");
                delay(5000);
            }
        }
        oledDisplay.setTextSize(1);
        oledDisplay.setTextColor(SSD1306_WHITE);
        oledDisplay.display();
    }

    void printStats()
    {
        oledDisplay.clearDisplay();
        oledDisplay.setCursor(0, 0);

        int recvBufferbytes = Serial.available();
        int writeBufferbytes = SERIAL_BUFFER_SIZE - Serial.availableForWrite();

        sprintf(oledBuffer, "\nserialrd=%d\nserialwr=%d\nfreebytes=%ld\nqdepth=%d\nrssi=%ddB",
                recvBufferbytes, writeBufferbytes, ESP.getFreeHeap(), espnowwrapper.getReceiveQueueLength(), espnowwrapper.getRSSI());

        oledDisplay.println(oledBuffer);
        oledDisplay.display();
    }

    static void task(void *parameter)
    {
        while (true)
        {
            vTaskDelay(1);
        }
    }

    TaskHandle_t *getHandleRef()
    {
        return &handle;
    }

    static OLEDHandler *getInstance()
    {
        extern OLEDHandler oledhandler;
        return &oledhandler;
    }
};

OLEDHandler oledhandler;

void oledTaskWrapper(void *parameter)
{
    while (true)
    {
        oledhandler.printStats();
        vTaskDelay(1);
    }
}

void radioSendHandler()
{
    size_t bytesRead = Serial.readBytesUntil(SERIAL_DELIMITER, inputBuffer, MAXIMUM_MESSAGE_SIZE - 1);
    inputBuffer[bytesRead] = '\0';
    if (bytesRead == 0)
    {
        return;
    }
    strcpy(espMessage.data, inputBuffer);
    auto result = espnowwrapper.send(broadcastAddress, espMessage, bytesRead + 1);
}

void radioReceiveHandler()
{
    char databuffer[2048];

    if (espnowwrapper.receive(&espMessage) == -1)
        return;

    Serial.print(espMessage.data);
    Serial.print(SERIAL_DELIMITER);
}

void setup()
{
    Serial.setRxBufferSize(SERIAL_BUFFER_SIZE);
    Serial.setTxBufferSize(SERIAL_BUFFER_SIZE);
    Serial.begin(BAUD_RATE_SERIAL);

    oledhandler.init();

    if (espnowwrapper.init() != ESP_OK)
    {
        while (true)
        {
            printDebug("Error Initializing ESP-NOW");
            delay(5000);
        }
    }
    if (espnowwrapper.addPeer(broadcastAddress, 0) != ESP_OK)
    {
        while (true)
        {
            printDebug("Error adding broadcast peer");
            delay(5000);
        }
    }

    printDebug("Init complete");

    // Create OLED task
    xTaskCreatePinnedToCore(
        oledTaskWrapper,
        "OLEDTask",
        4096,
        NULL,
        1,
        oledhandler.getHandleRef(),
        0);
}

void loop()
{
    if (Serial.available() > 0)
    {
        radioSendHandler();
    }

    int pendingReadPackets = espnowwrapper.getReceiveQueueLength();
    if (pendingReadPackets > 0)
    {
        radioReceiveHandler();
    }
}