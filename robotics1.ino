#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Your Wi-Fi
const char* ssid = "CMF_Phone2";
const char* password = "NcmfPhone2";

// Telegram
#define BOT_TOKEN "8785585155:AAFZU9P7jul1-zfpX-q_W_zCXK24AQDqNiI"
#define CHAT_ID "5123901768"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

void setup() {
  Serial.begin(115200);

  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Allows ESP32 to connect to Telegram
  client.setInsecure();

  // Send test message
  bot.sendMessage(
    CHAT_ID,
    "Hello! ESP32 Telegram test is working.",
    ""
  );

  Serial.println("Telegram message sent!");
}

void loop() {
}