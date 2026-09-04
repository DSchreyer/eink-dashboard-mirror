#include "epdif.h"
#include <SPI.h>

EpdIf::EpdIf() {
};

EpdIf::~EpdIf() {
};

void EpdIf::DigitalWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int EpdIf::DigitalRead(int pin) {
    return digitalRead(pin);
}

void EpdIf::DelayMs(unsigned int delaytime) {
    delay(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data) {
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(data);
    digitalWrite(CS_PIN, HIGH);
}

int EpdIf::IfInit(void) {
    pinMode(CS_PIN, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    pinMode(BUSY_PIN, INPUT);

    // No PWR_PIN toggle -- this all-in-one driver board handles panel
    // power internally, matching the proven-working ESPHome config which
    // never used one either.

    // Custom SCK/MOSI pins -- this board's panel SPI is wired to
    // GPIO13/GPIO14, not the ESP32 default VSPI pins (18/23). Matches
    // `spi: clk_pin: GPIO13, mosi_pin: GPIO14` in the proven-working
    // ESPHome config. MISO is unused (panel is write-only); CS is toggled
    // manually in SpiTransfer() above, so the 4th arg here is just SPI's
    // own idea of the default SS pin and isn't actually used as CS.
    SPI.begin(13, -1, 14, CS_PIN);
    // Confirmed 2MHz vs 500kHz makes no difference to solid-black
    // speckling -- rules out SPI signal integrity. Back to the
    // proven-working rate (matches ESPHome's spi::DATA_RATE_2MHZ).
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    return 0;
}
