#ifndef EPDIF_H
#define EPDIF_H

#include <Arduino.h>

// Pin definition -- Waveshare ESP32 Driver Board, matching the pinout
// already proven working in the ESPHome config this session (no PWR pin
// needed on this all-in-one board -- confirmed working without one for
// months of native-drawing tests).
#define RST_PIN         26
#define DC_PIN          27
#define CS_PIN          15
#define BUSY_PIN        25

class EpdIf {
public:
    EpdIf(void);
    ~EpdIf(void);

    static int  IfInit(void);
    static void DigitalWrite(int pin, int value);
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(unsigned char data);
};

#endif
