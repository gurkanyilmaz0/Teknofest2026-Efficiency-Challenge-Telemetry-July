#include <SPI.h>
#include <SD.h>
#include <math.h>
#include "config.h"

// ============================================================================
// GÜVENLİK VE SİSTEM DEĞİŞKENLERİ
// ============================================================================
unsigned long baslangicZamani = 0;

// Zamanlayıcılar (Millis Timer)
unsigned long sonSaniyeZamani       = 0;
unsigned long sonNextionVeriZamani  = 0;
unsigned long sonLoRaGonderimZamani = 0;
unsigned long sonSDZamani           = 0;
unsigned long sonAlarmZamani        = 0;
unsigned long sonAnonsZamani        = 0;

// Sinyal Kopukluk ve Röle Kilitleme (Latch) Bayrakları
bool baglantiKoptuMu = false; 
bool roleKilitlendi  = false; 

// Anlık Araç ve Telemetri Verileri
int g_hiz                 = 0;     // km/h
float g_sicaklik          = 0.0;   // °C (En yüksek hücre sıcaklığı)
float g_gerilim           = 0.0;   // V
float g_akim              = 0.0;   // A
float g_max_cell_v        = 0.0;   // V (Maksimum Hücre Gerilimi)
float g_min_cell_v        = 0.0;   // V (Minimum Hücre Gerilimi)
int g_soc                 = 100;   // %
long g_kalan_enerji       = 1000;  // Wh

// SD Kart Dosya Değişkenleri
File dataFile;
String fileName;

// ============================================================================
// YARDIMCI FONKSİYONLAR
// ============================================================================

// 10k NTC Sensör Sıcaklık Hesabı (Steinhart-Hart B-Equation)
float ntcOku(int analogPin) {
  int rawADC = analogRead(analogPin);
  if (rawADC == 0 || rawADC >= 1023) return -127.0; // Hata/Bağlantı yok
  
  float R_ntc = 10000.0 * (1023.0 / (float)rawADC - 1.0);
  float tempK = 1.0 / ( (1.0 / 298.15) + (1.0 / 3950.0) * log(R_ntc / 10000.0) );
  return tempK - 273.15; // Kelvin -> Celcius
}

// 3 Adet NTC Sensörünün En Yüksek Sıcaklığını Bularak Dönen Fonksiyon
float enYuksekSicaklikOku() {
  float t1 = ntcOku(SICAKLIK_DS1);
  float t2 = ntcOku(SICAKLIK_DS2);
  float t3 = ntcOku(SICAKLIK_DS3);
  
  float maxT = -127.0;
  if (t1 > maxT) maxT = t1;
  if (t2 > maxT) maxT = t2;
  if (t3 > maxT) maxT = t3;
  return maxT;
}

// Nextion Komut Bitirme Bayrağı (3x 0xFF)
void terminateNextion() {
  DISPLAY_SERIAL.write(0xFF);
  DISPLAY_SERIAL.write(0xFF);
  DISPLAY_SERIAL.write(0xFF);
}

// Nextion Sayısal Değer Gönderme (Tamsayı)
void sendNextionVal(String obj, int val) {
  DISPLAY_SERIAL.print(obj + "=" + String(val));
  terminateNextion();
}

// Nextion Metin Gönderme
void sendNextionTxt(String obj, String txt) {
  DISPLAY_SERIAL.print(obj + "=\"" + txt + "\"");
  terminateNextion();
}

// Nextion Xfloat (Ondalıklı Değer) Gönderme (Örn: 3.85V -> 385 integer gönderilir)
void sendNextionXfloat(String obj, float val, int vvs0 = 2) {
  int intVal = (int)(val * pow(10, vvs0));
  sendNextionVal(obj + ".val", intVal);
}

