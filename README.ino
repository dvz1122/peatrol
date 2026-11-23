/*
  ALAT PENJAGA SUHU TANAH GAMBUT
  Tugas alat: ukur suhu tanah. Kalau panas -> buzzer bunyi + kirim pesan ke HP.
*/

// ===== BAGIAN 1: AMBIL ALAT BANTU =====
// Bagian ini digunakan untuk memanggil alat bantu siap pakai, supaya NodeMCU bisa
// tersambung ke WiFi, mengirim data ke internet, dan membaca sensor suhu.
#include <ESP8266WiFi.h>        // alat untuk nyambung WiFi
#include <WiFiClientSecure.h>   // alat untuk kirim pesan dengan aman
#include <ESP8266HTTPClient.h>  // alat untuk buka alamat internet
#include <OneWire.h>            // alat untuk "ngobrol" dengan sensor
#include <DallasTemperature.h>  // alat untuk ubah data sensor jadi angka suhu


// ===== BAGIAN 2: ISIAN (cuma bagian ini yang perlu diganti) =====
// Bagian ini digunakan untuk mengisi data penting alat, seperti nama dan password hotspot,
// kunci bot Telegram, tujuan pesan, batas suhu aman, dan kunci grafik ThingSpeak.
const char* WIFI_SSID = "Dfs";       // nama hotspot HP
const char* WIFI_PASS = "davidsitorus5";   // password hotspot HP
const char* BOT_TOKEN = "9042578464:AAE2uRMS8iBJ_PR81nd46akMuXwxj52JelE";          // kunci bot Telegram
const char* CHAT_ID   = "1366941582";            // alamat tujuan pesan (orang atau grup)
const float BATAS_AMAN = 40.0;                // lebih dari 40 derajat = bahaya
const bool  BUZZER_LOW_AKTIF = true;          // setelan jenis buzzer, biarkan saja
const char* TS_API_KEY = "F18HXFLSMPYSHMAG"; // kunci untuk grafik ThingSpeak
const unsigned long INTERVAL_GRAFIK = 20000;  // kirim ke grafik tiap 20 detik


// ===== BAGIAN 3: KENALAN DENGAN SENSOR =====
// Bagian ini digunakan untuk memberi tahu bahwa sensor suhu dicolok di pin D2, serta menyiapkan
// catatan supaya pesan peringatan tidak dikirim berulang-ulang.
OneWire oneWire(D2);                 // sensor dicolok di lubang D2
DallasTemperature sensor(&oneWire);  // ini dia sensornya
bool sudahLapor = false;             // catatan: sudah kirim pesan belum?
unsigned long terakhirGrafik = 0;    // catatan: kapan terakhir kirim grafik


// ===== BAGIAN 4: TOMBOL BUZZER =====
// Bagian ini digunakan untuk menyalakan dan mematikan buzzer.
// buzzer(true) = bunyi.  buzzer(false) = diam.
void buzzer(bool bunyi) {
  digitalWrite(D5, (bunyi ^ BUZZER_LOW_AKTIF) ? HIGH : LOW);
}


// ===== BAGIAN 5: KIRIM PESAN KE TELEGRAM =====
// Bagian ini digunakan untuk mengirim pesan peringatan dari alat ke HP lewat Telegram.
void kirimTelegram(String teks) {
  if (WiFi.status() != WL_CONNECTED) {         // kalau WiFi putus...
    Serial.println(">> Telegram tidak terkirim: WiFi belum tersambung");
    return;                                    // ...batal kirim
  }
  teks.replace(" ", "%20");                    // ubah spasi supaya bisa dikirim
  teks.replace("\n", "%0A");                   // ubah ganti baris supaya bisa dikirim
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/sendMessage?chat_id=" + CHAT_ID + "&text=" + teks;   // tulis alamat surat
  http.begin(client, url);
  int kode = http.GET();                       // kirim suratnya!
  http.end();
  Serial.println(">> Hasil kirim Telegram, kode: " + String(kode));  // 200 = sampai
}


// ===== BAGIAN 6: KIRIM SUHU KE GRAFIK =====
// Bagian ini digunakan untuk mengirim data suhu ke ThingSpeak, supaya suhu bisa dilihat
// dalam bentuk grafik.
void kirimGrafik(float suhu, bool alarm) {
  if (WiFi.status() != WL_CONNECTED) {         // kalau WiFi putus, batal
    Serial.println(">> Grafik tidak terkirim: WiFi belum tersambung");
    return;
  }
  WiFiClient client;
  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=") + TS_API_KEY +
               "&field1=" + String(suhu, 1) + "&field2=" + String(alarm ? 1 : 0);
  http.begin(client, url);
  int kode = http.GET();                       // kirim angkanya!
  String balasan = http.getString();
  http.end();
  Serial.println(">> Grafik: kode " + String(kode) + ", data ke-" + balasan);
}


