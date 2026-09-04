/**
 *  @filename   :   epd7in5.cpp
 *  @brief      :   Implements for e-paper library
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd7in5_V2.h"

unsigned char Voltage_Frame_7IN5_V2[]={
	0x6, 0x3F, 0x3F, 0x11, 0x24, 0x7, 0x17,
};

unsigned char LUT_VCOM_7IN5_V2[]={	
	0x0,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x0,	0xF,	0x1,	0xF,	0x1,	0x2,	
	0x0,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
};						

unsigned char LUT_WW_7IN5_V2[]={	
	0x10,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x84,	0xF,	0x1,	0xF,	0x1,	0x2,	
	0x20,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
};

unsigned char LUT_BW_7IN5_V2[]={	
	0x10,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x84,	0xF,	0x1,	0xF,	0x1,	0x2,	
	0x20,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
};

// Frame-count nibbles here (positions 5, 11, 17) corrected to 0x3/0x4/0x3
// to match the proven-working ESPHome "7.50inv2alt" LUT -- the plain V2
// values (0x1/0x2/0x1) gave this waveform phase fewer refresh frames than
// this hardware needs, part of what produced a washed-out, low-contrast
// result instead of full black/white.
unsigned char LUT_WB_7IN5_V2[]={
	0x80,	0xF,	0xF,	0x0,	0x0,	0x3,
	0x84,	0xF,	0x1,	0xF,	0x1,	0x4,
	0x40,	0xF,	0xF,	0x0,	0x0,	0x3,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
};

unsigned char LUT_BB_7IN5_V2[]={	
	0x80,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x84,	0xF,	0x1,	0xF,	0x1,	0x2,	
	0x40,	0xF,	0xF,	0x0,	0x0,	0x1,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	
};

Epd::~Epd() {
};

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
};

int Epd::Init(void) {
    if (IfInit() != 0) {
        return -1;
    }
    Reset();

    // SendCommand(0x01); 
    // SendData(0x07);
    // SendData(0x07);
    // SendData(0x3f);
    // SendData(0x3f);

    // SendCommand(0x04);
    // DelayMs(100);
    // WaitUntilIdle();
    
    // SendCommand(0X00);			//PANNEL SETTING
    // SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

    // SendCommand(0x61);        	//tres
    // SendData(0x03);		//source 800
    // SendData(0x20);
    // SendData(0x01);		//gate 480
    // SendData(0xE0);

    // SendCommand(0X15);
    // SendData(0x00);

    // SendCommand(0X50);			//VCOM AND DATA INTERVAL SETTING
    // SendData(0x10);
    // SendData(0x07);

    // SendCommand(0X60);			//TCON SETTING
    // SendData(0x22);

    // Power setting: mode-select byte and VSH/VSHR match the
    // proven-working ESPHome "7.50inv2alt" values (0x07, not the
    // plain-V2 stock demo's 0x17). VSL runs symmetric with VSH (0x3F/
    // 0x3F) rather than V2alt's asymmetric 0x26 -- needed on this
    // specific panel for black to reach full saturation, not just
    // white (see PANEL SETTING below for the other half of that fix).
    SendCommand(0x01);  // power setting
    SendData(0x07);  // 1-0=11: internal power
    SendData(0x17);  // VGH&VGL
    SendData(0x3F);  // VSH
    SendData(0x3F);  // VSL
    SendData(0x11);  // VSHR

    SendCommand(0x82);  // VCOM DC Setting
    SendData(*(Voltage_Frame_7IN5_V2+4));  // VCOM

    SendCommand(0x06);  // Booster Setting
    SendData(0x27);
    SendData(0x27);
    SendData(0x2F);
    SendData(0x17);

    // No OSC (0x30) command here -- the proven-working ESPHome
    // "7.50inv2alt" init sequence never sends one; it was carried over
    // unmodified from the plain-V2 stock demo and isn't part of what
    // this hardware variant actually expects.

    SendCommand(0x04); //POWER ON
    DelayMs(100);
    WaitUntilIdle();

    // PANEL SETTING: every register-LUT (0x3F, "KW-3f") test today --
    // solid fills and real text/shapes, with both the old asymmetric
    // and the fixed symmetric VSL -- has shown some form of defect
    // (speckle, streaking, rough text), confirmed in person, not just
    // in photos. The one config that ever gave a genuinely clean result
    // was the panel's own built-in OTP waveform (0x1F, "BWOTP-1f") on a
    // solid fill. That test never isolated real text/shapes on their
    // own, though -- it was run together with a since-removed dithered
    // gradient bar that may have been dragging the result down. Back to
    // OTP mode to test clean text + solid shapes together, nothing else.
    SendCommand(0X00);			//PANNEL SETTING
    SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

    SendCommand(0x61);        	//tres
    SendData(0x03);		//source 800
    SendData(0x20);
    SendData(0x01);		//gate 480
    SendData(0xE0);

    SendCommand(0X15);
    SendData(0x00);

    SendCommand(0X50);			//VCOM AND DATA INTERVAL SETTING
    SendData(0x10);
    SendData(0x00);

    SendCommand(0X60);			//TCON SETTING
    SendData(0x22);

    SendCommand(0x65);  // Resolution setting
    SendData(0x00);
    SendData(0x00);//800*480
    SendData(0x00);
    SendData(0x00);

    // Proven-working sequence waits here for idle before loading the LUTs,
    // which the plain-V2 stock demo skipped.
    WaitUntilIdle();

    // Skipped while PANEL SETTING above selects OTP mode (0x1F) -- a
    // register LUT loaded here would be ignored by the chip anyway.
    // SetLut_by_host(LUT_VCOM_7IN5_V2, LUT_WW_7IN5_V2, LUT_BW_7IN5_V2, LUT_WB_7IN5_V2, LUT_BB_7IN5_V2);

    return 0;
}

// 4-level grayscale init. First attempt at this (now corrected) had the
// commands in the wrong ORDER, not just wrong values -- verified on real
// hardware to produce only 2 visual levels, not 4. Re-derived by reading
// Waveshare's exact reference file directly (RaspberryPi_JetsonNano/c/
// lib/e-Paper/EPD_7in5_V2.c, EPD_7IN5_V2_Init_4Gray), not a paraphrase of
// it -- command order for these controllers is meaningful, not just the
// byte values, and this file's earlier version got that wrong by
// building the sequence from a description rather than the literal
// source. The reference's exact order:
//   Reset -> PANEL SETTING(0x1F) -> 0x50(10 07) -> POWER ON+wait ->
//   Booster(27 27 18 17) -> 0xE0(02) -> 0xE5(5F)
// -- notably PANEL SETTING comes FIRST here, before POWER ON, unlike the
// B/W Init() above where POWER ON comes before PANEL SETTING. No 0x61
// (resolution) or 0x60 (TCON) commands in the reference's 4Gray init at
// all -- an earlier version of this function incorrectly added them.
//
// One deliberate deviation from the reference: it sends no 0x01 (power
// setting) or 0x82 (VCOM DC) at all, relying on OTP power defaults. We
// already proved on this exact physical panel that OTP power defaults
// under-drive VSL and wash out contrast (most of today's earlier
// bring-up) -- so those two commands are inserted right after Reset(),
// in the same position they hold in the proven-working B/W Init(),
// using the same proven values, before the reference's own sequence.
//
// RESULT ON THIS PHYSICAL PANEL: with this exact, verified-correct
// command order/values, DisplayFrame4Gray() still only produces 2
// visual levels, not 4 -- and does so with a very specific signature:
// the output exactly matches using ONLY the "old data" plane (command
// 0x10) and ignoring "new data" (0x13) entirely (old-plane-bit=0 -> the
// top two GRAY4_LEVELS both showed white; old-plane-bit=1 -> the bottom
// two both showed black). That clean, non-alternating split rules out a
// wiring/mapping bug in DisplayFrame4Gray() below (a real mapping error
// would produce a different, less clean pattern) and points at this
// panel's controller not actually running a real 4-gray waveform engine
// behind the two-plane protocol, regardless of what the underlying
// GDEW075T7 chip model is capable of in other batches/COG revisions.
// Panel's date code was unrecoverable (no visible manufacture date, only
// a generic "AWM 20624" cable certification marking), so this couldn't
// be cross-checked against Waveshare's "4Gray only works on panels sold
// after 24/10/23" caveat directly -- but the behavioral evidence points
// the same direction. Init4Gray()/DisplayFrame4Gray() are left in place,
// confirmed correct against the reference and internally consistent
// with eink-snapshot's pack_gray4() -- they're just unused by the
// current color_mode-less main.cpp, ready if a panel that actually
// supports this ever gets tested here.
int Epd::Init4Gray(void) {
    if (IfInit() != 0) {
        return -1;
    }
    Reset();

    SendCommand(0x01);  // power setting -- our proven values, not sent at all in the reference
    SendData(0x07);
    SendData(0x17);
    SendData(0x3F);
    SendData(0x3F);
    SendData(0x11);

    SendCommand(0x82);  // VCOM DC Setting -- our proven value, not sent at all in the reference
    SendData(*(Voltage_Frame_7IN5_V2+4));

    SendCommand(0X00);  // PANEL SETTING -- same 0x1F as B/W OTP mode
    SendData(0x1F);

    SendCommand(0X50);  // VCOM AND DATA INTERVAL SETTING -- 4Gray-specific (0x10 0x07, not B/W's 0x10 0x00)
    SendData(0x10);
    SendData(0x07);

    SendCommand(0x04); //POWER ON
    DelayMs(100);
    WaitUntilIdle();

    SendCommand(0x06);  // Booster Setting -- 4Gray-specific 3rd byte (0x18, not B/W's 0x2F)
    SendData(0x27);
    SendData(0x27);
    SendData(0x18);
    SendData(0x17);

    SendCommand(0xE0);  // 4Gray-specific, not sent at all in B/W path
    SendData(0x02);
    SendCommand(0xE5);  // 4Gray-specific, not sent at all in B/W path
    SendData(0x5F);

    WaitUntilIdle();
    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
bool Epd::WaitUntilIdle(void) {
    // Polarity corrected back to Waveshare's stock convention: raw
    // BUSY_PIN reads LOW while busy, HIGH when idle. An earlier version
    // of this file had this backwards (waiting while HIGH, exiting on
    // LOW) based on a misreading of the proven-working ESPHome config's
    // `busy_pin.inverted: true`. Verified directly from ESPHome's own
    // ESP32 GPIO source (esphome/components/esp32/gpio.cpp):
    //   digital_read() returns (raw_level != inverted_)
    // and ESPHome's wait loop is `while (busy_pin_->digital_read())`.
    // With inverted_=true that's `while (!raw_level)` -- i.e. it waits
    // while raw is LOW and exits on HIGH, the same as Waveshare's
    // original un-inverted `while (busy == 0)`. The ESPHome inversion
    // flag compensates for ESPHome's own default assumption being
    // opposite of Waveshare's, not for anything unusual about this
    // board's wiring. Getting this backwards meant the loop treated a
    // real, in-progress refresh (LOW) as "done" almost instantly, and
    // treated the panel already being idle (HIGH) as "still busy" and
    // spun uselessly until the timeout -- both observed directly while
    // bringing this up, and both are consistent with this fix.
    unsigned char busy;
    Serial.print("e-Paper Busy\r\n ");
    unsigned long start = millis();
    unsigned long lastLog = start;
    do{
        SendCommand(0x71);
        busy = DigitalRead(busy_pin);
        if (millis() - lastLog > 2000) {
            lastLog = millis();
            Serial.printf("  ...still waiting on busy pin (%lu ms elapsed, raw=%d)\n", millis() - start, busy);
        }
        if (millis() - start > 10000) {
            Serial.println("  TIMEOUT waiting on busy pin, giving up");
            return false;
        }
        DelayMs(10);
    }while(busy == 0);
    Serial.print("e-Paper Busy Release\r\n ");
    DelayMs(20);
    return true;
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    // Reset low-pulse shortened from Waveshare's stock 4ms to 2ms --
    // matches the proven-working ESPHome reset_duration: 2ms for this
    // exact driver board. Waveshare's own FAQ warns a too-long low pulse
    // trips the board's power-off protection circuit and causes the
    // reset (and therefore BUSY) to fail permanently.
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
    DigitalWrite(reset_pin, LOW);                //module reset
    DelayMs(2);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);
}

bool Epd::DisplayFrame(const unsigned char* frame_buffer) {
    // Matches the proven-working ESPHome per-update sequence exactly
    // (power on -> send data -> wait -> trigger refresh -> wait -> power
    // off), including a wait_until_idle() between finishing the data
    // send and triggering the refresh that the plain-V2 stock demo
    // (which this file started from) doesn't do.
    SendCommand(0x04);  // POWER ON
    DelayMs(200);
    if (!WaitUntilIdle()) return false;

    SendCommand(0x13);
    DelayMs(2);
    for (unsigned long j = 0; j < height; j++) {
        for (unsigned long i = 0; i < width/8; i++) {
            SendData(~frame_buffer[i + j * width/8]);
        }
    }
    DelayMs(100);
    if (!WaitUntilIdle()) return false;

    SendCommand(0x12);  // DISPLAY REFRESH
    DelayMs(100);
    if (!WaitUntilIdle()) return false;

    SendCommand(0x02);  // POWER OFF
    return WaitUntilIdle();
}

bool Epd::DisplayFrame4Gray(const unsigned char* frame_buffer) {
    // frame_buffer: 2 bits/pixel, MSB first, 4 pixels/byte -- matches
    // eink-snapshot's pack_gray4() exactly (itself a port of Waveshare's
    // own Python reference packing, including its value remap).
    //
    // The panel wants each pixel split into two 1-bit "planes" (old data
    // / new data) that it recombines internally into 4 gray levels.
    // Ported from Waveshare's official EPD_7IN5_V2_Display_4Gray() C
    // reference, but re-expressed pixel-by-pixel here instead of their
    // block-extraction loop, for clarity -- verified to produce the
    // identical bit-for-bit mapping:
    //   pixel code 3 (0b11, white)      -> old=0 new=0
    //   pixel code 0 (0b00, black)      -> old=1 new=1
    //   pixel code 2 (0b10, dark gray)  -> old=1 new=0
    //   pixel code 1 (0b01, light gray) -> old=0 new=1
    SendCommand(0x04);  // POWER ON
    DelayMs(200);
    if (!WaitUntilIdle()) return false;

    const unsigned long totalPixels = width * height;
    const unsigned long planeBytes = totalPixels / 8;  // 48000 for 800x480, same size as a full B/W frame

    SendCommand(0x10);  // old data plane
    DelayMs(2);
    for (unsigned long b = 0; b < planeBytes; b++) {
        unsigned char outByte = 0;
        for (unsigned char bit = 0; bit < 8; bit++) {
            unsigned long pixelIndex = b * 8 + bit;
            unsigned long srcByte = pixelIndex / 4;
            unsigned char shift = 6 - 2 * (pixelIndex % 4);
            unsigned char code = (frame_buffer[srcByte] >> shift) & 0x03;
            unsigned char oldBit = (code == 0 || code == 2) ? 1 : 0;
            outByte = (outByte << 1) | oldBit;
        }
        SendData(outByte);
    }
    DelayMs(20);

    SendCommand(0x13);  // new data plane
    DelayMs(2);
    for (unsigned long b = 0; b < planeBytes; b++) {
        unsigned char outByte = 0;
        for (unsigned char bit = 0; bit < 8; bit++) {
            unsigned long pixelIndex = b * 8 + bit;
            unsigned long srcByte = pixelIndex / 4;
            unsigned char shift = 6 - 2 * (pixelIndex % 4);
            unsigned char code = (frame_buffer[srcByte] >> shift) & 0x03;
            unsigned char newBit = (code == 0 || code == 1) ? 1 : 0;
            outByte = (outByte << 1) | newBit;
        }
        SendData(outByte);
    }
    DelayMs(100);
    if (!WaitUntilIdle()) return false;

    SendCommand(0x12);  // DISPLAY REFRESH
    DelayMs(100);
    if (!WaitUntilIdle()) return false;

    SendCommand(0x02);  // POWER OFF
    return WaitUntilIdle();
}

void Epd::Displaypart(const unsigned char* pbuffer, unsigned long xStart, unsigned long yStart,unsigned long Picture_Width,unsigned long Picture_Height) {
    SendCommand(0x13);
    // xStart = xStart/8;
    // xStart = xStart*8;
    for (unsigned long j = 0; j < height; j++) {
        for (unsigned long i = 0; i < width/8; i++) {
            if( (j>=yStart) && (j<yStart+Picture_Height) && (i*8>=xStart) && (i*8<xStart+Picture_Width)){
                SendData(~(pgm_read_byte(&(pbuffer[i-xStart/8 + (Picture_Width)/8*(j-yStart)]))) );
                // SendData(0xff);
            }else {
                SendData(0x00);
            }
        }
    }
    SendCommand(0x12);
    DelayMs(100);
    WaitUntilIdle();
}

void Epd::SetLut_by_host(unsigned char* lut_vcom,  unsigned char* lut_ww, unsigned char* lut_bw, unsigned char* lut_wb, unsigned char* lut_bb)
{
	unsigned char count;

	SendCommand(0x20); //VCOM	
	for(count=0; count<42; count++)
		SendData(lut_vcom[count]);

	SendCommand(0x21); //LUTBW
	for(count=0; count<42; count++)
		SendData(lut_ww[count]);

	SendCommand(0x22); //LUTBW
	for(count=0; count<42; count++)
		SendData(lut_bw[count]);

	SendCommand(0x23); //LUTWB
	for(count=0; count<42; count++)
		SendData(lut_wb[count]);

	SendCommand(0x24); //LUTBB
	for(count=0; count<42; count++)
		SendData(lut_bb[count]);
}

/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          The only one parameter is a check code, the command would be
 *          executed if check code = 0xA5. 
 *          You can use EPD_Reset() to awaken
 */
void Epd::Sleep(void) {
    SendCommand(0X02);
    WaitUntilIdle();
    SendCommand(0X07);
    SendData(0xA5);
}

void Epd::Clear(void) {
    
    // SendCommand(0x10);
    // for(unsigned long i=0; i<height*width; i++) {
    //     SendData(0x00);
    // }
    SendCommand(0x13);
    for(unsigned long i=0; i<height*width; i++)	{
        SendData(0x00);
    }
    SendCommand(0x12);
    DelayMs(100);
    WaitUntilIdle();
}


/* END OF FILE */


