#ifndef DEBUGLOG_HPP
#define DEBUGLOG_HPP

#include <Arduino.h>
#include <mutex>

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
  LogEntry lastError;
  uint32_t errorSequence;
  // Logged from the main loop, async web task and SystemLink tasks concurrently.
  mutable std::mutex lock;
  
public:
  DebugLogger() : writeIndex(0), count(0), lastError{}, errorSequence(0) {}
  
  // Add a log entry
  void log(LogLevel level, const char* message) {
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = level;
    strncpy(entry.message, message, 159);
    entry.message[159] = '\0';

    {
      std::lock_guard<std::mutex> guard(lock);
      logs[writeIndex] = entry;
      writeIndex = (writeIndex + 1) % MAX_LOGS;
      if (count < MAX_LOGS) count++;
      if (level == LOG_LEVEL_ERROR) {
        lastError = entry;
        errorSequence++;
      }
    }
    
    #ifdef DEBUG
    printLogEntry(entry);
    #endif
  }

  // Returns a sequence number that changes whenever a new ERROR entry is logged.
  uint32_t getLastError(LogEntry &out) const {
    std::lock_guard<std::mutex> guard(lock);
    out = lastError;
    return errorSequence;
  }

  // Plain-text dump of recent entries at or above minLevel, newest last, capped at maxChars.
  String getRecentText(int maxEntries, LogLevel minLevel, size_t maxChars) const {
    std::lock_guard<std::mutex> guard(lock);
    String text;
    int entriesToScan = min(maxEntries, count);
    int startIndex = (writeIndex - entriesToScan + MAX_LOGS) % MAX_LOGS;
    for (int i = 0; i < entriesToScan; i++) {
      const LogEntry &entry = logs[(startIndex + i) % MAX_LOGS];
      if (entry.level < minLevel) continue;
      String line = String(entry.timestamp) + " " + getLevelName(entry.level) + " " + entry.message + "\n";
      if (text.length() + line.length() > maxChars) break;
      text += line;
    }
    return text;
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
    std::lock_guard<std::mutex> guard(lock);
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
      for (int j = 0; j < 160 && logs[index].message[j] != '\0'; j++) {
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
    std::lock_guard<std::mutex> guard(lock);
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
