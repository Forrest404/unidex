// Checks sending a badge to a friend, piece by piece, on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/pet tools/meettest/send_test.cpp -o /tmp/send_test && /tmp/send_test
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "send_logic.h"

using namespace send;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// A whole transfer between two simulated devices, losing each packet with chance `loss` (in %) and sometimes
// delivering one twice. Returns true if the receiver ends with an intact copy.
static bool transfer(const std::vector<uint8_t> &file, int loss, uint32_t seed, int &sends) {
  srand(seed);
  auto lost = [&] { return rand() % 100 < loss; };
  std::vector<uint8_t> into(file.size());
  Receiver r;
  r.begin(into.data(), file.size(), crc32(0, file.data(), file.size()));
  uint8_t pkt[256];
  sends = 0;
  for (int i = 0; i < pieces(file.size()); i++) {
    bool acked = false;
    for (int tries = 0; tries < TRIES && !acked; tries++) {
      const size_t n = encodeData(1, 2, i, file.data(), file.size(), pkt);
      sends++;
      if (lost()) continue;
      Piece d;
      if (!decodeData(pkt, n, d)) return false;
      int ack = r.take(d);
      if (rand() % 10 == 0) ack = r.take(d);  // delivered twice
      if (ack < 0 || lost()) continue;        // no ACK, or the ACK got lost
      uint8_t a[16];
      const size_t an = encodeAck(2, 1, ack, a);
      uint32_t f, t;
      uint16_t idx;
      acked = decodeAck(a, an, f, t, idx) && idx == i;
    }
    if (!acked) return false;
  }
  return r.intact();
}

int main() {
  check(crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926, "the CRC is zlib's (as the USB upload uses)");
  check(pieces(5662) == 29 && pieces(200) == 1 && pieces(201) == 2, "a 5.7 KB badge is 29 pieces of 200 bytes");

  uint8_t p[256];
  Offer o = {11, 22, 5662, 0xABCD1234, "03-hello.bmp"}, back;
  size_t n = encodeOffer(o, p);
  check(decodeOffer(p, n, back) && back.size == 5662 && back.crc == 0xABCD1234 && strcmp(back.name, "03-hello.bmp") == 0,
        "an OFFER survives encoding and decoding");
  o.size = MAX_SIZE + 1;
  n = encodeOffer(o, p);
  check(!decodeOffer(p, n, back), "an offer over 16 KB is refused");
  o.size = 0;
  n = encodeOffer(o, p);
  check(!decodeOffer(p, n, back), "an empty offer is refused");
  std::vector<uint8_t> file(5662);
  for (size_t i = 0; i < file.size(); i++) file[i] = (uint8_t)(i * 7 + 3);
  n = encodeData(11, 22, 28, file.data(), file.size(), p);
  Piece d;
  check(decodeData(p, n, d) && d.index == 28 && d.len == 5662 - 28 * 200 && n <= 250, "the last piece is the rest, and fits in a packet");
  uint32_t f, t;
  uint8_t v;
  n = encodeVerdict(22, 11, KEPT, p);
  check(decodeVerdict(p, n, f, t, v) && v == KEPT, "a VERDICT survives encoding and decoding");

  int sends;
  check(transfer(file, 0, 1, sends) && sends == 29, "no losses: 29 pieces, 29 sends, an intact copy");
  bool allOk = true;
  int worst = 0;
  for (uint32_t seed = 1; seed <= 200; seed++) {
    allOk &= transfer(file, 20, seed, sends);
    worst = sends > worst ? sends : worst;
  }
  check(allOk, "a fifth of packets lost and some repeated: still an intact copy every time (200 runs)");
  printf("       (worst case %d sends for 29 pieces)\n", worst);

  // A damaged byte is caught by the CRC.
  std::vector<uint8_t> into(file.size());
  Receiver r;
  r.begin(into.data(), file.size(), crc32(0, file.data(), file.size()));
  for (int i = 0; i < pieces(file.size()); i++) {
    n = encodeData(1, 2, i, file.data(), file.size(), p);
    if (i == 5) p[20] ^= 1;
    decodeData(p, n, d);
    r.take(d);
  }
  check(r.complete() && !r.intact(), "a piece damaged on the way: complete, but the CRC catches it");
  Receiver ahead;
  ahead.begin(into.data(), file.size(), 0);
  n = encodeData(1, 2, 3, file.data(), file.size(), p);
  decodeData(p, n, d);
  check(ahead.take(d) == -1 && ahead.next == 0, "a piece from further ahead is ignored");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}
