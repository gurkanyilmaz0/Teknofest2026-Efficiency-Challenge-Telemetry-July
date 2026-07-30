#include <SoftwareSerial.h>

// ─────────────────────────────────────────
//  LORA PIN TANIMLAMALARI (Alıcı Tarafı - Uno/Nano)
// ─────────────────────────────────────────
// Arduino Uno/Nano: RX = Pin 8 (LoRa TX'e), TX = Pin 5 (LoRa RX'e)
SoftwareSerial loraSerial(8, 5); 
const int M0_PIN = 7;
const int M1_PIN = 6;

// Gelen verileri geçici olarak saklayacağımız tampon bellek (Buffer)
String gelenSatir = ""; 

void setup() {
  // Python arayüzü ile USB Serial Monitor haberleşme hızı
  Serial.begin(9600);     
  Serial.setTimeout(50); // Python okuma bloklanmasını önlemek için timeout 50ms yapıldı
  
  // SoftwareSerial üzerinden LoRa haberleşme hızı
  loraSerial.begin(9600);   
  
  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);
  
  // E22 Modülünü Normal Modda (Mode 0) başlat (M0=0, M1=0)
  digitalWrite(M0_PIN, LOW);
  digitalWrite(M1_PIN, LOW);
  
  delay(500); // Modülün toparlanması için kısa bekleme
  Serial.println("ALICI: Sistem Hazir, Veri Bekleniyor...");
}

void loop() {
  // 1. ARAÇTAN LORA İLE GELEN VERİ DİNLENİR
  while (loraSerial.available()) {
    char c = loraSerial.read();
    
    // Satır sonu karakterini (\n) görene kadar veriyi biriktir
    if (c == '\n') {
      gelenSatir.trim(); // Varsa başındaki/sonundaki boşlukları temizle
      
      // Güvenlik Kontrolü: Paket boş değilse ve '$' işaretiyle başlıyorsa
      if (gelenSatir.length() > 0 && gelenSatir.startsWith("$")) {
        
        // A. Paketi USB (Serial) üzerinden Python Arayüzüne ilet
        Serial.println(gelenSatir); 

        // B. LoRa modülünün RX->TX mod geçişi için güvenli bekleme
        delay(30);

        // C. Araç Tarafına (Arduino Mega'ya) "Sinyal Sağlam" Onayı (ACK) Gönder
        loraSerial.println("ACK");
      }
      
      gelenSatir = ""; // Tampon belleği yeni paket için temizle
    } 
    else if (c != '\r') {
      // Satır sonu karakteri haricindekileri tampon belleğe ekle
      gelenSatir += c;
    }
  }

  // 2. PYTHON YAZILIMINDAN BİR YANIT / KOMUT GELİRSE ARACA YOLLA
  if (Serial.available() > 0) {
    String pythonGelen = Serial.readStringUntil('\n');
    pythonGelen.trim();
    if (pythonGelen.length() > 0) {
      loraSerial.println(pythonGelen);
    }
  }
}
