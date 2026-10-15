// Mottakstest for Ali-delar – ESP32-S3-DevKitC-1 N16R8.
// Seriemonitor 115200. Send ein teikn for å velje test (sjå meny).
#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

// ---- Pinnar (ingen av 0/3/19/20/35-37/45/46) ----
constexpr int PIN_SDA = 8;
constexpr int PIN_SCL = 9;
constexpr int PIN_XSHUT1 = 4;  // TOF #1
constexpr int PIN_XSHUT2 = 5;  // TOF #2
constexpr int PIN_TRIG = 6;    // HC-SR04
constexpr int PIN_ECHO = 7;    // HC-SR04 (via spenningsdelar!)

constexpr uint8_t TOF_ADDR1 = 0x30;
constexpr uint8_t TOF_ADDR2 = 0x31;

static int feil = 0;  // teljar for FEIL i gjeldande køyring

static void ok(const char *namn, const String &info = "") {
  Serial.printf("  [OK]   %s %s\n", namn, info.c_str());
}
static void feilmld(const char *namn, const String &info = "") {
  Serial.printf("  [FEIL] %s %s\n", namn, info.c_str());
  feil++;
}

// ---------- I2C-hjelparar ----------
static bool finst(uint8_t adr) {
  Wire.beginTransmission(adr);
  return Wire.endTransmission() == 0;
}
static int lesReg(uint8_t adr, uint8_t reg) {
  Wire.beginTransmission(adr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)adr, 1) != 1) return -1;
  return Wire.read();
}
static String hex(int v) {
  char b[8];
  snprintf(b, sizeof b, "0x%02X", v);
  return String(b);
}

// ---------- 1: I2C-skann ----------
static void testSkann() {
  Serial.println("== I2C-skann (SDA=GPIO8, SCL=GPIO9) ==");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (finst(a)) {
      Serial.printf("  fann eining på %s\n", hex(a).c_str());
      n++;
    }
  }
  if (n > 0) ok("I2C-skann", String(n) + " eining(ar)");
  else feilmld("I2C-skann", "ingen eining funnen – sjekk SDA/SCL, 3V3 og GND");
}

// ---------- 2: MPU6500 ----------
static void testMpu() {
  Serial.println("== MPU6500 ==");
  uint8_t adr = 0;
  if (finst(0x68)) adr = 0x68;
  else if (finst(0x69)) adr = 0x69;
  if (!adr) {
    feilmld("MPU6500", "ingen svar på 0x68/0x69");
    return;
  }
  int id = lesReg(adr, 0x75);  // WHO_AM_I
  Serial.printf("  WHO_AM_I = %s på adresse %s\n", hex(id).c_str(), hex(adr).c_str());
  if (id == 0x70) ok("MPU6500", "(ekte MPU6500)");
  else if (id == 0x68) feilmld("MPU6500", "WHO_AM_I=0x68 => dette er ein MPU6050 (feilmerka vare!)");
  else if (id == 0x71 || id == 0x73) feilmld("MPU6500", "ser ut som MPU9250/9255, ikkje MPU6500");
  else feilmld("MPU6500", "uventa WHO_AM_I " + hex(id));
  if (id < 0) return;
  // Vakn opp og les rådata som ekstra sanity-sjekk
  Wire.beginTransmission(adr);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(adr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  if (Wire.requestFrom((int)adr, 6) == 6) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    Serial.printf("  akselerasjon rå: x=%d y=%d z=%d (ca 16384 = 1 g på ein akse)\n", ax, ay, az);
    if (ax == 0 && ay == 0 && az == 0) feilmld("MPU6500 data", "berre nullar");
    else ok("MPU6500 data");
  } else {
    feilmld("MPU6500 data", "kunne ikkje lese");
  }
}

// ---------- 3: 2x VL53L1X ----------
static VL53L1X tof1, tof2;

static bool startTof(VL53L1X &s, int xshut, uint8_t nyAdr, const char *namn) {
  digitalWrite(xshut, HIGH);
  delay(10);
  s.setTimeout(500);
  if (!s.init()) {
    feilmld(namn, "init feila (ingen svar på 0x29?)");
    return false;
  }
  s.setAddress(nyAdr);
  s.setDistanceMode(VL53L1X::Long);
  s.setMeasurementTimingBudget(50000);
  s.startContinuous(50);
  return true;
}

static void testTof() {
  Serial.println("== 2x VL53L1X (TOF400C) ==");
  pinMode(PIN_XSHUT1, OUTPUT);
  pinMode(PIN_XSHUT2, OUTPUT);
  digitalWrite(PIN_XSHUT1, LOW);
  digitalWrite(PIN_XSHUT2, LOW);
  delay(10);
  bool a = startTof(tof1, PIN_XSHUT1, TOF_ADDR1, "TOF #1");
  bool b = startTof(tof2, PIN_XSHUT2, TOF_ADDR2, "TOF #2");
  if (a && finst(TOF_ADDR1)) ok("TOF #1", "ny adresse " + hex(TOF_ADDR1));
  else if (a) feilmld("TOF #1", "adresseendring feila");
  if (b && finst(TOF_ADDR2)) ok("TOF #2", "ny adresse " + hex(TOF_ADDR2));
  else if (b) feilmld("TOF #2", "adresseendring feila");
  for (int i = 0; i < 5; i++) {
    delay(100);
    if (a) {
      uint16_t mm = tof1.read();
      if (tof1.timeoutOccurred()) feilmld("TOF #1 måling", "timeout");
      else Serial.printf("  TOF #1: %u mm\n", mm);
    }
    if (b) {
      uint16_t mm = tof2.read();
      if (tof2.timeoutOccurred()) feilmld("TOF #2 måling", "timeout");
      else Serial.printf("  TOF #2: %u mm\n", mm);
    }
  }
  if (a) ok("TOF #1 målingar");
  if (b) ok("TOF #2 målingar");
  Serial.println("  (Trykk reset på kortet før du køyrer test 2/4 igjen om adressar er endra.)");
}

