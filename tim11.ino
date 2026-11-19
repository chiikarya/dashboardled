// Oleh: Slamet Indriyanto
// ESP32 sebagai CLIENT MQTT: menerima perintah LED dari broker EMQX (broker.emqx.io).
// Library yang dibutuhkan: PubSubClient (Nick O'Leary) -> Library Manager.
//
// Topic yang dipakai (GRUP = "tim4"):
//   kelas-08A/24/tim4/led26/set      <- ESP32 SUBSCRIBE (perintah "1" / "0")
//   kelas-08A/24/tim4/led27/set      <- ESP32 SUBSCRIBE
//   kelas-08A/24/tim4/led26/status   -> ESP32 PUBLISH   (kondisi LED sebenarnya)
//   kelas-08A/24/tim4/led27/status   -> ESP32 PUBLISH
//   kelas-08A/24/tim4/esp32/status   -> ESP32 PUBLISH   ("online"; "offline" lewat LWT)

#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid       = "saingannya onic";
const char* password   = "cirikara";

// Broker publik EMQX
const char* mqttServer = "broker.emqx.io";
const int   mqttPort   = 1883;

// Nama kelompok/tim. Setiap kelompok pakai nama berbeda supaya topic tidak bentrok.
const char* GRUP = "tim11";

const int led26 = 26;
const int led27 = 27;
int stateLed26 = 0;
int stateLed27 = 0;

char tSet26[64], tSet27[64];
char tStat26[64], tStat27[64];
char tOnline[64];
String clientId;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastReconnect = 0;
const unsigned long reconnectInterval = 3000;

void buatTopic() {
  snprintf(tSet26,  sizeof(tSet26),  "kelas-08A/29/%s/led26/set",    GRUP);
  snprintf(tSet27,  sizeof(tSet27),  "kelas-08A/29/%s/led27/set",    GRUP);
  snprintf(tStat26, sizeof(tStat26), "kelas-08A/29/%s/led26/status", GRUP);
  snprintf(tStat27, sizeof(tStat27), "kelas-08A/29/%s/led27/status", GRUP);
  snprintf(tOnline, sizeof(tOnline), "kelas-08A/29/%s/esp32/status", GRUP);
}

// Ubah LED, lalu laporkan kondisi sebenarnya ke broker (retained)
void setLed(int pin, int nilai) {
  digitalWrite(pin, nilai ? HIGH : LOW);
  if (pin == led26) {
    stateLed26 = nilai;
    client.publish(tStat26, nilai ? "1" : "0", true);
  } else {
    stateLed27 = nilai;
    client.publish(tStat27, nilai ? "1" : "0", true);
  }
  Serial.printf("LED%d=%d\n", pin, nilai);
}

// Dipanggil otomatis setiap ada pesan masuk dari topic yang di-subscribe
void callback(char* topic, byte* payload, unsigned int length) {
  String pesan;
  for (unsigned int i = 0; i < length; i++) pesan += (char)payload[i];
  pesan.trim();
  Serial.printf("Pesan masuk [%s] = %s\n", topic, pesan.c_str());

  int nilai = (pesan == "1") ? 1 : 0;

  // Bandingkan topic SEBELUM publish, karena buffer topic dipakai bersama
  if (strcmp(topic, tSet26) == 0)      setLed(led26, nilai);
  else if (strcmp(topic, tSet27) == 0) setLed(led27, nilai);
}

void connectMQTT() {
  Serial.print("Menghubungkan ke broker MQTT...");
  // connect(clientId, user, pass, willTopic, willQoS, willRetain, willMessage)
  // LWT: jika ESP32 putus mendadak, broker otomatis mengumumkan "offline"
  if (client.connect(clientId.c_str(), NULL, NULL, tOnline, 0, true, "offline")) {
    Serial.println("berhasil");
    client.publish(tOnline, "online", true);
    client.publish(tStat26, stateLed26 ? "1" : "0", true);
    client.publish(tStat27, stateLed27 ? "1" : "0", true);
    client.subscribe(tSet26, 1);
    client.subscribe(tSet27, 1);
    // Jika ada perintah terakhir yang tersimpan (retained), broker langsung mengirimnya sekarang
  } else {
    Serial.printf("gagal, rc=%d\n", client.state());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(led26, OUTPUT);
  pinMode(led27, OUTPUT);
  digitalWrite(led26, LOW);
  digitalWrite(led27, LOW);

  buatTopic();
  // clientId harus unik; pakai bagian dari MAC address
  clientId = "esp32-led-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  // Koneksi WiFi (hanya 2.4 GHz) dengan pencetakan status
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  int percobaan = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++percobaan % 20 == 0) {          // tiap 10 detik
      Serial.printf("\nStatus WiFi: %d, coba ulang\n", WiFi.status());
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    // Coba sambung ulang tiap 3 detik tanpa memblokir program
    if (millis() - lastReconnect >= reconnectInterval || lastReconnect == 0) {
      lastReconnect = millis();
      if (WiFi.status() == WL_CONNECTED) connectMQTT();
    }
  } else {
    client.loop();   // wajib dipanggil rutin: menerima pesan dan menjaga koneksi
  }
}