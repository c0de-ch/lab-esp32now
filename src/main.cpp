#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define BUTTON_PIN  0   // BOOT button on most ESP32 dev boards
#define LED_PIN     2   // Onboard LED on most ESP32 dev boards
#define BLINK_COUNT 3
#define BLINK_MS    200
#define DEBOUNCE_MS 300

static const uint8_t BROADCAST_ADDR[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum MsgType : uint8_t {
    MSG_BLINK = 0x01,
};

struct __attribute__((packed)) Message {
    MsgType type;
    uint8_t sender[6];
};

static volatile bool shouldBlink = false;
static uint8_t myMac[6];

static void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(Message)) return;

    const Message *msg = reinterpret_cast<const Message *>(data);
    if (msg->type != MSG_BLINK) return;

    // Ignore messages from ourselves
    if (memcmp(msg->sender, myMac, 6) == 0) return;

    shouldBlink = true;
}

static void blinkLed() {
    for (int i = 0; i < BLINK_COUNT; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(BLINK_MS);
        digitalWrite(LED_PIN, LOW);
        delay(BLINK_MS);
    }
}

static void sendBlink() {
    Message msg;
    msg.type = MSG_BLINK;
    memcpy(msg.sender, myMac, 6);
    esp_now_send(BROADCAST_ADDR, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Init WiFi in station mode (required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.channel(1);

    WiFi.macAddress(myMac);
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        ESP.restart();
    }

    // Register broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    // Register receive callback
    esp_now_register_recv_cb(onReceive);

    Serial.println("Ready. Press BOOT button to blink all others.");
}

void loop() {
    // Check button (active LOW)
    static unsigned long lastPress = 0;
    if (digitalRead(BUTTON_PIN) == LOW) {
        unsigned long now = millis();
        if (now - lastPress > DEBOUNCE_MS) {
            lastPress = now;
            Serial.println("Button pressed — sending blink");
            sendBlink();
            // Brief LED flash on sender as confirmation
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
        }
    }

    // Blink if requested by another board
    if (shouldBlink) {
        shouldBlink = false;
        Serial.println("Blink received");
        blinkLed();
    }
}
