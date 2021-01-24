#ifndef _H_VS1003B_MP3_H_
#define _H_VS1003B_MP3_H_

#include <SPI.h>
#include <Arduino.h>
#include <FreeRTOS.h>

namespace VS1003B {
  // direct gpio
  #define GPIO_0TO31SET_REG   *((volatile unsigned long *)GPIO_OUT_W1TS_REG)
  #define GPIO_0TO31CLR_REG   *((volatile unsigned long *)GPIO_OUT_W1TC_REG)
  #define GPIO_0TO32RD_REG    *((volatile unsigned long *)GPIO_IN_REG)

  // pin connections with VS1003B
  #define VS1003B_SCK   (14)
  #define VS1003B_MISO  (12)
  #define VS1003B_MOSI  (13)

  #define VS1003B_XCS   (23)
  #define VS1003B_XDCS  (27)
  #define VS1003B_XRST  (26)
  #define VS1003B_DREQ  (25)

  // SCI_MODE
  #define SM_DIFF       (0b000000000000001)
  #define SM_SETTOZERO  (0b000000000000010)
  #define SM_RESET      (0b000000000000100)
  #define SM_OUTOFWAV   (0b000000000001000)
  #define SM_PDOWN      (0b000000000010000)
  #define SM_TESTS      (0b000000000100000)
  #define SM_STREAM     (0b000000001000000)
  #define SM_SETTOZERO2 (0b000000010000000)
  #define SM_DACT       (0b000000100000000)
  #define SM_SDIORD     (0b000001000000000)
  #define SM_SDISHARE   (0b000010000000000)
  #define SM_SDINEW     (0b000100000000000)
  #define SM_ADPCM      (0b001000000000000)
  #define SM_ADPCM_HP   (0b010000000000000)
  #define SM_LINE_IN    (0b100000000000000)

  // SCI registers
  #define SR_MODE           (0x00)
  #define SR_STATUS         (0x01)
  #define SR_BASS           (0x02)
  #define SR_CLOCKF         (0x03)
  #define SR_DECODE_TIME    (0x04)
  #define SR_AUDATA         (0x05)
  #define SR_WRAM           (0x06)
  #define SR_WRAMADDR       (0x07)
  #define SR_HDAT0          (0x08)
  #define SR_HDAT1          (0x09)
  #define SR_AIADDR         (0x0a)
  #define SR_VOL            (0x0b)
  #define SR_AICTRL0        (0x0c)
  #define SR_AICTRL1        (0x0d)
  #define SR_AICTRL2        (0x0e)
  #define SR_AICTRL3        (0x0f)

  // task state
  #define VS1003B_STOPPED   (0)
  #define VS1003B_PLAYING   (1)
  volatile int vs1003bState = VS1003B_STOPPED;

  class mp3 {
      private:
      void sciWrite(unsigned char addr, unsigned short data) {
        GPIO_0TO31SET_REG = 1 << VS1003B_XDCS;
        GPIO_0TO31CLR_REG = 1 << VS1003B_XCS;
        
        vsSpi->write(0x02);   // write instruction
        vsSpi->write(addr);
        vsSpi->write16(data);
        while(!(GPIO_0TO32RD_REG & (1 << VS1003B_DREQ))) {
          ;
        }
        GPIO_0TO31SET_REG = 1 << VS1003B_XCS;
      }
      void sdiWriteBytes(const unsigned char *pdata, int size) {
        GPIO_0TO31CLR_REG = 1 << VS1003B_XDCS;
        vsSpi->writeBytes((unsigned char *)pdata, size);
        GPIO_0TO31SET_REG = 1 << VS1003B_XDCS;
      }
      public:
      SPIClass *vsSpi;
      mp3(int cSpi, int pinSck, int pinMiso, int pinMosi) {
        pinMode(VS1003B_XCS, OUTPUT);
        pinMode(VS1003B_XDCS, OUTPUT);
        pinMode(VS1003B_XRST, OUTPUT);
        pinMode(VS1003B_DREQ, INPUT);

        GPIO_0TO31SET_REG = (1 << VS1003B_XCS) | (1 << VS1003B_XDCS);
        vsSpi = new SPIClass(cSpi); // whether VSPI or HSPI.
        vsSpi->end();
        vsSpi->begin(pinSck, pinMiso, pinMosi, -1);

        while(!(GPIO_0TO32RD_REG & (1 << VS1003B_DREQ))) {
          delay(1);
        }

        sciWrite(SR_AUDATA, 44100 + 1); // set the sampling rate(44.1KHz) and stereo flag(1).
        sciWrite(SR_CLOCKF, 0x8000);    // set clock multiplier(x3.0), addition(0.0x) and frequency(12.288MHz).
        sciWrite(SR_VOL, 0x1a1a);
        sciWrite(SR_MODE, SM_RESET | SM_DIFF | SM_SDINEW);

        vsSpi->setFrequency(36864000 / 7);  // (XTAL * 3) / 7 : max       
      }
      void soundOut(const unsigned char *mp3buff, int size) {
        for(int i = 0; i < size; i += 32) {
          while(!(GPIO_0TO32RD_REG & (1 << VS1003B_DREQ))) {
            ;
          }
          sdiWriteBytes(&mp3buff[i], 32);
        }
      }
  };

} /* namespace VS1003B */
#endif /* _H_VS1003B_MP3_H_ */
