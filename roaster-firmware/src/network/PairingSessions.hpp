#pragma once

#include <stdint.h>
#include <string.h>

class PairingSessions {
public:
  static constexpr unsigned Capacity = 8;
  static constexpr uint32_t WindowMs = 60000;
  struct Session {
    uint8_t digest[32];
    uint32_t pairedAt;
    char name[33];
    bool cli;
  };
  Session sessions[Capacity]{};

  static uint32_t pairedTimestamp(int64_t deviceTime, const char *clientTime) {
    constexpr uint32_t earliest = 1600000000;
    if (deviceTime >= earliest && deviceTime <= UINT32_MAX) return deviceTime;
    uint64_t seconds = 0;
    for (const char *digit = clientTime; digit && *digit; ++digit) {
      if (*digit < '0' || *digit > '9') return 0;
      seconds = seconds * 10 + (*digit - '0');
      if (seconds > UINT32_MAX) return 0;
    }
    return seconds >= earliest ? seconds : 0;
  }

  void open(uint32_t now) {
    opened = now;
    enabled = true;
    pending = approved = false;
  }
  void close() { enabled = pending = approved = false; }
  bool isOpen(uint32_t now) const { return enabled && now - opened < WindowMs; }
  bool hasRequest(uint32_t now) const { return isOpen(now) && pending && !approved; }
  unsigned displayCode() const { return code; }
  bool request(uint32_t now, const Session &candidate, unsigned pairingCode) {
    if (!isOpen(now) || pending || freeSlot() < 0) return false;
    offered = candidate;
    code = pairingCode;
    pending = true;
    return true;
  }
  bool approve(uint32_t now, unsigned shownCode) {
    if (!hasRequest(now) || code != shownCode) return false;
    approved = true;
    return true;
  }
  int claim(uint32_t now, const uint8_t *ticket, Session &candidate) const {
    if (!isOpen(now) || !approved || !equal(ticket, offered.digest)) return -1;
    candidate = offered;
    return freeSlot();
  }
  bool isPending(uint32_t now, const uint8_t *ticket) const {
    return hasRequest(now) && equal(ticket, offered.digest);
  }
  int lookup(const uint8_t *digest, bool cli) const {
    for (unsigned i = 0; i < Capacity; ++i)
      if (sessions[i].name[0] && sessions[i].cli == cli && equal(sessions[i].digest, digest)) return i;
    return -1;
  }
  static bool equal(const uint8_t *a, const uint8_t *b) {
    unsigned diff = 0;
    for (unsigned i = 0; i < 32; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
  }
private:
  int freeSlot() const {
    for (unsigned i = 0; i < Capacity; ++i) if (!sessions[i].name[0]) return i;
    return -1;
  }
  Session offered{};
  uint32_t opened = 0;
  unsigned code = 0;
  bool enabled = false, pending = false, approved = false;
};
