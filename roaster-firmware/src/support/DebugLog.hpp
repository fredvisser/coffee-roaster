#ifndef DEBUGLOG_HPP
#define DEBUGLOG_HPP

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "../platform/BoardConfig.hpp"
#include <vector>

// Debug logging system with ring buffer for web console

// Log levels
enum LogLevel {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_ERROR = 3
};

// Log entry structure
struct LogEntry {
  unsigned long timestamp;
  uint32_t bootId;
  LogLevel level;
  char message[160];  // Keep bounded but large enough for HTTP/API diagnostics
};

// Ring buffer for log entries
class DebugLogger {
private:
  static const int MAX_LOGS = 100;
  static constexpr UBaseType_t CARD_LOG_QUEUE_LENGTH = 64;
  static constexpr uint32_t CARD_LOG_TASK_STACK = 6144;
  static constexpr uint8_t CARD_LOG_FILE_COUNT = 5;
  static constexpr uint32_t CARD_LOG_FILE_LIMIT = 1024 * 1024;
  LogEntry logs[MAX_LOGS];
  int writeIndex;
  int count;
  QueueHandle_t cardLogQueue;
  File cardLogFile;
  SPIClass cardSpi{HSPI};
  SemaphoreHandle_t cardStorageMutex;
  volatile bool cardLoggingEnabled;
  volatile bool cardMounted;
  uint32_t bootSessionId;
  const char* cardLogError;
  mutable portMUX_TYPE logsMux = portMUX_INITIALIZER_UNLOCKED;

  String cardLogPath(uint8_t fileIndex) const {
    if (fileIndex == 0) return "/roaster-debug.jsonl";
    return String("/roaster-debug-") + String(fileIndex) + ".jsonl";
  }

  bool rotateCardLogFile() {
    cardLogFile.flush();
    if (cardLogFile && cardLogFile.getWriteError() != 0) return false;
    cardLogFile.close();

    String oldestPath = cardLogPath(CARD_LOG_FILE_COUNT - 1);
    if (SD.exists(oldestPath) && !SD.remove(oldestPath)) return false;

    for (int fileIndex = CARD_LOG_FILE_COUNT - 1; fileIndex > 0; --fileIndex) {
      String sourcePath = cardLogPath(static_cast<uint8_t>(fileIndex - 1));
      String destinationPath = cardLogPath(static_cast<uint8_t>(fileIndex));
      if (SD.exists(sourcePath) && !SD.rename(sourcePath, destinationPath)) return false;
    }

    cardLogFile = SD.open(cardLogPath(0), FILE_APPEND);
    return static_cast<bool>(cardLogFile);
  }

  bool appendLogText(char* output, size_t capacity, size_t& length, const char* text, size_t textLength) const {
    if (length + textLength > capacity) return false;
    memcpy(output + length, text, textLength);
    length += textLength;
    return true;
  }

  bool appendJsonString(char* output, size_t capacity, size_t& length, const char* value) const {
    if (!appendLogText(output, capacity, length, "\"", 1)) return false;

    for (const unsigned char* character = reinterpret_cast<const unsigned char*>(value); *character != '\0'; ++character) {
      const char* escaped = nullptr;
      size_t escapedLength = 0;
      char unicodeEscape[7];

      switch (*character) {
        case '"': escaped = "\\\""; escapedLength = 2; break;
        case '\\': escaped = "\\\\"; escapedLength = 2; break;
        case '\b': escaped = "\\b"; escapedLength = 2; break;
        case '\f': escaped = "\\f"; escapedLength = 2; break;
        case '\n': escaped = "\\n"; escapedLength = 2; break;
        case '\r': escaped = "\\r"; escapedLength = 2; break;
        case '\t': escaped = "\\t"; escapedLength = 2; break;
        default: break;
      }

      if (escaped != nullptr) {
        if (!appendLogText(output, capacity, length, escaped, escapedLength)) return false;
      } else if (*character < 0x20) {
        int escapeLength = snprintf(unicodeEscape, sizeof(unicodeEscape), "\\u%04x", *character);
        if (escapeLength < 0 || !appendLogText(output, capacity, length, unicodeEscape, static_cast<size_t>(escapeLength))) return false;
      } else {
        char rawCharacter = static_cast<char>(*character);
        if (!appendLogText(output, capacity, length, &rawCharacter, 1)) return false;
      }
    }

    return appendLogText(output, capacity, length, "\"", 1);
  }