// ===== BAGIAN 7: BANGUN TIDUR (jalan sekali saat alat dinyalakan) =====
// Bagian ini digunakan untuk menyiapkan alat saat pertama kali dinyalakan: menyiapkan buzzer,
// menyalakan sensor, menyambung ke WiFi, lalu mengirim pesan bahwa alat sudah aktif.
void setup() {
  Serial.begin(115200);              // buka "layar obrolan" di laptop
  pinMode(D5, OUTPUT);               // siapkan buzzer
  buzzer(false);                     // buzzer diam dulu
  sensor.begin();                    // bangunkan sensor
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // coba nyambung ke hotspot
  Serial.print("\nMenyambung ke WiFi");
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);                      // tunggu sebentar...
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {   // kalau berhasil nyambung:
    Serial.println(" TERSAMBUNG");
    kirimTelegram("Alat pendeteksi suhu aktif. Batas aman: " + String(BATAS_AMAN, 0) + " C");  // bilang "aku sudah hidup!"
  } else {                               // kalau gagal:
    Serial.println(" GAGAL (cek nama/password hotspot, harus 2.4 GHz)");
  }
}


// ===== BAGIAN 8: KERJA TERUS (diulang-ulang tanpa henti) =====
// Bagian ini digunakan untuk menjalankan tugas utama alat secara terus-menerus: mengukur suhu,
// menampilkannya, mengirimnya ke grafik, lalu membunyikan buzzer dan mengirim peringatan
// jika suhu melewati batas aman.
void loop() {
  // 1) Ukur suhu
  sensor.requestTemperatures();
  float suhu = sensor.getTempCByIndex(0);

  // 2) Sensor tidak menjawab? Coba lagi nanti
  if (suhu == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor tidak terbaca (-127): cek kabel kuning & resistor");
    delay(1000);
    return;
  }
  Serial.println("Suhu: " + String(suhu, 1) + " C");   // tampilkan suhu di laptop

  // 3) Tiap 20 detik, kirim suhu ke grafik
  if (terakhirGrafik == 0 || millis() - terakhirGrafik >= INTERVAL_GRAFIK) {
    kirimGrafik(suhu, suhu > BATAS_AMAN);
    terakhirGrafik = millis();
  }

  // 4) Panas atau tidak?
  if (suhu > BATAS_AMAN) {
    // PANAS! Buzzer bunyi tit... tit... tit...
    buzzer(true);  delay(500);
    buzzer(false); delay(300);
    if (!sudahLapor) {                 // kalau belum kirim pesan, kirim sekarang (cukup sekali)
      kirimTelegram("PERINGATAN! Suhu tanah gambut " + String(suhu, 1) + " C, melebihi batas aman.\nSegera cek lokasi alat.");
      sudahLapor = true;
    }
  } else {
    // AMAN. Buzzer diam.
    buzzer(false);
    if (suhu < BATAS_AMAN - 3) sudahLapor = false;   // sudah dingin lagi, siap lapor kalau panas lagi
    delay(1000);                                     // istirahat 1 detik, lalu ulang dari atas
  }
}


/*KETERANGAN UMUM 
  ALARM SUHU GAMBUT + GRAFIK ONLINE (ThingSpeak)
  1 sensor suhu + buzzer + notifikasi Telegram + grafik suhu di ThingSpeak

  Papan   : NodeMCU ESP8266 V3 (Lolin, CH340)
            -> Arduino IDE, board "NodeMCU 1.0 (ESP-12E Module)"
  Library : OneWire, DallasTemperature (instal lewat Library Manager)

  Rangkaian TANPA SOLDER (breadboard mini + kabel jumper):
    DS18B20 merah  -> breadboard -> jumper -> pin "3V" NodeMCU (3,3 volt)
    DS18B20 hitam  -> breadboard -> jumper -> pin "G"  NodeMCU (ground)
    DS18B20 kuning -> breadboard -> jumper -> pin "D2" NodeMCU (data)
    Resistor 4,7 kOhm -> antara baris kabel merah dan baris kabel kuning
    Modul buzzer aktif: VCC -> 3V, GND -> G, I/O -> D5
    */
