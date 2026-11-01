#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>

#define DEVICE_NAME "OLED_BLE_UPDATER"
#define SERVICE_UUID "7b100001-7c4a-4f2d-9a31-123456789001"
#define SIZE_UUID    "7b100002-7c4a-4f2d-9a31-123456789001"
#define DATA_UUID    "7b100003-7c4a-4f2d-9a31-123456789001"
#define FINISH_UUID  "7b100004-7c4a-4f2d-9a31-123456789001"
#define STATUS_UUID  "7b100005-7c4a-4f2d-9a31-123456789001"

BLECharacteristic *statusCharacteristic = nullptr;
volatile bool deviceConnected = false;
uint32_t expectedSize = 0;
uint32_t receivedSize = 0;
bool updateStarted = false;
bool updateFinished = false;

void sendStatus(const char *message) {
  if (statusCharacteristic) {
    statusCharacteristic->setValue(message);
    if (deviceConnected) statusCharacteristic->notify();
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    deviceConnected = true;
    sendStatus("CONNECTED");
  }
  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    if (updateStarted && !updateFinished) {
      Update.abort();
      updateStarted = false;
      sendStatus("TRANSFER_ABORTED");
    }
    BLEDevice::startAdvertising();
  }
};

class SizeCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    if (updateStarted) { sendStatus("UPDATE_BUSY"); return; }
    size_t length = characteristic->getLength();
    uint8_t *data = characteristic->getData();
    if (!data || length != 4) { sendStatus("INVALID_SIZE"); return; }
    expectedSize = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                   ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    if (expectedSize == 0 || expectedSize > 0x140000) {
      sendStatus("SIZE_INVALID_OR_TOO_LARGE"); return;
    }
    receivedSize = 0;
    if (!Update.begin(expectedSize, U_FLASH)) {
      Update.printError(Serial); sendStatus("UPDATE_BEGIN_FAILED"); return;
    }
    updateStarted = true;
    updateFinished = false;
    sendStatus("READY");
    Serial.printf("Expected firmware: %lu bytes\n", (unsigned long)expectedSize);
  }
};

class DataCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    if (!updateStarted || updateFinished) { sendStatus("NOT_READY"); return; }
    size_t length = characteristic->getLength();
    uint8_t *data = characteristic->getData();
    if (!data || length == 0) return;
    if (receivedSize + length > expectedSize) {
      Update.abort(); updateStarted = false; sendStatus("SIZE_OVERFLOW"); return;
    }
    size_t written = Update.write(data, length);
    if (written != length) {
      Update.abort(); updateStarted = false;
      Update.printError(Serial); sendStatus("WRITE_FAILED"); return;
    }
    receivedSize += written;
    if (receivedSize == expectedSize) sendStatus("DATA_RECEIVED");
  }
};

class FinishCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    if (!updateStarted || updateFinished) { sendStatus("NOT_READY"); return; }
    if (receivedSize != expectedSize) { sendStatus("INCOMPLETE"); return; }
    if (Update.end(true)) {
      updateFinished = true;
      updateStarted = false;
      sendStatus("UPDATE_SUCCESS_RESTARTING");
      Serial.println("Firmware update complete; restarting.");
      delay(1200);
      ESP.restart();
    } else {
      Update.printError(Serial);
      Update.abort(); updateStarted = false; sendStatus("UPDATE_FAILED");
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting ESP32-C3 BLE OTA updater");
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(517);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *sizeChar = service->createCharacteristic(SIZE_UUID, BLECharacteristic::PROPERTY_WRITE);
  sizeChar->setCallbacks(new SizeCallbacks());
  BLECharacteristic *dataChar = service->createCharacteristic(DATA_UUID, BLECharacteristic::PROPERTY_WRITE);
  dataChar->setCallbacks(new DataCallbacks());
  BLECharacteristic *finishChar = service->createCharacteristic(FINISH_UUID, BLECharacteristic::PROPERTY_WRITE);
  finishChar->setCallbacks(new FinishCallbacks());
  statusCharacteristic = service->createCharacteristic(STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusCharacteristic->addDescriptor(new BLE2902());
  statusCharacteristic->setValue("WAITING");
  service->start();
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Advertising as OLED_BLE_UPDATER");
}

void loop() { delay(100); }
