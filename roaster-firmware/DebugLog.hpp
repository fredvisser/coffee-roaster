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
  char message[80];  // Fixed size for predictable memory usage
};

// Ring buffer for log entries
class DebugLogger {
private:
  static const int MAX_LOGS = 100;
  LogEntry logs[MAX_LOGS];
  int writeIndex;
  int count;
  
public:
  DebugLogger() : writeIndex(0), count(0) {}
  
  // Add a log entry
  void log(LogLevel level, const char* message) {
    logs[writeIndex].timestamp = millis();
    logs[writeIndex].level = level;
    strncpy(logs[writeIndex].message, message, 79);
    logs[writeIndex].message[79] = '\0';  // Ensure null termination
    
    writeIndex = (writeIndex + 1) % MAX_LOGS;
    if (count < MAX_LOGS) count++;
    
    // Also print to Serial for debugging
    #ifdef DEBUG
    printLogEntry(logs[(writeIndex - 1 + MAX_LOGS) % MAX_LOGS]);
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
    
    int entriesToReturn = min(maxEntries, count);
    int startIndex = (writeIndex - entriesToReturn + MAX_LOGS) % MAX_LOGS;
    
    for (int i = 0; i < entriesToReturn; i++) {
      int index = (startIndex + i) % MAX_LOGS;
      
      if (i > 0) json += ",";
      
      json += "{";
      json += "\"timestamp\":" + String(logs[index].timestamp) + ",";
      json += "\"level\":\"" + String(getLevelName(logs[index].level)) + "\",";
      json += "\"message\":\"";
      
      // Escape special characters in message
      for (int j = 0; j < 80 && logs[index].message[j] != '\0'; j++) {
        char c = logs[index].message[j];
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
    writeIndex = 0;
    count = 0;
  }
  
  // Get log count
  int getCount() const {
    return count;
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
  char buffer[80];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 80, format, args);
  va_end(args);
  debugLogger.log(level, buffer);
}

#define LOG_DEBUGF(fmt, ...) logf(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFOF(fmt, ...) logf(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNF(fmt, ...) logf(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERRORF(fmt, ...) logf(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif // DEBUGLOG_HPP
