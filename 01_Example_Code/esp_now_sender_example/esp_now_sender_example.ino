/*
  ESP-NOW sender example for LED_matrix_display_8x64_web.

  Set ESPNOW_CHANNEL to the channel shown in the receiver web UI.
  Open Serial Monitor at 115200 baud and use:
    S HELLO       immediate static text
    Q ALERT       stack static text for 3 seconds
    R RUNNING     immediate running text
    T NEXT INFO   stack running text for one complete pass
    TIME          switch receiver to clock mode
    CLEAR         clear the receiver display stack
*/

#include <Arduino.h>
#include <string.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <espnow.h>
  extern "C" {
    #include <user_interface.h>
  }
#elif defined(ESP32)
  #include <WiFi.h>
  #include <esp_now.h>
  #include <esp_wifi.h>
#else
  #error "Use an ESP8266 or ESP32 board."
#endif

static const uint8_t ESPNOW_CHANNEL = 1; // Match the receiver UI.
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint32_t ESPNOW_DISPLAY_MAGIC = 0x4C4D4553UL; // "LMES"
static const uint8_t ESPNOW_DISPLAY_VERSION = 1;
static const uint8_t ESPNOW_FLAG_STACK = 0x01;

enum EspNowDisplayCommand : uint8_t
{
  ESPNOW_SHOW_STATIC = 1,
  ESPNOW_SHOW_RUNNING = 2,
  ESPNOW_SHOW_TIME = 3,
  ESPNOW_CLEAR_STACK = 4
};

struct __attribute__((packed)) EspNowDisplayPacket
{
  uint32_t magic;
  uint8_t version;
  uint8_t command;
  uint8_t flags;
  uint8_t repeatCount;
  uint16_t speedMs;
  uint16_t durationMs;
  char text[96];
};

void setRadioChannel(uint8_t channel)
{
#if defined(ESP8266)
  wifi_set_channel(channel);
#else
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
#endif
}

bool sendPacket(uint8_t command, const char* value = "", bool stack = false,
                uint16_t speedMs = 60, uint16_t durationMs = 3000,
                uint8_t repeatCount = 1)
{
  EspNowDisplayPacket packet = {};
  packet.magic = ESPNOW_DISPLAY_MAGIC;
  packet.version = ESPNOW_DISPLAY_VERSION;
  packet.command = command;
  packet.flags = stack ? ESPNOW_FLAG_STACK : 0;
  packet.repeatCount = repeatCount;
  packet.speedMs = speedMs;
  packet.durationMs = durationMs;
  strncpy(packet.text, value, sizeof(packet.text) - 1);

#if defined(ESP8266)
  return esp_now_send((uint8_t*)BROADCAST_MAC, (uint8_t*)&packet, sizeof(packet)) == 0;
#else
  return esp_now_send(BROADCAST_MAC, (uint8_t*)&packet, sizeof(packet)) == ESP_OK;
#endif
}

bool beginEspNow()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  setRadioChannel(ESPNOW_CHANNEL);

#if defined(ESP8266)
  if (esp_now_init() != 0) return false;
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  return esp_now_add_peer((uint8_t*)BROADCAST_MAC, ESP_NOW_ROLE_SLAVE,
                          ESPNOW_CHANNEL, nullptr, 0) == 0;
#else
  if (esp_now_init() != ESP_OK) return false;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
#endif
}

void printHelp()
{
  Serial.println(F("Commands: S text | Q text | R text | T text | TIME | CLEAR"));
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(300);
  Serial.println();
  if (!beginEspNow())
  {
    Serial.println(F("ESP-NOW initialization failed."));
    return;
  }

  Serial.printf("Sender MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("ESP-NOW channel: %u\n", ESPNOW_CHANNEL);
  Serial.println(F("ESP-NOW sender ready."));
  printHelp();
}

void loop()
{
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  bool sent = false;
  if (line == F("TIME"))
    sent = sendPacket(ESPNOW_SHOW_TIME);
  else if (line == F("CLEAR"))
    sent = sendPacket(ESPNOW_CLEAR_STACK);
  else if (line.startsWith(F("S ")))
    sent = sendPacket(ESPNOW_SHOW_STATIC, line.substring(2).c_str(), false);
  else if (line.startsWith(F("Q ")))
    sent = sendPacket(ESPNOW_SHOW_STATIC, line.substring(2).c_str(), true, 60, 3000, 1);
  else if (line.startsWith(F("R ")))
    sent = sendPacket(ESPNOW_SHOW_RUNNING, line.substring(2).c_str(), false, 60, 3000, 1);
  else if (line.startsWith(F("T ")))
    sent = sendPacket(ESPNOW_SHOW_RUNNING, line.substring(2).c_str(), true, 60, 3000, 1);
  else
  {
    printHelp();
    return;
  }

  Serial.println(sent ? F("Packet queued for transmission.") : F("Send failed."));
}