  bool writeCardLogEntry(const LogEntry& entry) {
    char output[sizeof(entry.message) * 6 + 128];
    int headerLength = snprintf(output, sizeof(output), "{\"boot_id\":\"%08lx\",\"uptime_ms\":%lu,\"level\":\"%s\",\"message\":",
                                static_cast<unsigned long>(entry.bootId), static_cast<unsigned long>(entry.timestamp), getLevelName(entry.level));
    if (headerLength < 0 || static_cast<size_t>(headerLength) >= sizeof(output)) return false;

    size_t length = static_cast<size_t>(headerLength);
    if (!appendJsonString(output, sizeof(output), length, entry.message) ||
        !appendLogText(output, sizeof(output), length, "}\n", 2)) return false;

    if (xSemaphoreTake(cardStorageMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    bool success = true;
    if (cardLogFile.size() > 0 && cardLogFile.size() + length > CARD_LOG_FILE_LIMIT) {
      success = rotateCardLogFile();
    }
    if (success) {
      success = cardLogFile.write(reinterpret_cast<const uint8_t*>(output), length) == length;
    }
    xSemaphoreGive(cardStorageMutex);
    return success;
  }

  void stopCardLogging(const char* reason) {
    cardLoggingEnabled = false;
    cardLogError = reason;
    if (cardStorageMutex != nullptr && xSemaphoreTake(cardStorageMutex, portMAX_DELAY) == pdTRUE) {
      cardLogFile.close();
      xSemaphoreGive(cardStorageMutex);
    }
    Serial.printf("TF card logging stopped: %s\n", reason);
  }

  void cardLoggingTask() {
    bool hasUnflushedEntries = false;
    unsigned long lastFlush = millis();

    for (;;) {
      LogEntry entry;
      if (xQueueReceive(cardLogQueue, &entry, pdMS_TO_TICKS(250)) == pdTRUE) {
        do {
          if (!writeCardLogEntry(entry)) {
            stopCardLogging("write failed");
            vTaskDelete(nullptr);
          }
          hasUnflushedEntries = true;
        } while (xQueueReceive(cardLogQueue, &entry, 0) == pdTRUE);
      }

      if (hasUnflushedEntries && millis() - lastFlush >= 1000) {
        bool flushFailed = true;
        if (xSemaphoreTake(cardStorageMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          cardLogFile.flush();
          flushFailed = cardLogFile.getWriteError() != 0;
          xSemaphoreGive(cardStorageMutex);
        }
        if (flushFailed) {
          stopCardLogging("flush failed");
          vTaskDelete(nullptr);
        }
        hasUnflushedEntries = false;
        lastFlush = millis();
      }
    }
  }

  static void cardLoggingTaskEntry(void* context) {
    static_cast<DebugLogger*>(context)->cardLoggingTask();
  }

  void reverseText(String& value) const {
    for (size_t left = 0, right = value.length(); left < right && left < --right; ++left) {
      char character = value[left];
      value.setCharAt(left, value[right]);
      value.setCharAt(right, character);
    }
  }

public:
  DebugLogger() : writeIndex(0), count(0), cardLogQueue(nullptr), cardStorageMutex(nullptr), cardLoggingEnabled(false), cardMounted(false), bootSessionId(0), cardLogError("not_started") {}

  bool beginCard() {
#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
    cardLogError = "initializing";
    cardStorageMutex = xSemaphoreCreateMutex();
    if (cardStorageMutex == nullptr) {
      cardLogError = "mutex_allocation_failed";
      return false;
    }

    cardSpi.begin(BoardConfig::TfSckPin, BoardConfig::TfMisoPin, BoardConfig::TfMosiPin, BoardConfig::TfCsPin);
    if (!SD.begin(BoardConfig::TfCsPin, cardSpi, 4000000)) {
      cardLogError = "mount_failed";
      return false;
    }
    if (SD.cardType() == CARD_NONE) {
      SD.end();
      cardLogError = "no_card_detected";
      return false;
    }

    cardMounted = true;
    bootSessionId = esp_random();

    File existingLog = SD.open(cardLogPath(0), FILE_READ);
    bool activeLogFull = existingLog && existingLog.size() >= CARD_LOG_FILE_LIMIT;
    existingLog.close();
    if (activeLogFull && !rotateCardLogFile()) {
      cardLogError = "rotation_failed";
      return false;
    }

    cardLogFile = SD.open(cardLogPath(0), FILE_APPEND);
    if (!cardLogFile) {
      cardLogError = "log_file_open_failed";
      return false;
    }

    cardLogQueue = xQueueCreate(CARD_LOG_QUEUE_LENGTH, sizeof(LogEntry));
    if (cardLogQueue == nullptr) {
      cardLogFile.close();
      cardLogError = "queue_allocation_failed";
      return false;
    }

    if (xTaskCreate(cardLoggingTaskEntry, "sd-log", CARD_LOG_TASK_STACK, this, 1, nullptr) != pdPASS) {
      vQueueDelete(cardLogQueue);
      cardLogQueue = nullptr;
      cardLogFile.close();
      cardLogError = "task_creation_failed";
      return false;
    }
    cardLoggingEnabled = true;
    cardLogError = "none";
    return true;
#else
    cardLogError = "unsupported_board";
    return false;
#endif
  }

  String getCardLogStatusJSON() {
    String json = "{\"available\":";
    json += cardMounted ? "true" : "false";
    json += ",\"logging\":";
    json += cardLoggingEnabled ? "true" : "false";
    json += ",\"error\":\"";
    json += cardLogError;
    json += "\"";
    json += ",\"files\":[";

    if (cardMounted) {
      if (cardStorageMutex == nullptr || xSemaphoreTake(cardStorageMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        json += "],\"busy\":true}";
        return json;
      }
      bool firstFile = true;
      for (uint8_t fileIndex = 0; fileIndex < CARD_LOG_FILE_COUNT; ++fileIndex) {
        String path = cardLogPath(fileIndex);
        File file = SD.open(path, FILE_READ);
        if (!file) continue;
        if (!firstFile) json += ",";
        json += "{\"id\":" + String(fileIndex) + ",\"name\":\"" + path.substring(1) + "\",\"size\":" + String(file.size()) + "}";
        firstFile = false;
        file.close();
      }
      xSemaphoreGive(cardStorageMutex);
    }

    json += "],\"busy\":false}";
    return json;
  }

  String getCardLogPageJSON(uint8_t fileIndex, uint32_t cursor, bool startAtEnd, int maxEntries) {
    if (!cardMounted || cardStorageMutex == nullptr) {
      return "{\"available\":false,\"logs\":[],\"has_more\":false,\"next_cursor\":0}";
    }
    if (fileIndex >= CARD_LOG_FILE_COUNT) {
      return "{\"available\":true,\"file_found\":false,\"logs\":[],\"has_more\":false,\"next_cursor\":0}";
    }
    if (xSemaphoreTake(cardStorageMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
      return "{\"available\":true,\"busy\":true,\"logs\":[],\"has_more\":false,\"next_cursor\":0}";
    }

    String path = cardLogPath(fileIndex);
    File file = SD.open(path, FILE_READ);
    if (!file) {
      xSemaphoreGive(cardStorageMutex);
      return "{\"available\":true,\"file_found\":false,\"logs\":[],\"has_more\":false,\"next_cursor\":0}";
    }

    uint32_t fileSize = file.size();
    uint32_t scanPosition = startAtEnd || cursor > fileSize ? fileSize : cursor;
    std::vector<String> newestFirst;
    newestFirst.reserve(maxEntries);
    String reverseLine;
    reverseLine.reserve(1024);
    uint32_t currentLineStart = scanPosition;
    uint32_t nextCursor = 0;
    bool lineTooLong = false;
    bool pageFull = false;
    uint8_t block[256];

    while (scanPosition > 0 && newestFirst.size() < static_cast<size_t>(maxEntries)) {
      uint32_t blockStart = scanPosition > sizeof(block) ? scanPosition - sizeof(block) : 0;
      file.seek(blockStart);
      size_t requestedLength = scanPosition - blockStart;
      size_t bytesRead = file.readBytes(reinterpret_cast<char*>(block), requestedLength);
      if (bytesRead == 0) break;

      for (size_t byteIndex = bytesRead; byteIndex > 0; --byteIndex) {
        uint32_t bytePosition = blockStart + static_cast<uint32_t>(byteIndex - 1);
        char character = static_cast<char>(block[byteIndex - 1]);
        scanPosition = bytePosition;

        if (character == '\n') {
          if (!lineTooLong && reverseLine.length() > 0) {
            reverseText(reverseLine);
            if (reverseLine.startsWith("{") && reverseLine.endsWith("}")) {
              newestFirst.push_back(reverseLine);
              if (newestFirst.size() >= static_cast<size_t>(maxEntries)) {
                nextCursor = currentLineStart;
                pageFull = true;
                break;
              }
            }
          }
          reverseLine = "";
          lineTooLong = false;
          currentLineStart = bytePosition;
        } else {
          currentLineStart = bytePosition;
          if (reverseLine.length() < 2048) reverseLine += character;
          else lineTooLong = true;
        }
      }
      if (pageFull) break;
    }

    if (!pageFull && scanPosition == 0 && !lineTooLong && reverseLine.length() > 0 && newestFirst.size() < static_cast<size_t>(maxEntries)) {
      reverseText(reverseLine);
      if (reverseLine.startsWith("{") && reverseLine.endsWith("}")) newestFirst.push_back(reverseLine);
    }
    file.close();
    xSemaphoreGive(cardStorageMutex);

    String json;
    json.reserve(128 + newestFirst.size() * 256);
    json = "{\"available\":true,\"file_found\":true,\"file_index\":" + String(fileIndex);
    json += ",\"file_size\":" + String(fileSize);
    json += ",\"has_more\":";
    json += pageFull && nextCursor > 0 ? "true" : "false";
    json += ",\"next_cursor\":" + String(pageFull ? nextCursor : 0) + ",\"logs\":[";
    for (size_t index = newestFirst.size(); index > 0; --index) {
      if (index != newestFirst.size()) json += ",";
      json += newestFirst[index - 1];
    }
    json += "]}";
    return json;
  }
  
  // Add a log entry
  void log(LogLevel level, const char* message) {
    LogEntry entry;
    entry.timestamp = millis();
    entry.bootId = bootSessionId;
    entry.level = level;
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';

    portENTER_CRITICAL(&logsMux);
    logs[writeIndex] = entry;
    writeIndex = (writeIndex + 1) % MAX_LOGS;
    if (count < MAX_LOGS) count++;
    portEXIT_CRITICAL(&logsMux);

    if (cardLoggingEnabled) xQueueSend(cardLogQueue, &entry, 0);

    // Also print to Serial for debugging
    #ifdef DEBUG
    printLogEntry(entry);
    #endif
  }
  
  // Get log level name
  const char* getLevelName(LogLevel level) const {
    switch(level) {
      case LOG_LEVEL_DEBUG: return "DEBUG";
      case LOG_LEVEL_INFO:  return "INFO";
      case LOG_LEVEL_WARN:  return "WARN";
      case LOG_LEVEL_ERROR: return "ERROR";
      default: return "UNKNOWN";
    }
  }
  
  // Print a single log entry to Serial
  void printLogEntry(const LogEntry& entry) const {
    Serial.printf("[%lu] %s: %s\n", 
                  entry.timestamp, 
                  getLevelName(entry.level), 
                  entry.message);
  }
  
  // Get logs as JSON array
  String getLogsJSON(int maxEntries = 50, bool wrapInObject = false) const {
    String json = wrapInObject ? "{\"logs\":[" : "[";

    portENTER_CRITICAL(&logsMux);
    int entriesToReturn = min(maxEntries, count);
    int startIndex = (writeIndex - entriesToReturn + MAX_LOGS) % MAX_LOGS;
    portEXIT_CRITICAL(&logsMux);

    for (int i = 0; i < entriesToReturn; i++) {
      int index = (startIndex + i) % MAX_LOGS;
      LogEntry entry;

      portENTER_CRITICAL(&logsMux);
      entry = logs[index];
      portEXIT_CRITICAL(&logsMux);
      
      if (i > 0) json += ",";
      
      json += "{";
      json += "\"timestamp\":" + String(entry.timestamp) + ",";
      json += "\"level\":\"" + String(getLevelName(entry.level)) + "\",";
      json += "\"message\":\"";
      
      // Escape special characters in message
      for (int j = 0; j < sizeof(entry.message) && entry.message[j] != '\0'; j++) {
        unsigned char character = static_cast<unsigned char>(entry.message[j]);
        switch (character) {
          case '"': json += "\\\""; break;
          case '\\': json += "\\\\"; break;
          case '\b': json += "\\b"; break;
          case '\f': json += "\\f"; break;
          case '\n': json += "\\n"; break;
          case '\r': json += "\\r"; break;
          case '\t': json += "\\t"; break;
          default:
            if (character < 0x20) {
              char unicodeEscape[7];
              snprintf(unicodeEscape, sizeof(unicodeEscape), "\\u%04x", character);
              json += unicodeEscape;
            } else {
              json += static_cast<char>(character);
            }
            break;
        }
      }
      
      json += "\"}";
    }
    
    json += wrapInObject ? "]}" : "]";
    return json;
  }
  
  // Clear all logs
  void clear() {
    portENTER_CRITICAL(&logsMux);
    writeIndex = 0;
    count = 0;
    portEXIT_CRITICAL(&logsMux);
  }
  
  // Get log count
  int getCount() const {
    portENTER_CRITICAL(&logsMux);
    int currentCount = count;
    portEXIT_CRITICAL(&logsMux);
    return currentCount;
  }
};

// Global logger instance
DebugLogger debugLogger;

// Convenience macros for logging
#define LOG_DEBUG(msg) debugLogger.log(LOG_LEVEL_DEBUG, msg)
#define LOG_INFO(msg) debugLogger.log(LOG_LEVEL_INFO, msg)
#define LOG_WARN(msg) debugLogger.log(LOG_LEVEL_WARN, msg)
#define LOG_ERROR(msg) debugLogger.log(LOG_LEVEL_ERROR, msg)

// Formatted logging helpers
void logf(LogLevel level, const char* format, ...) {
  char buffer[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 160, format, args);
  va_end(args);
  debugLogger.log(level, buffer);
}

#define LOG_DEBUGF(fmt, ...) logf(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFOF(fmt, ...) logf(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNF(fmt, ...) logf(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERRORF(fmt, ...) logf(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif // DEBUGLOG_HPP
