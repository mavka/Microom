/*
 * pcm1865.cpp
 *
 *  Created on: 08 ����. 2015 �.
 *      Author: Kreyl
 */

#include "pcm1865.h"
#include "pcm1865_regs.h"
#include "uart.h"
#include "clocking.h"

/*
 * Fs = 8kHz
 * SCKI = Fs*256 (fixed) == 2048kHz (stm output)
 *
 */


void PCM1865_t::Init() {
    CS.Init();
    CS.SetHi();
    // SPI pins
    PinSetupAlterFunc(PCM_SPI_GPIO, PCM_SCK,  omPushPull, pudNone, PCM_SPI_AF);
    PinSetupAlterFunc(PCM_SPI_GPIO, PCM_MISO, omPushPull, pudNone, PCM_SPI_AF);
    PinSetupAlterFunc(PCM_SPI_GPIO, PCM_MOSI, omPushPull, pudNone, PCM_SPI_AF);
    // ==== Control SPI ==== MSB first, master, ClkLowIdle, FirstEdge, Baudrate=32/4=8MHz
    ISpi.Setup(PCM_SPI, boMSB, cpolIdleLow, cphaSecondEdge, sbFdiv4);
    ISpi.Enable();

    // ==== I2S ====
    // GPIO
    PinSetupAlterFunc(GPIOC, 6,  omPushPull, pudNone, AF5);  // I2S2 MCK
    PinSetupAlterFunc(GPIOB, 12, omPushPull, pudNone, AF5);  // I2S2_WS LRClk1
    PinSetupAlterFunc(GPIOB, 13, omPushPull, pudNone, AF5);  // I2S2_CK BitClk1
    PinSetupAlterFunc(GPIOB, 15, omPushPull, pudNone, AF5);  // I2S2_SD DataOut1

    rccEnableSPI2(FALSE);
    // I2S Clk
    RCC->CFGR &= ~RCC_CFGR_I2SSRC;  // Disable external clock
    Clk.SetupI2SClk(128, 5);        // I2S PLL Divider
    // I2S
    SPI2->CR1 = 0;
    SPI2->CR2 = 0;
    SPI2->I2SCFGR = 0;  // Disable I2S
    // I2S, Master Receive, CkPol=Low, DatLen=16bit, ChLen=16bit, Frame = Philips I2S
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG;
    SPI2->I2SPR = SPI_I2SPR_MCKOE | (1 << 8) | (uint16_t)12;    // 8000
    SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;

    SelectPage(0);
}

void PCM1865_t::WriteReg(uint8_t Addr, uint8_t Value) {
    CS.SetLo();
    Addr = (Addr << 1) | 0x00;  // Write operation
    ISpi.ReadWriteByte(Addr);
    ISpi.ReadWriteByte(Value);
    CS.SetHi();
}

uint8_t PCM1865_t::ReadReg(uint8_t Addr) {
    CS.SetLo();
    Addr = (Addr << 1) | 0x01;  // Read operation
    ISpi.ReadWriteByte(Addr);
    uint8_t r = ISpi.ReadWriteByte(0);
    CS.SetHi();
    return r;
}

