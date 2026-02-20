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

void sendSerialToRadio()
{
  char inputBuffer[MAXIMUM_MESSAGE_SIZE];
  size_t bytesRead = Serial.readBytesUntil(SERIAL_DELIMITER, inputBuffer, MAXIMUM_MESSAGE_SIZE - 1);
  inputBuffer[bytesRead] = '\0';
  if (bytesRead == 0)
  {
    return;
  }
  strcpy(espMessage.data, inputBuffer);
  auto result = espnowwrapper.sendWithRetries(espnowwrapper.getBoundPeerAddress(), espMessage, bytesRead + 1);
}

void receiveRadioToSerial()
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
  processSerialData();
  processRadioData();
  outputDebugInfo();
}

void processSerialData()
{
  for (int readCount = 0; Serial.available() > 0 && readCount < MAX_LOOP_READS; ++readCount)
  {
    sendSerialToRadio();
  }
}

void processRadioData()
{
  for (int readCount = 0; espnowwrapper.areRadioRecvPacketsAvailable() && readCount < MAX_LOOP_READS; ++readCount)
  {
    receiveRadioToSerial();
  }
}

void outputDebugInfo()
{
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug >= 2000)
  {
    printDebug(("<RSSI>" + String(espnowwrapper.getRSSI())).c_str());
    lastDebug = millis();
  }
}