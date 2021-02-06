#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <FreeRTOS.h>

#include "VS1003Bmp3.hpp"
#include "testmidi.h"

// SD
#define SD_SCK  (18)
#define SD_MISO (19)
#define SD_MOSI (23)
#define SD_SS   ( 5)
SPIClass vSpi(VSPI);

volatile unsigned long cnt = 0;
#define SD_BUFF_SIZE   (32 * 64)
#define SD_BUFF_NUM (16) 
unsigned char sdBuff[SD_BUFF_NUM][SD_BUFF_SIZE];
unsigned char dummyBuff[800];
int readBuffNum = 0;
int writeBuffNum = 0;
typedef enum {
  BUFF_EMPTY,
  BUFF_READING,
  BUFF_WRITING,
  BUFF_FULL,
} buff_status_t;
int buffStatus[SD_BUFF_NUM] = {BUFF_EMPTY};

hw_timer_t *timer = NULL;   // 音声再生用
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

VS1003B::mp3 *m;

File gRoot;
int gFileCnt = 0;
#define PLAY_LIST_MAX (256)
char *playList[PLAY_LIST_MAX] = {0};

void IRAM_ATTR timerCallbackFunc() {
  portENTER_CRITICAL_ISR(&timerMux);
  size_t written = 32;
  size_t accumWritten = 0;

  if(buffStatus[readBuffNum] == BUFF_FULL && cnt == 0) {
    buffStatus[readBuffNum] = BUFF_READING;
  }
  if(buffStatus[readBuffNum] == BUFF_READING) {
    do {
      m->soundOut((const unsigned char *)&sdBuff[readBuffNum][cnt], written);

      cnt += written;
      accumWritten += written;
    } while(accumWritten == 32);

    if(cnt >= SD_BUFF_SIZE) {
      buffStatus[readBuffNum] = BUFF_EMPTY;
      int tmp_readBuffNum = readBuffNum;
      ++tmp_readBuffNum %= SD_BUFF_NUM;
      if(buffStatus[tmp_readBuffNum] == BUFF_FULL) {
        cnt = 0;
        ++readBuffNum %= SD_BUFF_NUM;
      }
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

// プレイリストにファイルパスを格納
void getFileNames(char **playList, File dir) {
  for(;;) {
    File item = dir.openNextFile();
    if(!item || gFileCnt >= PLAY_LIST_MAX) {
      break;
    } 

    if(item.isDirectory()) {
      // 特定ディレクトリは中に入らないよう除外
      if(strcmp(item.name(), "/System Volume Information") != 0 &&
         strcmp(item.name(), "/hidden") != 0) {
        getFileNames(playList, item);
      }
    } else {
      playList[gFileCnt] = (char *)malloc(256);
      strcpy(playList[gFileCnt], item.name()); 
      gFileCnt++;
    }
  }
}

// プレイリスト作成
void makePlayList(char **playList) {
  File root = SD.open("/");
  getFileNames(playList, root);
  root.close();
}

void setup(void) {
  Serial.begin(115200);

  m = new VS1003B::mp3(HSPI, 14, 12, 13);

  vSpi.end();
  vSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS);
  //vSpi.setFrequency(15000000);
  vSpi.setDataMode(SPI_MODE1);
  if(!SD.begin(SD_SS, vSpi, 15000000)) {
    Serial.println("SD card mount failed!!");
  }
  makePlayList(playList);

  for(int i = 0; i < 256; i++) {
    if(playList[i]) {
      Serial.printf("%s\r\n", playList[i]);
    }
  }

  // タイマー割り込み設定 
   timer = timerBegin(1, 4, true);
  timerAttachInterrupt(timer, timerCallbackFunc, true);
  timerAlarmWrite(timer, 558 << 5, true);
  timerAlarmDisable(timer);
}

void loop() {
  // プレイリストのインデックス
  static int wavNum = 0;
  // 新しいファイルの始まり
  static bool first = true;
   bool flg_waitForBufferFilled = true;
  
  File file = SD.open(playList[wavNum], FILE_READ);

  if(!file) {
    Serial.println("FILE_READ error...");
  } else {
    // 再生データ長を取得
    int rest = file.size();
    cnt = 0;
    readBuffNum = 0;
    writeBuffNum = 0;
    memset(buffStatus, BUFF_EMPTY, sizeof(buffStatus));

    while(rest > 0) {
      if(first) {
        first = false;
        // バッファ全面にデータを読み込んでおく
        do {
          file.read(sdBuff[writeBuffNum], rest < SD_BUFF_SIZE ? rest : SD_BUFF_SIZE);
          rest -= SD_BUFF_SIZE;
          ++writeBuffNum;
        } while(writeBuffNum < SD_BUFF_NUM);

        for(int i = 0; i < SD_BUFF_NUM; i++) {
          buffStatus[i] = BUFF_FULL;
        }
        readBuffNum = 0;
        writeBuffNum = SD_BUFF_NUM / 2;

        if(flg_waitForBufferFilled) {
          // 再生開始
          timerAlarmEnable(timer);
          flg_waitForBufferFilled = false;
        }
      }
      // 空のバッファにデータ読み込み
      if(buffStatus[writeBuffNum] == BUFF_EMPTY) {
        buffStatus[writeBuffNum] = BUFF_WRITING;
        while(!file.read(sdBuff[writeBuffNum], rest < SD_BUFF_SIZE ? rest : SD_BUFF_SIZE)) {
          Serial.printf("sd read() error. rest:%d\r\n", rest);
        }
        rest -= SD_BUFF_SIZE;
        buffStatus[writeBuffNum] = BUFF_FULL;
        Serial.printf("RB:%d, WB:%d\r\n", readBuffNum, writeBuffNum);
      } else {
        ++writeBuffNum %= SD_BUFF_NUM;
      }
    }
    ++wavNum %= gFileCnt;
  }
  delay(1);
}