// ============================================================================
// SETUP (İLK KURULUM)
// ============================================================================
void setup() {
  // Donanımsal Portlar (Arduino Mega 2560)
  Serial.begin(SERIAL_BAUD);          // USB Serial Monitor
  LORA_SERIAL.begin(LORA_BAUD);       // Hardware Serial1 (Pin 18/19)
  DISPLAY_SERIAL.begin(DISPLAY_BAUD); // Hardware Serial2 (Pin 16/17)

  // Serial Timeout Süresini 50ms'ye Çek (Donma Riski İptal Edildi)
  LORA_SERIAL.setTimeout(50);

  // Pin Yönlendirmeleri
  pinMode(ROLE_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  // SPI Çakışmasını Önlemek İçin CS Pinleri Yüksek Duruma Çekilir
  pinMode(SD_MODULE_CS, OUTPUT);
  digitalWrite(SD_MODULE_CS, HIGH);
  pinMode(BMS_CAN_CS, OUTPUT);
  digitalWrite(BMS_CAN_CS, HIGH);

  // LoRa Normal Mod (M0=0, M1=0)
  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);

  // Başlangıç Röle ve Buzzer Durumları (Röle İletimde/Çekili, Buzzer Kapalı)
  digitalWrite(ROLE_PIN, HIGH);
  digitalWrite(ALARM_PIN, LOW);

  baslangicZamani = millis();
  sonAnonsZamani = baslangicZamani;

  // Açılış Buzzer Bip Sesi
  tone(ALARM_PIN, 1000); delay(150); noTone(ALARM_PIN);

  // MicroSD Kart Başlatma & Otomatik Dosya İsmi (AY0.CSV, AY1.CSV...)
  if (SD.begin(SD_MODULE_CS)) {
    for (int i = 0; i < 1000; i++) {
      fileName = "AY" + String(i) + ".CSV";
      if (!SD.exists(fileName)) break;
    }
  }

  sendNextionTxt("t0.txt", "SISTEM HAZIR");
}

