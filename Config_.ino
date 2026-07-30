#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// 1. GENEL SERİ HABERLEŞME & BİLGİSAYAR (USB Serial Monitor)
// ============================================================================
#define SERIAL_BAUD         9600        // Bilgisayar / Seri port izleme hızı

// ============================================================================
// 2. BMS CAN BUS MODÜLÜ (MCP2515 - SPI HATTI)
// ============================================================================
// SPI Ortak Pinleri (Arduino Mega Donanımsal SPI):
// SCK  -> Pin 52 (SD Kart ile Paralel/Ortak)
// MISO -> Pin 50 (SD Kart ile Paralel/Ortak)
// MOSI -> Pin 51 (SD Kart ile Paralel/Ortak)
#define BMS_CAN_CS          53          // MCP2515 Özel Chip Select pini
#define BMS_CAN_INT         2           // MCP2515 Kesme (Interrupt) pini
#define CAN_OSC_FREQ        MCP_8MHZ    // MCP2515 kartı üzerindeki osilatör (8 MHz)
#define CAN_BUS_SPEED       CAN_250KBPS // Daly BMS fabrika çıkış CAN Bus hızı

// ============================================================================
// 3. LoRa TELEMETRİ MODÜLÜ (E22-900T22D - HARDWARE SERIAL 1)
// ============================================================================
#define LORA_SERIAL         Serial1     // Arduino Mega RX1 (Pin 19) / TX1 (Pin 18)
#define LORA_BAUD           9600        // LoRa fabrika çıkış UART hızı
#define LORA_M0             8           // LoRa Mod Seçim Pini M0
#define LORA_M1             9           // LoRa Mod Seçim Pini M1
#define TELEMETRY_SEND      1           // Saniye cinsinden paket gönderim periyodu (Şartname riski sıfırlandı)[cite: 1]

// ============================================================================
// 4. HMI / NEXTION DOKUNMATİK EKRAN (HARDWARE SERIAL 2)
// ============================================================================
#define DISPLAY_SERIAL      Serial2     // Arduino Mega RX2 (Pin 17) / TX2 (Pin 16)
#define DISPLAY_BAUD        9600        // Nextion ekran fabrika çıkış UART hızı

// ============================================================================
// 5. HARİCİ NTC SICAKLIK SENSÖRLERİ (ANALOG GİRİŞLER)
// ============================================================================
#define SICAKLIK_DS1        A0          // NTC Sensör 1 (10k NTC + 10k Sabit Direnç)
#define SICAKLIK_DS2        A1          // NTC Sensör 2 (10k NTC + 10k Sabit Direnç)
#define SICAKLIK_DS3        A2          // NTC Sensör 3 (10k NTC + 10k Sabit Direnç)

// ============================================================================
// 6. GÜVENLİK, RÖLE, BUZZER & KORUMA EŞİKLERİ
// ============================================================================
#define ROLE_PIN            22          // Batarya Kesici Röle / MOSFET Kontrol Pini
#define ALARM_PIN           23          // Sesli İkaz (Buzzer) Pini
#define ALARM_MAX           55          // °C cinsinden Sesli İkaz Başlangıç Eşiği[cite: 1, 2]
#define SICAKLIK_MAX        70          // °C cinsinden Röleyi Açma (Trip) Eşiği[cite: 1, 2]
#define LOST_MAX            60          // İletişim kopukluğunda güvenlik süresi (Saniye)[cite: 1]

// ============================================================================
// 7. MicroSD KART MODÜLÜ (SPI HATTI)
// ============================================================================
// SPI Ortak Pinleri (Arduino Mega Donanımsal SPI):
// SCK  -> Pin 52 (MCP2515 ile Paralel/Ortak)
// MISO -> Pin 50 (MCP2515 ile Paralel/Ortak)
// MOSI -> Pin 51 (MCP2515 ile Paralel/Ortak)
#define SD_MODULE_CS        4           // SD Kart Modülü Özel Chip Select pini

// ============================================================================
// 8. ARAÇ ANLIK DURUM OTOMATI (STATE MACHINE)
// ============================================================================
enum ActiveStatus {
  STATE_IDLE,       // Beklemede
  STATE_RUNNING,    // Normal Çalışma / Sürüş
  STATE_OBSTACLE,   // İkaz / Uyarı Durumu
  STATE_STOPPED,    // Güvenli Durdurma (Röle Açık)
  STATE_ERROR       // Kritik Hata
};

#endif // CONFIG_H
