// c++ -std=c++17 tests/host/pairing_test.cpp -o /tmp/roaster-pairing-test && /tmp/roaster-pairing-test
#include <cassert>
#include "../../src/network/PairingSessions.hpp"

int main() {
  assert(PairingSessions::pairedTimestamp(0, "1795899600") == 1795899600);
  assert(PairingSessions::pairedTimestamp(1795899600, "1795899599") == 1795899600);
  assert(PairingSessions::pairedTimestamp(0, nullptr) == 0);
  assert(PairingSessions::pairedTimestamp(0, "1795899600x") == 0);
  assert(PairingSessions::pairedTimestamp(0, "4294967296") == 0);
  PairingSessions state;
  PairingSessions::Session offer{}, session{};
  strcpy(offer.name, "Firmware CLI");
  offer.cli = true;
  offer.digest[0] = 23;
  uint8_t wrong[32]{};
  assert(!state.request(0, offer, 1234));
  state.open(10);
  assert(state.request(20, offer, 1234));
  assert(!state.request(21, offer, 1235));
  assert(!state.approve(22, 1235));
  assert(state.claim(23, offer.digest, session) == -1);
  assert(state.approve(24, 1234));
  assert(state.claim(25, wrong, session) == -1);
  assert(state.claim(26, offer.digest, session) == 0 && session.cli);
  session.digest[0] = 94;  // Runtime replaces the pairing ticket digest before persisting.
  state.sessions[0] = session;
  state.close();
  assert(state.claim(27, offer.digest, session) == -1);
  assert(state.lookup(session.digest, false) == -1);
  assert(state.lookup(session.digest, true) == 0);
  state.sessions[0] = {};
  assert(state.lookup(session.digest, true) == -1);
  state.open(UINT32_MAX - 100);
  assert(state.request(50, offer, 1234));
  assert(!state.approve(PairingSessions::WindowMs, 1234));
}
