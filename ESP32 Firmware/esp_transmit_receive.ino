#include "ESPNowWrapper.h"
#include <cstring>

#define BAUD_RATE_SERIAL 921600
#define SERIAL_DELIMITER '\0'
#define SERIAL_BUFFER_SIZE 1500 * 30


ESPNowWrapper espnowwrapper;
ESPMessage espMessage;

void printDebug(const char *dbgMessage)
{
    Serial.printf("<DEBUG>%s", dbgMessage);
    Serial.print(SERIAL_DELIMITER);
}

void serialToRadio()
{
    char inputBuffer[MAXIMUM_MESSAGE_SIZE];
    size_t bytesRead = Serial.readBytesUntil(SERIAL_DELIMITER, inputBuffer, MAXIMUM_MESSAGE_SIZE - 1);
    inputBuffer[bytesRead] = '\0';
    if (bytesRead == 0)
    {
        return;
    }
    strcpy(espMessage.data, inputBuffer);
    auto result = espnowwrapper.send(espnowwrapper.getBoundPeerAddress(), espMessage, bytesRead + 1);
}

void radioToSerial()
{
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

    if (espnowwrapper.areRadioRecvPacketsAvailable())
    {
        radioToSerial();
    }
}