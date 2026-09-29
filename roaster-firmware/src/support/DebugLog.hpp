#ifndef DEBUGLOG_HPP
#define DEBUGLOG_HPP

#include <Arduino.h>

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
  LogLevel level;
  char message[160];  // Keep bounded but large enough for HTTP/API diagnostics
};

// Ring buffer for log entries
class DebugLogger {
private:
  static const int MAX_LOGS = 100;
  LogEntry logs[MAX_LOGS];
  int writeIndex;
  int count;
  portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
  
public:
  DebugLogger() : writeIndex(0), count(0) {}
  
  // Add a log entry
  void log(LogLevel level, const char* message) {
    LogEntry entry;
    portENTER_CRITICAL(&lock);
    entry.timestamp = millis();
    entry.level = level;
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    logs[writeIndex] = entry;
    writeIndex = (writeIndex + 1) % MAX_LOGS;
    if (count < MAX_LOGS) count++;
    portEXIT_CRITICAL(&lock);
    
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
    int entriesToReturn;
    int startIndex;
    portENTER_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
    entriesToReturn = min(maxEntries, count);
    startIndex = (writeIndex - entriesToReturn + MAX_LOGS) % MAX_LOGS;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
    
    for (int i = 0; i < entriesToReturn; i++) {
      int index = (startIndex + i) % MAX_LOGS;
      LogEntry entry;
      portENTER_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
      entry = logs[index];
      portEXIT_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
      
      if (i > 0) json += ",";
      
      json += "{";
      json += "\"timestamp\":" + String(entry.timestamp) + ",";
      json += "\"level\":\"" + String(getLevelName(entry.level)) + "\",";
      json += "\"message\":\"";
      
      // Escape special characters in message
      for (int j = 0; j < sizeof(entry.message) && entry.message[j] != '\0'; j++) {
        char c = entry.message[j];
        if (c == '"' || c == '\\') json += '\\';
        json += c;
      }
      
      json += "\"}";
    }
    
    json += wrapInObject ? "]}" : "]";
    return json;
  }
  
  // Clear all logs
  void clear() {
    portENTER_CRITICAL(&lock);
    writeIndex = 0;
    count = 0;
    portEXIT_CRITICAL(&lock);
  }
  
  // Get log count
  int getCount() const {
    portENTER_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
    int result = count;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE *>(&lock));
    return result;
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
