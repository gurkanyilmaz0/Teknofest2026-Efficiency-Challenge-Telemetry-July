#include <math.h>

const int NTC_PIN = A0;

// 1kΩ direnç kullandığımız için 1000.0 yazıyoruz
const float R_REF = 1000.0; 
const float R_0 = 10000.0;   // NTC'nin 25°C'deki direnci (10k)
const float T_0 = 298.15;    // 25°C (Kelvin)
const float B_VALUE = 3950;  // Standart Beta katsayısı

void setup() {
  Serial.begin(9600);
  Serial.println("NTC Testi (1k Pull-Down) Başladı...");
}

void loop() {
  int analogVal = analogRead(NTC_PIN);

  if (analogVal > 0 && analogVal < 1023) {
    float vOut = analogVal * (5.0 / 1023.0);
    
    // NTC 5V'a, 1k direnç GND'ye bağlıysa direnç hesabı:
    float rNtc = R_REF * ((5.0 / vOut) - 1.0);

    // Steinhart-Hart Denklemi
    float tempK = 1.0 / ((1.0 / T_0) + (1.0 / B_VALUE) * log(rNtc / R_0));
    float tempC = tempK - 273.15;

    Serial.print("Analog: ");
    Serial.print(analogVal);
    Serial.print(" | Sıcaklık: ");
    Serial.print(tempC);
    Serial.println(" °C");
  } else {
    Serial.println("Bağlantı Hatası!");
  }

  delay(1000);
}
