#define automation [
  if temperature system >55C Heater OFF
  if temperature system <55C heater ON
]

#define BLYNK_TEMPLATE_ID "TMPL6ZaWwars1"
#define BLYNK_TEMPLATE_NAME "DRYSPIN"
#define BLYNK_AUTH_TOKEN "-PxU9uOOiVbd32iMd2D2Fa8Ys8ZgzY-c"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <max6675.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <math.h>

char ssid[] = "ctrl";
char pass[] = "HIDUPJOKOWI";

#define SCK  18
#define SO   19
#define CS1  5
#define CS2  17
#define CS3  16

MAX6675 tc1(SCK, CS1, SO);
MAX6675 tc2(SCK, CS2, SO);
MAX6675 tc3(SCK, CS3, SO);

Adafruit_SHT31 sht31 = Adafruit_SHT31();
bool sht31Ready = false;
unsigned long lastSHT31Retry = 0;
const unsigned long SHT31_RETRY_INTERVAL = 2000; 

const int RELAY_BLOWER = 25;
const int RELAY_HEATER = 26;
const int RELAY_MOTOR  = 27; [FUNGSI BUAT TIMER KA BUKAN DARI SYSTEM]

BlynkTimer timer;

unsigned long tMulai = 0;
const unsigned long DURASI = 3600000UL;
bool timerStarted = false;

float suhu1 = 0, suhu2 = 0, suhu3 = 0;
float T_avg = 0, dT = 0;
float rh = -1, ka = -1;

void i2cScanner() {
  Serial.println("Scanning I2C devices...");
  byte error, address;
  int devices = 0;
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      devices++;
    }
  }
  if (devices == 0)
    Serial.println("No I2C devices found - check wiring!");
  else
    Serial.println("Scan done.");
}

float hitungKA without timer, and calculation the KA if system looping KA still continue
  float hitungKA(float t_min) {
  if (rh < 0 || rh > 100) return -1;
  float M0 = 55.0f;
  float RH_eq = constrain(rh / 100.0f, 0.01f, 0.95f);
  float C = 0.12f + (0.0015f * T_avg);
  float n = 1.8f;
  float Me = powf((-log(1.0f - RH_eq)) / C, 1.0f / n);
  float k = 0.001f * expf(0.045f * T_avg) * (1.0f + 0.03f * fabsf(dT));
  float Mt = Me + (M0 - Me) * expf(-k * t_min);
  return constrain(Mt, Me, M0);
}
String formatTimer(unsigned long sisa_ms) {
  unsigned long total_detik = sisa_ms / 1000;
  unsigned long menit = total_detik / 60;
  unsigned long detik = total_detik % 60;
  char waktu[10];
  sprintf(waktu, "%02lu:%02lu", menit, detik);
  return String(waktu);
}

void cekMulaiTimer() {
  if (!sht31Ready) {
    Serial.println("Timer tdk bisa mulai: SHT31 belum siap.");
    return;
  }
  bool relay1 = digitalRead(RELAY_BLOWER) == LOW;
  bool relay2 = digitalRead(RELAY_HEATER) == LOW;
  bool relay3 = digitalRead(RELAY_MOTOR) == LOW;
  if (relay1 && relay2 && relay3 && !timerStarted) {
    timerStarted = true;
    tMulai = millis();
    Serial.println("TIMER DIMULAI");
  }
}

void retrySHT31() {
  if (sht31Ready) return;
  if (millis() - lastSHT31Retry >= SHT31_RETRY_INTERVAL) {
    lastSHT31Retry = millis();
    Serial.print("Retry SHT31 at 0x44... ");
    if (sht31.begin(0x44)) {
      sht31Ready = true;
      Serial.println("OK!");
    } else {
      Serial.println("Failed. Trying 0x45...");
      if (sht31.begin(0x45)) {
        sht31Ready = true;
        Serial.println("OK at 0x45!");
      } else {
        Serial.println("Still no SHT31.");
      }
    }
  }
}