// ============================================================================
// LOOP (ANA DÖNGÜ)
// ============================================================================
void loop() {
  unsigned long suankiMs = millis();

  // --------------------------------------------------------------------------
  // 1. SİNYAL VE HABERLEŞME KONTROLÜ (ŞARTNAME TEKNİK KONTROL REVİZYONU)
  // --------------------------------------------------------------------------
  if (LORA_SERIAL.available() > 0) {
    String gelen = LORA_SERIAL.readStringUntil('\n');
    if (gelen.indexOf("ACK") >= 0) {
      sonAnonsZamani = suankiMs;
      baglantiKoptuMu = false; // Yer istasyonundan yanıt geldi, araç içi SD kaydı DURDUR[cite: 1]
    }
  }

  // 3 saniyeden uzun süre ACK gelmezse haberleşme koptu kabul et ve SD kaydı BAŞLAT[cite: 1]
  if (suankiMs - sonAnonsZamani > 3000) {
    baglantiKoptuMu = true;
  }

  // --------------------------------------------------------------------------
  // 2. SENSÖR OKUMALARI VE VERİ İŞLEME
  // --------------------------------------------------------------------------
  g_sicaklik = enYuksekSicaklikOku();
  if (g_sicaklik < -50) g_sicaklik = 0; // Sensör hatası koruması

  // --------------------------------------------------------------------------
  // 3. EKRAN KRONOMETRESİ (1 SANİYEDE BİR)
  // --------------------------------------------------------------------------
  if (suankiMs - sonSaniyeZamani >= 1000) {
    sonSaniyeZamani = suankiMs;
    unsigned long toplamSn = (suankiMs - baslangicZamani) / 1000;
    char kronoTam[10];
    sprintf(kronoTam, "%02d:%02d:%02d", (int)(toplamSn / 3600), (int)((toplamSn % 3600) / 60), (int)(toplamSn % 60));
    sendNextionTxt("t4.txt", String(kronoTam));
  }

  // --------------------------------------------------------------------------
  // 4. ARAÇ ÜSTÜ SD KART KAYDI (SADECE HABERLEŞME KOPUKKEN ÇALIŞIR)[cite: 1]
  // --------------------------------------------------------------------------
  if (baglantiKoptuMu && (suankiMs - sonSDZamani >= 1000)) {
    sonSDZamani = suankiMs;
    
    dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      // Şartnamede belirtilen CSV başlık yapısı (Sadece kesinti anları yazılır)[cite: 1]
      if (dataFile.size() == 0) {
        dataFile.println("zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh");[cite: 1]
      }
      
      // ŞARTNAME FORMAT REVİZYONU: T_bat_C miliderece (x1000) cinsinden yazılmalı[cite: 1]
      long t_bat_milli = (long)(g_sicaklik * 1000.0);
      
      dataFile.print(suankiMs - baslangicZamani); dataFile.print(";");
      dataFile.print(g_hiz);                       dataFile.print(";");
      dataFile.print(t_bat_milli);                 dataFile.print(";");
      dataFile.print((long)g_gerilim);              dataFile.print(";");
      dataFile.println(g_kalan_enerji);
      dataFile.close();
    }
  }

  // --------------------------------------------------------------------------
  // 5. NEXTION EKRAN GÜNCELLEME (300 ms)
  // --------------------------------------------------------------------------
  if (suankiMs - sonNextionVeriZamani >= 300) {
    sonNextionVeriZamani = suankiMs;
    
    int sicBar = constrain((int)(g_sicaklik * 100.0 / 80.0), 0, 100);

    // Hız, Sıcaklık ve Batarya Barı
    sendNextionVal("n0.val", g_hiz);
    sendNextionVal("z0.val", g_hiz);
    sendNextionVal("n1.val", (int)g_sicaklik);
    sendNextionVal("j1.val", sicBar);
    
    // Voltaj, Akım ve SOC
    sendNextionVal("n2.val", (int)g_gerilim);
    sendNextionXfloat("x2", g_akim, 2);       // Akım (A) -> x2
    sendNextionVal("j0.val", g_soc);
    sendNextionTxt("t1.txt", "%" + String(g_soc));
    
    // Hücre Voltajları ve Kalan Enerji
    sendNextionXfloat("x0", g_max_cell_v, 2); // Maks Hücre Voltajı -> x0
    sendNextionXfloat("x1", g_min_cell_v, 2); // Min Hücre Voltajı -> x1
    sendNextionVal("n5.val", (int)g_kalan_enerji);
  }

  // --------------------------------------------------------------------------
  // 6. LORA TELEMETRİ GÖNDERİMİ (1 SANİYEDE BİR)[cite: 1]
  // --------------------------------------------------------------------------
  if (suankiMs - sonLoRaGonderimZamani >= ((unsigned long)TELEMETRY_SEND * 1000)) {
    sonLoRaGonderimZamani = suankiMs;

    // ŞARTNAME FORMAT REVİZYONU: Sıcaklık x1000 ile gönderiliyor[cite: 1]
    long t_bat_milli = (long)(g_sicaklik * 1000.0);

    String paket = "$" + String(suankiMs - baslangicZamani) + ";" + 
                   String(g_hiz) + ";" + 
                   String(t_bat_milli) + ";" + 
                   String((int)g_gerilim) + ";" + 
                   String(g_kalan_enerji);

    LORA_SERIAL.println(paket);
    Serial.println("Telemetri Gönderildi: " + paket);
  }

  // --------------------------------------------------------------------------
  // 7. GÜVENLİK, ALARM VE MÜHÜRLÜ KESİCİ RÖLE KONTROLÜ[cite: 1, 2]
  // --------------------------------------------------------------------------
  if (suankiMs - sonAlarmZamani >= 200) {
    sonAlarmZamani = suankiMs;

    // A. KRİTİK SICAKLIK: 70°C VE ÜZERİ (SİSTEMİ KAPAT & MÜHÜRLE)[cite: 1, 2]
    if (g_sicaklik >= SICAKLIK_MAX || roleKilitlendi) {
      roleKilitlendi = true;       // Soğusa dahi kapalı kalsın (Latch)
      digitalWrite(ROLE_PIN, LOW); // Batarya Kesici Röleyi Aç / Enerjiyi Kes[cite: 1, 2]
      tone(ALARM_PIN, 2000);       // Kesintisiz Acil Durum Düdüğü
      sendNextionTxt("t0.txt", "TEHLIKE: TRIP (70C)");
    } 
    // B. UYARI DURUMU: SICAKLIK >= 55°C (ŞARTNAME ZORUNLU UYARI)[cite: 1, 2]
    else if (g_sicaklik >= ALARM_MAX) {
      digitalWrite(ROLE_PIN, HIGH); // Röle İletimde Kalmaya Devam Etsin
      tone(ALARM_PIN, 1000);        // Sesli İkaz (Buzzer)[cite: 1, 2]
      sendNextionTxt("t0.txt", "UYARI: YUKSEK SICAKLIK!");
    } 
    // C. SİSTEM NORMAL
    else {
      digitalWrite(ROLE_PIN, HIGH); // Röle İletimde
      noTone(ALARM_PIN);            // Alarm Kapalı
      sendNextionTxt("t0.txt", "SISTEM NORMAL");
    }
  }
}
