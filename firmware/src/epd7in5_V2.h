/**
 *  @filename   :   epd7in5.h
 *  @brief      :   Header file for e-paper library epd7in5.cpp
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

#ifndef EPD7IN5_V2_H
#define EPD7IN5_V2_H

#include "epdif.h"

// Display resolution
#define EPD_WIDTH       800
#define EPD_HEIGHT      480

class Epd : EpdIf {
public:
    Epd();
    ~Epd();
    int  Init(void);
    // 4-level grayscale variant of Init() -- separate method, not a
    // parameter on Init(), so the confirmed-working B/W path (Init())
    // stays byte-for-byte untouched by this addition. See DisplayFrame4Gray.
    int  Init4Gray(void);
    bool WaitUntilIdle(void);
    void Reset(void);
    void SetLut(void);
    // Returns true only if the panel signaled real completion via BUSY;
    // false means we gave up after a timeout (BUSY pin unreliable right
    // now) -- callers should NOT call Sleep() after a false return, since
    // that could interrupt a physical refresh that's still in progress.
    bool DisplayFrame(const unsigned char* frame_buffer);
    // 4-level grayscale display. frame_buffer is 2 bits/pixel, MSB first,
    // 4 pixels/byte (EPD_WIDTH*EPD_HEIGHT/4 bytes total) -- matches the
    // eink-snapshot add-on's pack_gray4() exactly, itself a port of
    // Waveshare's own Python reference packing. Must be preceded by
    // Init4Gray(), not Init().
    bool DisplayFrame4Gray(const unsigned char* frame_buffer);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void Sleep(void);
    void Clear(void);
    void Displaypart(const unsigned char* pbuffer, unsigned long Start_X, unsigned long Start_Y,unsigned long END_X,unsigned long END_Y);
 
private:
    unsigned int reset_pin;
    unsigned int dc_pin;
    unsigned int cs_pin;
    unsigned int busy_pin;
    unsigned long width;
    unsigned long height;

	void SetLut(unsigned char *lut);
    void SetLut_by_host(unsigned char *lut_vcom, unsigned char *lut_ww, unsigned char *lut_bw, unsigned char *lut_wb, unsigned char *lut_bb);
};

#endif /* EPD7IN5_H */

/* END OF FILE */
