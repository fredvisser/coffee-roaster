#ifndef BOARD_CONFIG_HPP
#define BOARD_CONFIG_HPP

#include <Arduino.h>

#define ROASTER_BOARD_JC4827W543C 1
#define ROASTER_BOARD_JC4827W543R 2

#ifndef ROASTER_TARGET_BOARD
#define ROASTER_TARGET_BOARD ROASTER_BOARD_JC4827W543C
#endif

namespace BoardConfig
{
inline constexpr int DisplayWidth = 480;
inline constexpr int DisplayHeight = 272;
#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
inline constexpr bool DisplayInvert = false;
inline constexpr int TouchSdaPin = 8;
inline constexpr int TouchSclPin = 4;
inline constexpr int TouchIntPin = 3;
inline constexpr int TouchResetPin = 38;
inline constexpr bool TouchInvertX = true;
inline constexpr bool TouchInvertY = true;
inline constexpr int TfCsPin = 10;
inline constexpr int TfMosiPin = 11;
inline constexpr int TfSckPin = 12;
inline constexpr int TfMisoPin = 13;
#elif ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543R
inline constexpr bool DisplayInvert = true;
inline constexpr int TouchSpiSckPin = 12;
inline constexpr int TouchSpiMisoPin = 13;
inline constexpr int TouchSpiMosiPin = 11;
inline constexpr int TouchSpiChipSelectPin = 38;
inline constexpr int TouchIntPin = 3;
inline constexpr bool TouchInvertX = false;
inline constexpr bool TouchInvertY = false;
#else
#error "Unsupported ROASTER_TARGET_BOARD"
#endif
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

inline constexpr unsigned long DisplayBaudRate = 115200;
}

#endif