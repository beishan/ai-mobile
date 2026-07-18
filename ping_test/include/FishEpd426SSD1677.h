#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

constexpr int16_t EPD_WIDTH = 800;
constexpr int16_t EPD_HEIGHT = 480;

constexpr uint16_t EPD_BLACK = 0;
constexpr uint16_t EPD_WHITE = 1;

struct FishEpdPins {
  int8_t sck = 18;
  int8_t mosi = 23;
  int8_t cs = 5;
  int8_t dc = 26;
  int8_t rst = 27;
  int8_t busy = 4;
  uint8_t busyActiveLevel = HIGH;
};

class FishEpd426SSD1677 : public Adafruit_GFX {
 public:
  explicit FishEpd426SSD1677(const FishEpdPins& pins, SPIClass& spi = SPI);

  bool begin(uint32_t spiFrequency = 4000000);
  void clear(uint16_t color = EPD_WHITE);
  void display(bool powerOffAfter = true);
  void sleep();

  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  uint8_t* frameBuffer() { return _buffer; }
  size_t frameBufferSize() const { return sizeof(_buffer); }

 private:
  static constexpr size_t kBufferSize = EPD_WIDTH * EPD_HEIGHT / 8;

  FishEpdPins _pins;
  SPIClass* _spi;
  SPISettings _settings;
  uint8_t _buffer[kBufferSize];

  void reset();
  void initPanel();
  void powerOn();
  void powerOff();
  bool isBusy() const;
  void waitBusy(uint32_t timeoutMs = 30000);
  void command(uint8_t value);
  void data(uint8_t value);
  void data(const uint8_t* values, size_t length);
  void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
};
