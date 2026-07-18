#include "FishEpd426SSD1677.h"

FishEpd426SSD1677::FishEpd426SSD1677(const FishEpdPins& pins, SPIClass& spi)
    : Adafruit_GFX(EPD_WIDTH, EPD_HEIGHT),
      _pins(pins),
      _spi(&spi),
      _settings(4000000, MSBFIRST, SPI_MODE0) {
  clear();
}

bool FishEpd426SSD1677::begin(uint32_t spiFrequency) {
  _settings = SPISettings(spiFrequency, MSBFIRST, SPI_MODE0);

  pinMode(_pins.cs, OUTPUT);
  pinMode(_pins.dc, OUTPUT);
  pinMode(_pins.rst, OUTPUT);
  pinMode(_pins.busy, INPUT);
  digitalWrite(_pins.cs, HIGH);
  digitalWrite(_pins.dc, HIGH);
  digitalWrite(_pins.rst, HIGH);

  Serial.print(F("EPD pins SCK="));
  Serial.print(_pins.sck);
  Serial.print(F(" MOSI="));
  Serial.print(_pins.mosi);
  Serial.print(F(" CS="));
  Serial.print(_pins.cs);
  Serial.print(F(" DC="));
  Serial.print(_pins.dc);
  Serial.print(F(" RST="));
  Serial.print(_pins.rst);
  Serial.print(F(" BUSY="));
  Serial.print(_pins.busy);
  Serial.print(F(" BUSY active="));
  Serial.println(_pins.busyActiveLevel == HIGH ? F("HIGH") : F("LOW"));

  _spi->begin(_pins.sck, -1, _pins.mosi, _pins.cs);
  reset();
  initPanel();
  return true;
}

void FishEpd426SSD1677::clear(uint16_t color) {
  memset(_buffer, color == EPD_BLACK ? 0x00 : 0xFF, sizeof(_buffer));
}

void FishEpd426SSD1677::display(bool powerOffAfter) {
  Serial.println(F("EPD display begin"));
  powerOn();
  setWindow(0, 0, EPD_WIDTH, EPD_HEIGHT);

  command(0x10);  // old image
  for (size_t i = 0; i < sizeof(_buffer); ++i) {
    data(0xFF);
  }

  command(0x13);  // new image
  data(_buffer, sizeof(_buffer));

  command(0x12);  // display refresh
  waitBusy(60000);
  Serial.println(F("EPD refresh finished"));

  if (powerOffAfter) {
    powerOff();
  }
}

void FishEpd426SSD1677::sleep() {
  powerOff();
  command(0x07);  // deep sleep
  data(0xA5);
  delay(100);
}

void FishEpd426SSD1677::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= width() || y < 0 || y >= height()) {
    return;
  }

  switch (getRotation()) {
    case 1:
      {
        int16_t tmp = x;
        x = y;
        y = tmp;
      }
      x = EPD_WIDTH - x - 1;
      break;
    case 2:
      x = EPD_WIDTH - x - 1;
      y = EPD_HEIGHT - y - 1;
      break;
    case 3:
      {
        int16_t tmp = x;
        x = y;
        y = tmp;
      }
      y = EPD_HEIGHT - y - 1;
      break;
    default:
      break;
  }

  const uint32_t index = (static_cast<uint32_t>(y) * EPD_WIDTH + x) / 8;
  const uint8_t mask = 0x80 >> (x & 7);
  if (color == EPD_BLACK) {
    _buffer[index] &= ~mask;
  } else {
    _buffer[index] |= mask;
  }
}

void FishEpd426SSD1677::reset() {
  digitalWrite(_pins.rst, HIGH);
  delay(20);
  digitalWrite(_pins.rst, LOW);
  delay(20);
  digitalWrite(_pins.rst, HIGH);
  delay(200);
}

void FishEpd426SSD1677::initPanel() {
  command(0x12);  // software reset
  waitBusy();

  command(0x01);  // driver output control
  data((EPD_HEIGHT - 1) & 0xFF);
  data((EPD_HEIGHT - 1) >> 8);
  data(0x00);

  command(0x11);  // data entry mode: X then Y increment
  data(0x03);

  command(0x44);  // RAM X range, byte addressed
  data(0x00);
  data((EPD_WIDTH / 8) - 1);

  command(0x45);  // RAM Y range
  data(0x00);
  data(0x00);
  data((EPD_HEIGHT - 1) & 0xFF);
  data((EPD_HEIGHT - 1) >> 8);

  command(0x3C);  // border waveform
  data(0x01);

  command(0x18);  // temperature sensor
  data(0x80);

  command(0x4E);  // RAM X address
  data(0x00);
  command(0x4F);  // RAM Y address
  data(0x00);
  data(0x00);
  waitBusy();
}

void FishEpd426SSD1677::powerOn() {
  command(0x22);
  data(0xF8);
  command(0x20);
  waitBusy();
}

void FishEpd426SSD1677::powerOff() {
  command(0x22);
  data(0x83);
  command(0x20);
  waitBusy();
}

bool FishEpd426SSD1677::isBusy() const {
  return digitalRead(_pins.busy) == _pins.busyActiveLevel;
}

void FishEpd426SSD1677::waitBusy(uint32_t timeoutMs) {
  const uint32_t start = millis();
  Serial.print(F("EPD BUSY initial="));
  Serial.println(digitalRead(_pins.busy));
  while (isBusy()) {
    delay(10);
    if (millis() - start > timeoutMs) {
      Serial.println(F("EPD BUSY timeout"));
      break;
    }
  }
}

void FishEpd426SSD1677::command(uint8_t value) {
  _spi->beginTransaction(_settings);
  digitalWrite(_pins.dc, LOW);
  digitalWrite(_pins.cs, LOW);
  _spi->transfer(value);
  digitalWrite(_pins.cs, HIGH);
  _spi->endTransaction();
}

void FishEpd426SSD1677::data(uint8_t value) {
  _spi->beginTransaction(_settings);
  digitalWrite(_pins.dc, HIGH);
  digitalWrite(_pins.cs, LOW);
  _spi->transfer(value);
  digitalWrite(_pins.cs, HIGH);
  _spi->endTransaction();
}

void FishEpd426SSD1677::data(const uint8_t* values, size_t length) {
  _spi->beginTransaction(_settings);
  digitalWrite(_pins.dc, HIGH);
  digitalWrite(_pins.cs, LOW);
  for (size_t i = 0; i < length; ++i) {
    _spi->transfer(values[i]);
  }
  digitalWrite(_pins.cs, HIGH);
  _spi->endTransaction();
}

void FishEpd426SSD1677::setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  const uint16_t xe = x + w - 1;
  const uint16_t ye = y + h - 1;

  command(0x44);
  data(x / 8);
  data(xe / 8);

  command(0x45);
  data(y & 0xFF);
  data(y >> 8);
  data(ye & 0xFF);
  data(ye >> 8);

  command(0x4E);
  data(x / 8);
  command(0x4F);
  data(y & 0xFF);
  data(y >> 8);
  waitBusy();
}
