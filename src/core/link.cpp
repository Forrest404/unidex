#include "link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/queue.h>
#include "net.h"
#include "power.h"

static const uint8_t CHANNEL = 1;  // every unidex listens on the same channel
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static QueueHandle_t queue;  // packets from the WiFi task to the main loop
static bool on;

// The receive callback on this core doesn't give the signal strength, so the radio also listens to every frame
// (promiscuous mode, management frames only) and notes the strength of each ESP-NOW frame by its sender. Both
// callbacks run in the WiFi task, the sniffer first, so no lock is needed.
struct Strength {
  uint8_t mac[6];
  int8_t rssi;
};
static Strength strengths[8];
static uint8_t nextStrength;

static void sniffed(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
  const uint8_t *f = pkt->payload;
  // An action frame (0xD0), vendor specific (127) from Espressif (18:FE:34): an ESP-NOW frame.
  if (pkt->rx_ctrl.sig_len < 28 || f[0] != 0xD0 || f[24] != 127 || f[25] != 0x18 || f[26] != 0xFE || f[27] != 0x34)
    return;
  const uint8_t *from = f + 10;
  for (Strength &s : strengths)
    if (memcmp(s.mac, from, 6) == 0) {
      s.rssi = pkt->rx_ctrl.rssi;
      return;
    }
  Strength &s = strengths[nextStrength++ % 8];
  memcpy(s.mac, from, 6);
  s.rssi = pkt->rx_ctrl.rssi;
}

static int8_t strengthOf(const uint8_t *mac) {
  for (const Strength &s : strengths)
    if (memcmp(s.mac, mac, 6) == 0) return s.rssi;
  return 0;
}

// Runs in the WiFi task: copy the packet out and return straight away.
static void received(const uint8_t *mac, const uint8_t *data, int len) {
  if (!queue || len <= 0 || len > (int)sizeof LinkPacket::data) return;
  LinkPacket p;
  memcpy(p.mac, mac, 6);
  p.rssi = strengthOf(mac);
  p.at = millis();
  p.len = len;
  memcpy(p.data, data, len);
  xQueueSend(queue, &p, 0);  // full: dropped, like a packet lost in the air
}

static bool addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peer) == ESP_ERR_ESPNOW_FULL) {  // 20 at most: forget the others, keep broadcast
    esp_now_peer_info_t p;
    for (bool first = true; esp_now_fetch_peer(first, &p) == ESP_OK; first = false)
      if (memcmp(p.peer_addr, BROADCAST, 6)) esp_now_del_peer(p.peer_addr);
    return esp_now_add_peer(&peer) == ESP_OK;
  }
  return esp_now_is_peer_exist(mac);
}

const char *linkStart() {
  if (on) return nullptr;
  if (netClaimed()) return "WiFi busy: a note is sending";
  if (!queue) queue = xQueueCreate(16, sizeof(LinkPacket));
  netClaim(true);
  powerHold();  // light sleep would turn the radio off
  setCpuFrequencyMhz(240);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep misses packets
  uint8_t mac[6];
  esp_fill_random(mac, sizeof mac);
  mac[0] = (mac[0] & 0xFC) | 0x02;  // locally administered, single device
  esp_wifi_set_mac(WIFI_IF_STA, mac);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK || esp_now_register_recv_cb(received) != ESP_OK || !addPeer(BROADCAST)) {
    esp_now_deinit();
    netOff();
    netClaim(false);
    powerRelease();
    return "The radio didn't start";
  }
  memset(strengths, 0, sizeof strengths);
  const wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(sniffed);
  esp_wifi_set_promiscuous(true);
  xQueueReset(queue);
  on = true;
  return nullptr;
}

void linkStop() {
  if (!on) return;
  on = false;
  esp_wifi_set_promiscuous(false);
  esp_now_deinit();
  netOff();
  netClaim(false);
  powerRelease();
}

bool linkOn() { return on; }

bool linkBroadcast(const uint8_t *data, size_t len) {
  return on && len <= ESP_NOW_MAX_DATA_LEN && esp_now_send(BROADCAST, data, len) == ESP_OK;
}

bool linkSend(const uint8_t mac[6], const uint8_t *data, size_t len) {
  return on && len <= ESP_NOW_MAX_DATA_LEN && addPeer(mac) && esp_now_send(mac, data, len) == ESP_OK;
}

bool linkPoll(LinkPacket &out) { return on && queue && xQueueReceive(queue, &out, 0) == pdTRUE; }