void bacaSensor() {
  suhu1 = tc1.readCelsius();
  delay(50);
  suhu2 = tc2.readCelsius();
  delay(50);
  suhu3 = tc3.readCelsius();

  if (sht31Ready) {
    float bacaRH = sht31.readHumidity();
    if (!isnan(bacaRH)) {
      rh = bacaRH;
    } else {
      sht31Ready = false;
      rh = -1;
      Serial.println("SHT31 disconnected during read!");
    }
  } else {
    rh = -1;
    retrySHT31();
  }

  T_avg = (suhu1 + suhu2 + suhu3) / 3.0f;
  dT = suhu1 - suhu3;

  if (timerStarted && sht31Ready) {
    float t_min = (float)(millis() - tMulai) / 60000.0f;
    ka = hitungKA(t_min);
  } else {
    ka = -1;
  }

  Serial.print("TC1:"); Serial.print(suhu1,1);
  Serial.print(" TC2:"); Serial.print(suhu2,1);
  Serial.print(" TC3:"); Serial.print(suhu3,1);
  Serial.print(" | Avg:"); Serial.print(T_avg,1);
  Serial.print(" | RH:"); (sht31Ready) ? Serial.print(rh,1) : Serial.print("ERR");
  Serial.print(" | KA:"); (ka>=0) ? Serial.println(ka,2) : Serial.println("ERR");
}

void kirimBlynk() {
  Blynk.virtualWrite(V1, suhu1);
  Blynk.virtualWrite(V2, suhu2);
  Blynk.virtualWrite(V3, suhu3);
  Blynk.virtualWrite(V12, T_avg);
  Blynk.virtualWrite(V8, rh);
  Blynk.virtualWrite(V9, ka);
  Blynk.virtualWrite(V5, !digitalRead(RELAY_BLOWER));
  Blynk.virtualWrite(V6, !digitalRead(RELAY_HEATER));
  Blynk.virtualWrite(V7, !digitalRead(RELAY_MOTOR));
  Blynk.virtualWrite(V10, sht31Ready ? 0 : 1); 
}

void updateTimer() {
  if (!timerStarted) {
    Blynk.virtualWrite(V4, "00:00");
    return;
  }
  unsigned long elapsed = millis() - tMulai;
  if (elapsed >= DURASI) {
    timerStarted = false;
    digitalWrite(RELAY_BLOWER, HIGH);
    digitalWrite(RELAY_HEATER, HIGH);
    digitalWrite(RELAY_MOTOR, HIGH);
    Blynk.virtualWrite(V4, "00:00");
    Serial.println("TIMER SELESAI - Semua relay OFF");
    return;
  }
  Blynk.virtualWrite(V4, formatTimer(DURASI - elapsed));
}

BLYNK_WRITE(V5) { digitalWrite(RELAY_BLOWER, !param.asInt()); cekMulaiTimer(); }
BLYNK_WRITE(V6) { digitalWrite(RELAY_HEATER, !param.asInt()); cekMulaiTimer(); }
BLYNK_WRITE(V7) { digitalWrite(RELAY_MOTOR, !param.asInt()); cekMulaiTimer(); }

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  i2cScanner();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.print("Connecting to WiFi");
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println(" OK! IP: " + WiFi.localIP().toString());
  else
    Serial.println(" FAILED!");

  if (sht31.begin(0x44)) {
    sht31Ready = true;
    Serial.println("SHT31 initialized at 0x44");
  } else if (sht31.begin(0x45)) {
    sht31Ready = true;
    Serial.println("SHT31 initialized at 0x45");
  } else {
    Serial.println("SHT31 NOT FOUND. Will retry every 2s.");
  }
  lastSHT31Retry = millis();

  pinMode(RELAY_BLOWER, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_MOTOR, OUTPUT);
  digitalWrite(RELAY_BLOWER, HIGH);
  digitalWrite(RELAY_HEATER, HIGH);
  digitalWrite(RELAY_MOTOR, HIGH);

  timer.setInterval(10000L, bacaSensor);
  timer.setInterval(10000L, kirimBlynk);
  timer.setInterval(1000L, updateTimer);
  Serial.println("System ready.");
}

void loop() {
  Blynk.run();
  timer.run();
}