#ifndef XPT2046_TOUCH_HPP
#define XPT2046_TOUCH_HPP

#include <Arduino.h>
#include <SPI.h>

class Xpt2046TouchController
{
public:
  Xpt2046TouchController(int chipSelectPin, int interruptPin, int clockPin, int misoPin, int mosiPin)
      : chipSelectPin_(chipSelectPin),
        interruptPin_(interruptPin),
        clockPin_(clockPin),
        misoPin_(misoPin),
        mosiPin_(mosiPin),
        spi_(HSPI)
  {
  }

  bool begin()
  {
    pinMode(chipSelectPin_, OUTPUT);
    digitalWrite(chipSelectPin_, HIGH);
    pinMode(interruptPin_, INPUT_PULLUP);
    return spi_.begin(clockPin_, misoPin_, mosiPin_, chipSelectPin_);
  }

  bool read(uint16_t &x, uint16_t &y)
  {
    if (digitalRead(interruptPin_) != LOW)
    {
      return false;
    }

    spi_.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(chipSelectPin_, LOW);

    uint16_t z1 = readRegister(0xB0);
    uint16_t z2 = readRegister(0xC0);
    int pressure = static_cast<int>(z1) + 4095 - static_cast<int>(z2);

    uint32_t rawX = 0;
    uint32_t rawY = 0;
    uint8_t validSamples = 0;

    if (pressure >= 300)
    {
      for (uint8_t sample = 0; sample < 3; ++sample)
      {
        uint16_t sampleX = readRegister(0xD0);
        uint16_t sampleY = readRegister(0x90);
        if (sampleX >= 50 && sampleX <= 4045 && sampleY >= 50 && sampleY <= 4045)
        {
          rawX += sampleX;
          rawY += sampleY;
          ++validSamples;
        }
      }
    }

    readRegister(0xD0);
    readRegister(0x00);
    digitalWrite(chipSelectPin_, HIGH);
    spi_.endTransaction();

    if (validSamples == 0)
    {
      return false;
    }

    x = static_cast<uint16_t>((rawX / validSamples) * 479UL / 4095UL);
    y = static_cast<uint16_t>((rawY / validSamples) * 271UL / 4095UL);
    return true;
  }

private:
  uint16_t readRegister(uint8_t command)
  {
    spi_.transfer(command);
    uint16_t value = static_cast<uint16_t>(spi_.transfer(0)) << 8;
    value |= spi_.transfer(0);
    return value >> 3;
  }

  int chipSelectPin_;
  int interruptPin_;
  int clockPin_;
  int misoPin_;
  int mosiPin_;
  SPIClass spi_;
};

#endif
