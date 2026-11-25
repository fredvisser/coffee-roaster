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
#ifndef ROASTER_DISPLAY_INVERT
#define ROASTER_DISPLAY_INVERT 0
#endif
#ifndef ROASTER_TOUCH_INVERT_X
#define ROASTER_TOUCH_INVERT_X 1
#endif
#ifndef ROASTER_TOUCH_INVERT_Y
#define ROASTER_TOUCH_INVERT_Y 1
#endif
inline constexpr int TouchSdaPin = 8;
inline constexpr int TouchSclPin = 4;
inline constexpr int TouchIntPin = 3;
inline constexpr int TouchResetPin = 38;
inline constexpr int TfCsPin = 10;
inline constexpr int TfMosiPin = 11;
inline constexpr int TfSckPin = 12;
inline constexpr int TfMisoPin = 13;
#elif ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543R
#ifndef ROASTER_DISPLAY_INVERT
#define ROASTER_DISPLAY_INVERT 1
#endif
#ifndef ROASTER_TOUCH_INVERT_X
#define ROASTER_TOUCH_INVERT_X 0
#endif
#ifndef ROASTER_TOUCH_INVERT_Y
#define ROASTER_TOUCH_INVERT_Y 0
#endif
inline constexpr int TouchSpiSckPin = 12;
inline constexpr int TouchSpiMisoPin = 13;
inline constexpr int TouchSpiMosiPin = 11;
inline constexpr int TouchSpiChipSelectPin = 38;
inline constexpr int TouchIntPin = 3;
#else
#error "Unsupported ROASTER_TARGET_BOARD"
#endif
inline constexpr bool DisplayInvert = ROASTER_DISPLAY_INVERT != 0;
inline constexpr bool TouchInvertX = ROASTER_TOUCH_INVERT_X != 0;
inline constexpr bool TouchInvertY = ROASTER_TOUCH_INVERT_Y != 0;
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