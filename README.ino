# peatrol
/*
  ALARM SUHU GAMBUT + GRAFIK ONLINE (ThingSpeak)
  1 sensor suhu + buzzer + notifikasi Telegram + grafik suhu di ThingSpeak
  Rangkaian SAMA PERSIS dengan versi sederhana, tidak ada kabel tambahan.

  Papan   : NodeMCU ESP8266 V3 (Lolin, CH340)
            -> Arduino IDE, board "NodeMCU 1.0 (ESP-12E Module)"
            (juga bisa Wemos D1 mini -> board "LOLIN(WEMOS) D1 R2 & mini")
  Library : OneWire, DallasTemperature (instal lewat Library Manager)

  Rangkaian TANPA SOLDER (pakai breadboard mini + kabel jumper):
    DS18B20 merah  -> baris A breadboard -> jumper -> pin "3V" NodeMCU (keluaran 3,3 volt)
    DS18B20 hitam  -> baris B breadboard -> jumper -> G NodeMCU
    DS18B20 kuning -> baris C breadboard -> jumper -> D2 NodeMCU
    Resistor 4.7k  -> antara baris A dan baris C
    Modul buzzer aktif 3 pin: VCC -> 3V, GND -> G, I/O -> D5
    (kebanyakan modul berbunyi saat I/O = LOW; jika buzzer Anda terbalik
     -- bunyi terus saat suhu normal -- ubah BUZZER_LOW_AKTIF menjadi false)
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- UBAH BAGIAN INI ----------------
const char* WIFI_SSID = "NamaHotspot";
const char* WIFI_PASS = "PasswordHotspot";
const char* BOT_TOKEN = "123456:ABC-token-dari-BotFather";
const char* CHAT_ID   = "123456789";
const float BATAS_AMAN = 40.0;     // suhu (°C) di atas ini = alarm (demo: 40, lapangan: 50)
const bool  BUZZER_LOW_AKTIF = true;
const char* TS_API_KEY = "ISI_WRITE_API_KEY";   // Write API Key dari ThingSpeak
const unsigned long INTERVAL_GRAFIK = 20000;   // kirim ke grafik tiap 20 detik (gratis: min 15 detik)
// -------------------------------------------------

OneWire oneWire(D2);
DallasTemperature sensor(&oneWire);
bool sudahLapor = false;
unsigned long terakhirGrafik = 0;

void buzzer(bool bunyi) {
  digitalWrite(D5, (bunyi ^ BUZZER_LOW_AKTIF) ? HIGH : LOW);
}

void kirimTelegram(String teks) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(">> Telegram tidak terkirim: WiFi belum tersambung");
    return;
  }
  teks.replace(" ", "%20");
  teks.replace("\n", "%0A");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/sendMessage?chat_id=" + CHAT_ID + "&text=" + teks;
  http.begin(client, url);
  int kode = http.GET();
  http.end();
  // 200 = berhasil, 401 = token salah, 400 = chat ID salah / bot belum di-/start
  Serial.println(">> Hasil kirim Telegram, kode: " + String(kode));
}

// Kirim suhu (field1) dan status alarm (field2: 0 = aman, 1 = alarm) ke ThingSpeak
void kirimGrafik(float suhu, bool alarm) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(">> Grafik tidak terkirim: WiFi belum tersambung");
    return;
  }
  WiFiClient client;
  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=") + TS_API_KEY +
               "&field1=" + String(suhu, 1) + "&field2=" + String(alarm ? 1 : 0);
  http.begin(client, url);
  int kode = http.GET();
  String balasan = http.getString();
  http.end();
  // balasan = nomor data (1, 2, 3, ...) jika berhasil; "0" jika API Key salah / terlalu cepat
  Serial.println(">> Grafik: kode " + String(kode) + ", data ke-" + balasan);
}

void setup() {
  Serial.begin(115200);
  pinMode(D5, OUTPUT);
  buzzer(false);
  sensor.begin();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("\nMenyambung ke WiFi");
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" TERSAMBUNG");
    kirimTelegram("Alat pendeteksi suhu aktif. Batas aman: " + String(BATAS_AMAN, 0) + " C");
  } else {
    Serial.println(" GAGAL (cek nama/password hotspot, harus 2.4 GHz)");
  }
}

void loop() {
  sensor.requestTemperatures();
  float suhu = sensor.getTempCByIndex(0);
  if (suhu == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor tidak terbaca (-127): cek kabel kuning & resistor");
    delay(1000);
    return;
  }
  Serial.println("Suhu: " + String(suhu, 1) + " C");

  if (terakhirGrafik == 0 || millis() - terakhirGrafik >= INTERVAL_GRAFIK) {
    kirimGrafik(suhu, suhu > BATAS_AMAN);
    terakhirGrafik = millis();
  }

  if (suhu > BATAS_AMAN) {
    // bunyi putus-putus
    buzzer(true);  delay(500);
    buzzer(false); delay(300);
    if (!sudahLapor) {      // kirim notifikasi sekali tiap kejadian
      kirimTelegram("PERINGATAN! Suhu tanah gambut " + String(suhu, 1) + " C, melebihi batas aman.\nSegera cek lokasi alat.");
      sudahLapor = true;
    }
  } else {
    buzzer(false);
    if (suhu < BATAS_AMAN - 3) sudahLapor = false;   // reset setelah suhu turun
    delay(1000);
  }
}
