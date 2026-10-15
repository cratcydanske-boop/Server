# Mottakstest – sjekk Ali-delane

Testfastvare for **ESP32-S3-DevKitC-1 N16R8**. Han testar éin del om gongen og skriv `[OK]` eller `[FEIL]` på seriemonitoren.

## Kva treng du
- ESP32-S3-DevKitC-1 N16R8 og USB-kabel (bruk USB-kontakten merkt **UART**)
- Breadboard og jumperkablar (hunn-hunn eller hann-hunn, alt etter modulen). Kjøp modular med pinnar ferdig lodda, så slepp du å lodde.

## Køyre
1. Installer VS Code + utvidinga *PlatformIO IDE*.
2. Opne mappa `mottakstest/` og trykk *Upload*.
3. Opne *Serial Monitor* (115200). Skriv eit tal og trykk Enter:

| Tast | Test |
|------|------|
| 1 | I2C-skann (finn alle I2C-einingar) |
| 2 | MPU6500 – `WHO_AM_I` skal vere **0x70**. **0x68 = feilmerka MPU6050** |
| 3 | 2× VL53L1X (TOF400C) |
| 4 | 10DOF-modul |
| 5 | HC-SR04 |
| a | alle |

## Kopling (pinne -> pinne)
Alle I2C-delar deler dei same to ledningane (SDA og SCL), og får 3,3 V.

**I2C-bussen (felles for MPU6500, begge TOF og 10DOF)**
```
ESP32 GPIO8  -> SDA på alle modular
ESP32 GPIO9  -> SCL på alle modular
ESP32 3V3    -> VCC/VIN på alle modular
ESP32 GND    -> GND på alle modular
```

**MPU6500**
```
MPU VCC -> ESP32 3V3     MPU GND -> ESP32 GND
MPU SDA -> GPIO8         MPU SCL -> GPIO9
MPU AD0 -> GND (adresse 0x68)
```

**2× VL53L1X (TOF400C)** – begge har adresse 0x29, så kvar får ein eigen XSHUT-ledning. Programmet gir dei adressane 0x30 og 0x31.
```
TOF1 VCC -> 3V3   TOF1 GND -> GND   TOF1 SDA -> GPIO8   TOF1 SCL -> GPIO9
TOF1 XSHUT -> GPIO4
TOF2 VCC -> 3V3   TOF2 GND -> GND   TOF2 SDA -> GPIO8   TOF2 SCL -> GPIO9
TOF2 XSHUT -> GPIO5
```

**10DOF-modul**
```
VCC -> 3V3   GND -> GND   SDA -> GPIO8   SCL -> GPIO9
```
Modulen kan bruke 0x68 som MPU6500 → **kopla frå MPU6500 når du testar 10DOF** (eller omvendt). Programmet kjenner att brikker frå adressene: 0x68/0x69 (gyro/akselerometer), 0x1E eller 0x0D (kompass HMC5883L/QMC5883L), 0x76/0x77 (trykk BMP280/BME280/BMP180/BMP388).

**HC-SR04 – OBS: ekko-signalet er 5 V, ESP32 toler berre 3,3 V!**
Set inn ein spenningsdelar på ECHO-ledninga med to motstandar (1 kΩ og 2 kΩ):
```
ESP32 5V   -> HC-SR04 VCC
ESP32 GND  -> HC-SR04 GND
ESP32 GPIO6 -> HC-SR04 TRIG        (3,3 V frå ESP32 er nok)

HC-SR04 ECHO -> 1 kΩ -+-> ESP32 GPIO7
                      |
                     2 kΩ
                      |
                     GND
```
Ut frå delaren kjem 5 V × 2/3 ≈ 3,3 V. Hopp ikkje over dette – det kan øydeleggje kortet.

## Pinnar vi med vilje ikkje brukar
GPIO 35–37 (octal PSRAM), 0/3/45/46 (strapping), 19/20 (USB).

## Tips
- Etter TOF-testen er adressane endra til 0x30/0x31 til du trykkjer **RST** på kortet.
- `[FEIL] ... ingen eining funnen`: sjekk at SDA/SCL ikkje er bytta og at GND er kopla.
