#include "MyMesh.h"
#include <cstring>

MyMesh::MyMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms,
               mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables)
  : mesh::Mesh(board, radio, ms, rng, rtc, tables),
    _fs(nullptr),
    next_local_advert(0),
    next_flood_advert(0),
    _logging(false),
    _cli(*this) {
  // blocking state defaults are already set in-class
}

void MyMesh::begin(FILESYSTEM* fs) {
  _fs = fs;
  Mesh::begin(fs);
}

// === Repeater-path blocking control ===
void MyMesh::setBlockedRepeaterKey(const uint8_t key[PUB_KEY_SIZE]) {
  memcpy(_blocked_repeater_key, key, PUB_KEY_SIZE);
  _block_enabled = true;
}

bool MyMesh::setBlockedRepeaterKeyHex(const char* hex, size_t len) {
  if (!hex) return false;
  if (len == 0) len = strlen(hex);
  if (len != PUB_KEY_SIZE * 2) return false;

  auto nyb = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };

  for (size_t i = 0; i < PUB_KEY_SIZE; ++i) {
    int hi = nyb(hex[2*i]);
    int lo = nyb(hex[2*i + 1]);
    if (hi < 0 || lo < 0) return false;
    _blocked_repeater_key[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  _block_enabled = true;
  return true;
}

void MyMesh::clearBlockedRepeaterKey() {
  memset(_blocked_repeater_key, 0, sizeof(_blocked_repeater_key));
  _block_enabled = false;
}

// === Enforcement: called by Mesh before forwarding ===
bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  // If no block configured, or invalid packet, use base policy
  if (!_block_enabled || !packet || packet->path_len == 0) {
    return mesh::Mesh::allowPacketForward(packet);
  }

  // path_len is bytes, must be multiple of PUB_KEY_SIZE
  if ((packet->path_len % PUB_KEY_SIZE) != 0) {
    return mesh::Mesh::allowPacketForward(packet);
  }

  const uint8_t* path_ptr = packet->path;
  size_t hop_count = packet->path_len / PUB_KEY_SIZE;

  for (size_t i = 0; i < hop_count; ++i) {
    const uint8_t* hop = path_ptr + i * PUB_KEY_SIZE;
    if (memcmp(hop, _blocked_repeater_key, PUB_KEY_SIZE) == 0) {
      if (_logging) {
        Serial.println("Blocked packet: repeater found in path.");
      }
      return false; // block forwarding
    }
  }

  return mesh::Mesh::allowPacketForward(packet);
}