// ---------- 4: 10DOF ----------
static void sjekkAdr(uint8_t adr, int &funn) {
  if (!finst(adr)) return;
  funn++;
  String namn;
  switch (adr) {
    case 0x68: {
      int id = lesReg(0x68, 0x75), id2 = lesReg(0x68, 0x00);
      if (id == 0x68) namn = "MPU6050 (gyro+aks)";
      else if (id == 0x70) namn = "MPU6500 (gyro+aks)";
      else if (id == 0x71) namn = "MPU9250 (gyro+aks+kompass)";
      else if (id2 == 0x68) namn = "ITG3205 (gyro)";
      else namn = "ukjend på 0x68 (WHO_AM_I=" + hex(id) + ")";
      break;
    }
    case 0x69: {
      int id = lesReg(0x69, 0x0F), id2 = lesReg(0x69, 0x75);
      if (id == 0xD3) namn = "L3G4200D (gyro)";
      else if (id2 == 0x68) namn = "MPU6050 (AD0=høg)";
      else namn = "ukjend på 0x69 (0x0F=" + hex(id) + ", 0x75=" + hex(id2) + ")";
      break;
    }
    case 0x1E: {
      bool h = lesReg(0x1E, 0x0A) == 'H' && lesReg(0x1E, 0x0B) == '4' && lesReg(0x1E, 0x0C) == '3';
      namn = h ? "HMC5883L (kompass)" : "kompass-lik eining på 0x1E";
      break;
    }
    case 0x0D: namn = "QMC5883L (kompass)"; break;
    case 0x53: namn = lesReg(0x53, 0x00) == 0xE5 ? "ADXL345 (akselerometer)" : "ukjend på 0x53"; break;
    case 0x76:
    case 0x77: {
      int id = lesReg(adr, 0xD0);
      if (id == 0x58) namn = "BMP280 (trykk)";
      else if (id == 0x60) namn = "BME280 (trykk+fukt)";
      else if (id == 0x55) namn = "BMP180 (trykk)";
      else if (id == 0x50) namn = "BMP388 (trykk)";
      else if (id == 0x61) namn = "BME680 (gass)";
      else namn = "trykksensor? chip-ID " + hex(id);
      break;
    }
    default: namn = "ikkje ein kjend 10DOF-del"; break;
  }
  Serial.printf("  %s => %s\n", hex(adr).c_str(), namn.c_str());
}

static void test10dof() {
  Serial.println("== 10DOF-modul ==");
  Serial.println("  Tips: kopla frå MPU6500 først viss modulen òg bruker 0x68.");
  const uint8_t kandidatar[] = {0x68, 0x69, 0x1E, 0x0D, 0x53, 0x76, 0x77};
  int funn = 0;
  for (uint8_t a : kandidatar) sjekkAdr(a, funn);
  if (funn >= 3) ok("10DOF", String(funn) + " adresser (gyro/aks + kompass + trykk forventa)");
  else if (funn > 0) feilmld("10DOF", "berre " + String(funn) + " adresse(r); modulen skal ha gyro/aks, kompass og trykk");
  else feilmld("10DOF", "ingen kjende adresser");
}

// ---------- 5: HC-SR04 ----------
static void testHcsr04() {
  Serial.println("== HC-SR04 (TRIG=GPIO6, ECHO=GPIO7 via spenningsdelar) ==");
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  delay(50);
  int gyldige = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    unsigned long t = pulseIn(PIN_ECHO, HIGH, 30000UL);
    if (t == 0) {
      Serial.println("  ingen ekko");
    } else {
      float cm = t * 0.0343f / 2.0f;
      Serial.printf("  %.1f cm\n", cm);
      if (cm >= 2 && cm <= 400) gyldige++;
    }
    delay(100);
  }
  if (gyldige >= 3) ok("HC-SR04", String(gyldige) + "/5 gyldige målingar");
  else feilmld("HC-SR04", "berre " + String(gyldige) + "/5 gyldige – sjekk 5 V til VCC og spenningsdelaren");
}

static void meny() {
  Serial.println();
  Serial.println("===== MOTTAKSTEST =====");
  Serial.println(" 1  I2C-skann");
  Serial.println(" 2  MPU6500");
  Serial.println(" 3  2x VL53L1X (TOF400C)");
  Serial.println(" 4  10DOF-modul");
  Serial.println(" 5  HC-SR04");
  Serial.println(" a  Alle testane");
  Serial.println(" m  Vis meny");
  Serial.print("Val> ");
}

static void kjor(char c) {
  feil = 0;
  Serial.println(c);
  switch (c) {
    case '1': testSkann(); break;
    case '2': testMpu(); break;
    case '3': testTof(); break;
    case '4': test10dof(); break;
    case '5': testHcsr04(); break;
    case 'a':
      testSkann(); testMpu(); testTof(); test10dof(); testHcsr04();
      Serial.printf("== Samandrag: %d feil ==\n", feil);
      break;
    case 'm': case '?': break;
    default: Serial.println("Ukjend val."); break;
  }
  meny();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  pinMode(PIN_XSHUT1, OUTPUT);
  pinMode(PIN_XSHUT2, OUTPUT);
  digitalWrite(PIN_XSHUT1, HIGH);  // stå ope til TOF-testen
  digitalWrite(PIN_XSHUT2, HIGH);
  meny();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') return;
    kjor(c);
  }
}
