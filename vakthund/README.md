# Vakthund – overvaking med Discord-varsel

ESP32-S3-DevKitC-1 N16R8 som kvart minutt pingar ein vert (standard `192.168.1.225`) og prøver å kople til ein TCP-port (standard `22`).
- Ein runde tel som **feil** viss ping *eller* TCP-porten feilar.
- Etter **3 feil på rad** kjem **éi** Discord-melding.
- Så kjem ei **påminning kvar 6. time** så lenge verten er nede.
- Når verten svarar att kjem ei **«svarer att»-melding**.
- Er det ikkje WiFi på ESP-en sjølv, tel det ikkje som vertsfeil.

## Kopling
Ingen! Berre USB-kabel til PC eller ein USB-straumadapter. Ingen lodding, ingen ledningar.

## Oppsett (steg for steg)
1. Lag ein Discord-webhook: Serverinnstillingar -> Integrasjonar -> Webhooks -> *Ny webhook* -> kopier URL-en.
2. Kopier `include/secrets.example.h` til `include/secrets.h`.
3. Opne `secrets.h` og fyll inn WiFi-namn, passord og webhook-URL. (Bruk 2,4 GHz-WiFi.)
4. Vil du overvake ein annan vert/port? Fjern `//` framfor `WATCH_HOST` / `WATCH_PORT` i `secrets.h`.
5. Opne mappa `vakthund/` i VS Code med PlatformIO, trykk *Upload*, og opne *Serial Monitor* (115200).

`secrets.h` ligg i `.gitignore` og blir aldri lagt i git. Ikkje legg webhook-URL-en nokon annan stad.

## Merknad
Discord-tilkoplinga brukar HTTPS utan sertifikatsjekk (`setInsecure()`) for å halde det enkelt.