#if 1 // ============================== Commands ===============================
void PCM1865_t::PrintState() {
    Uart.Printf("\rPCM State");
    uint8_t b = ReadReg(0x72);
    switch(b & 0x0F) {
        case 0x00: Uart.Printf("\r  PwrDown"); break;
        case 0x01: Uart.Printf("\r  Wait Clk Stable"); break;
        case 0x02: Uart.Printf("\r  Release reset"); break;
        case 0x03: Uart.Printf("\r  Stand-by"); break;
        case 0x04: Uart.Printf("\r  Fade IN"); break;
        case 0x05: Uart.Printf("\r  Fade OUT"); break;
        case 0x09: Uart.Printf("\r  Sleep"); break;
        case 0x0F: Uart.Printf("\r  Run"); break;
        default: Uart.Printf("\r  ! Reserved"); break;
    }
    // Ratio
    b = ReadReg(0x73);
    switch(b & 0x07) {
        case 0x00: Uart.Printf("\r  Out of range (Low) or LRCK Halt"); break;
        case 0x01: Uart.Printf("\r  8kHz"); break;
        case 0x02: Uart.Printf("\r  16kHz"); break;
        case 0x03: Uart.Printf("\r  32-48kHz"); break;
        case 0x04: Uart.Printf("\r  88.2-96kHz"); break;
        case 0x05: Uart.Printf("\r  176.4-192kHz"); break;
        case 0x06: Uart.Printf("\r  Out of range (High)"); break;
        case 0x07: Uart.Printf("\r  Invalid Fs"); break;
        default: break;
    }
    b = ReadReg(0x74);
    switch(b & 0x70) {
        case 0x00: Uart.Printf("\r  BCK Ratio: Out of range (L) or BCK Halt"); break;
        case 0x10: Uart.Printf("\r  BCK Ratio: 32"); break;
        case 0x20: Uart.Printf("\r  BCK Ratio: 48"); break;
        case 0x30: Uart.Printf("\r  BCK Ratio: 64"); break;
        case 0x40: Uart.Printf("\r  BCK Ratio: 256"); break;
        case 0x60: Uart.Printf("\r  BCK Ratio: Out of range (H)"); break;
        case 0x70: Uart.Printf("\r  Invalid BCK ratio or LRCK Halt"); break;
        default: break;
    }
    switch(b & 0x07) {
        case 0x00: Uart.Printf("\r  SCK Ratio: Out of range (L) or SCK Halt"); break;
        case 0x01: Uart.Printf("\r  SCK Ratio: 128"); break;
        case 0x02: Uart.Printf("\r  SCK Ratio: 256"); break;
        case 0x03: Uart.Printf("\r  SCK Ratio: 384"); break;
        case 0x04: Uart.Printf("\r  SCK Ratio: 512"); break;
        case 0x05: Uart.Printf("\r  SCK Ratio: 768"); break;
        case 0x06: Uart.Printf("\r  SCK Ratio: Out of range (H)"); break;
        case 0x07: Uart.Printf("\r  Invalid SCK ratio or LRCK Halt"); break;
        default: break;
    }
    // Clock
    b = ReadReg(0x75);
    if(b & 0x40) Uart.Printf("\r  LRCK Halt");
    else if(b & 0x04) Uart.Printf("\r  LRCK Error");
    else Uart.Printf("\r  LRCK Ok");
    if(b & 0x20) Uart.Printf("\r  BCK Halt");
    else if(b & 0x02) Uart.Printf("\r  BCK Error");
    else Uart.Printf("\r  BCK Ok");
    if(b & 0x10) Uart.Printf("\r  SCK Halt");
    else if(b & 0x01) Uart.Printf("\r  SCK Error");
    else Uart.Printf("\r  SCK Ok");
    // Power
    b = ReadReg(0x78);
    if(b & 0x04) Uart.Printf("\r  DVDD Ok");
    else Uart.Printf("\r  DVDD Bad/Missing");
    if(b & 0x02) Uart.Printf("\r  AVDD Ok");
    else Uart.Printf("\r  AVDD Bad/Missing");
    if(b & 0x01) Uart.Printf("\r  LDO Ok");
    else Uart.Printf("\r  LDO Failure");
}

void PCM1865_t::SetGain(PcmAdcChnls_t Chnl, int8_t Gain_dB) {
//    if(Gain_dB < -12 or Gain_dB > 40) return;

//    uint8_t r =
      ReadReg(0x72);
      ReadReg(0x73);
      ReadReg(0x74);
      ReadReg(0x75);
      ReadReg(0x78);
//    Uart.Printf("\rBefore: %X", r);

//    Gain_dB *= 2;
//    WriteReg((uint8_t)Chnl, (uint8_t)Gain_dB);
//    WriteReg(1, 5);
//    r = ReadReg(1);
//    Uart.Printf("\rAfter: %X", r);
}
#endif