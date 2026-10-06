#pragma once
// Sending a badge to a friend over the radio, a piece at a time. No Arduino code, so it can be checked on a
// computer (tools/meettest).
//
// Packets (after meet_logic.h's "UDX", version, type; from u32, to u32):
//   OFFER   size u32, crc32 u32, name length u8 + up to 31 bytes        -> the receiver answers ACK READY (or BUSY)
//   DATA    index u16, length u8, up to 200 bytes                      -> ACK index
//   ACK     index u16 (READY, BUSY or a piece's index)
//   VERDICT verdict u8 (KEPT, NO_THANKS, FAILED): what the friend did with it
// Stop-and-wait: the sender sends a piece and waits for its ACK, resending a few times before giving up. A badge
// (5.7 KB) is 29 pieces, about a second.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "meet_logic.h"

namespace send {

enum Type : uint8_t { OFFER = 7, DATA = 8, ACK = 9, VERDICT = 10 };
enum : uint16_t { READY = 0xFFFF, BUSY = 0xFFFE };
enum Verdict : uint8_t { KEPT = 1, NO_THANKS = 2, FAILED = 3 };
static const size_t PIECE = 200, MAX_SIZE = 16384, MAX_NAME = 31;
static const int TRIES = 12;               // sends of one piece before giving up
static const uint32_t ACK_WAIT_MS = 120;   // how long to wait for a piece's ACK before sending it again

// zlib's CRC-32, as the USB badge upload uses (esp_rom_crc32_le on the device).
inline uint32_t crc32(uint32_t crc, const uint8_t *p, size_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) crc = crc >> 1 ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

inline int pieces(size_t size) { return (int)((size + PIECE - 1) / PIECE); }

inline size_t start(uint8_t *out, Type t, uint32_t from, uint32_t to) {
  meet::header(out, (meet::Type)t);
  meet::put32(out + 5, from);
  meet::put32(out + 9, to);
  return 13;
}

struct Offer {
  uint32_t from, to, size, crc;
  char name[MAX_NAME + 1];
};

inline size_t encodeOffer(const Offer &o, uint8_t *out) {
  size_t n = start(out, OFFER, o.from, o.to);
  meet::put32(out + n, o.size);
  meet::put32(out + n + 4, o.crc);
  const size_t len = strnlen(o.name, MAX_NAME);
  out[n + 8] = len;
  memcpy(out + n + 9, o.name, len);
  return n + 9 + len;
}

inline bool decodeOffer(const uint8_t *p, size_t len, Offer &o) {
  if (meet::typeOf(p, len) != OFFER || len < 22 || p[21] > MAX_NAME || len != 22u + p[21]) return false;
  o.from = meet::get32(p + 5), o.to = meet::get32(p + 9);
  o.size = meet::get32(p + 13), o.crc = meet::get32(p + 17);
  memcpy(o.name, p + 22, p[21]);
  o.name[p[21]] = 0;
  return o.from && o.to && o.size > 0 && o.size <= MAX_SIZE;
}

inline size_t encodeData(uint32_t from, uint32_t to, uint16_t index, const uint8_t *data, size_t size, uint8_t *out) {
  size_t n = start(out, DATA, from, to);
  const size_t at = (size_t)index * PIECE, len = size - at < PIECE ? size - at : PIECE;
  out[n] = index & 0xFF, out[n + 1] = index >> 8, out[n + 2] = len;
  memcpy(out + n + 3, data + at, len);
  return n + 3 + len;
}

struct Piece {
  uint32_t from, to;
  uint16_t index;
  uint8_t len;
  const uint8_t *data;
};

inline bool decodeData(const uint8_t *p, size_t len, Piece &d) {
  if (meet::typeOf(p, len) != DATA || len < 16 || p[15] > PIECE || len != 16u + p[15]) return false;
  d.from = meet::get32(p + 5), d.to = meet::get32(p + 9);
  d.index = p[13] | p[14] << 8, d.len = p[15], d.data = p + 16;
  return d.from && d.to;
}

// ACK and VERDICT: one small number after from and to.
inline size_t encodeAck(uint32_t from, uint32_t to, uint16_t index, uint8_t *out) {
  size_t n = start(out, ACK, from, to);
  out[n] = index & 0xFF, out[n + 1] = index >> 8;
  return n + 2;
}
inline bool decodeAck(const uint8_t *p, size_t len, uint32_t &from, uint32_t &to, uint16_t &index) {
  if (meet::typeOf(p, len) != ACK || len != 15) return false;
  from = meet::get32(p + 5), to = meet::get32(p + 9), index = p[13] | p[14] << 8;
  return from && to;
}
inline size_t encodeVerdict(uint32_t from, uint32_t to, uint8_t verdict, uint8_t *out) {
  size_t n = start(out, VERDICT, from, to);
  out[n] = verdict;
  return n + 1;
}
inline bool decodeVerdict(const uint8_t *p, size_t len, uint32_t &from, uint32_t &to, uint8_t &verdict) {
  if (meet::typeOf(p, len) != VERDICT || len != 14) return false;
  from = meet::get32(p + 5), to = meet::get32(p + 9), verdict = p[13];
  return from && to && verdict >= KEPT && verdict <= FAILED;
}

// The receiving side: the pieces in order into `buf` (the caller's, at least `size` bytes).
class Receiver {
 public:
  uint8_t *buf = nullptr;
  uint32_t size = 0, crc = 0;
  int next = 0;  // the piece expected next

  void begin(uint8_t *into, uint32_t bytes, uint32_t crc32) { buf = into, size = bytes, crc = crc32, next = 0; }

  // A piece arrived: returns the index to acknowledge (this one, or an earlier one again if it was a repeat), or -1
  // to stay quiet (a piece from further ahead: the sender is waiting for the one before, and will resend it).
  int take(const Piece &d) {
    if (d.index < next) return d.index;  // a repeat: its ACK was lost, so acknowledge it again
    if (d.index > next || (size_t)d.index * PIECE + d.len > size) return -1;
    const size_t want = size - (size_t)d.index * PIECE < PIECE ? size - (size_t)d.index * PIECE : PIECE;
    if (d.len != want) return -1;
    memcpy(buf + (size_t)d.index * PIECE, d.data, d.len);
    next++;
    return d.index;
  }

  bool complete() const { return next == pieces(size); }
  bool intact() const { return complete() && crc32(0, buf, size) == crc; }
};

}  // namespace send
