#ifndef BOARD_CONFIG_HPP
#define BOARD_CONFIG_HPP

#include <Arduino.h>

#define ROASTER_BOARD_CURRENT 0
#define ROASTER_BOARD_JC4827W543C 1

#ifndef ROASTER_TARGET_BOARD
#define ROASTER_TARGET_BOARD ROASTER_BOARD_CURRENT
#endif

namespace BoardConfig
{
inline constexpr int DisplayWidth = 480;
inline constexpr int DisplayHeight = 272;
inline constexpr int TouchSdaPin = 8;
inline constexpr int TouchSclPin = 4;
inline constexpr int TouchIntPin = 3;
inline constexpr int TouchResetPin = 38;

#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
inline constexpr bool TouchInvertX = true;
inline constexpr bool TouchInvertY = true;
inline constexpr int BeanThermocoupleChipSelectPin = 9;
inline constexpr int FanThermocoupleChipSelectPin = 14;
inline constexpr int ThermocoupleClockPin = 5;
inline constexpr int ThermocoupleDataPin = 46;
inline constexpr int HeaterPwmPin = 6;
inline constexpr int FanPwmPin = 7;
inline constexpr int BdcFanServoPin = 15;
inline constexpr int ServiceUartTxPin = 17;
inline constexpr int ServiceUartRxPin = 18;
inline constexpr int AuxInputPin = 16;
#else
inline constexpr int BeanThermocoupleChipSelectPin = 10;
inline constexpr int FanThermocoupleChipSelectPin = 9;
inline constexpr int ThermocoupleClockPin = SCK;
inline constexpr int ThermocoupleDataPin = MISO;
inline constexpr int HeaterPwmPin = A0;
inline constexpr int FanPwmPin = A1;
inline constexpr int BdcFanServoPin = D5;
inline constexpr int ServiceUartTxPin = -1;
inline constexpr int ServiceUartRxPin = -1;
inline constexpr int AuxInputPin = -1;
inline constexpr bool TouchInvertX = false;
inline constexpr bool TouchInvertY = false;
#endif

inline constexpr unsigned long DisplayBaudRate = 115200;
}

#endif