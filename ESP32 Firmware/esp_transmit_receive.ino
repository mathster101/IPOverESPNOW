#include "ESPNowWrapper.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstring>

#define BAUD_RATE_SERIAL 921600
#define SERIAL_DELIMITER '\0'
#define SERIAL_BUFFER_SIZE 30720


ESPNowWrapper espnowwrapper;
ESPMessage espMessage;
char inputBuffer[MAXIMUM_MESSAGE_SIZE];

void printDebug(const char *dbgMessage);

void serialToRadio()
{
    size_t bytesRead = Serial.readBytesUntil(SERIAL_DELIMITER, inputBuffer, MAXIMUM_MESSAGE_SIZE - 1);
    inputBuffer[bytesRead] = '\0';
    if (bytesRead == 0)
    {
        return;
    }
    strcpy(espMessage.data, inputBuffer);
    char printBuffer[100];
    unsigned long start = micros();
    auto result = espnowwrapper.send(espnowwrapper.boundPeerAddress, espMessage, bytesRead + 1);
    // sprintf(printBuffer, "%u Bytes sent in %luus\n", bytesRead, micros() - start);
    // printDebug(printBuffer);
}

void radioToSerial()
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


    if (espnowwrapper.init() != ESP_OK)
    {
        while (true)
        {
            printDebug("Error Initializing ESP-NOW");
            delay(5000);
        }
    }

    printDebug("Init complete");

}

void loop()
{
    if (Serial.available() > 0)
    {
        serialToRadio();
    }

    int pendingReadPackets = espnowwrapper.getReceiveQueueLength();
    if (pendingReadPackets > 0)
    {
        radioToSerial();
    }
}


void printDebug(const char *dbgMessage)
{
    char printBuffer[512];
    sprintf(printBuffer, "<DEBUG>%s", dbgMessage);
    Serial.print(printBuffer);
    Serial.print(SERIAL_DELIMITER);
}