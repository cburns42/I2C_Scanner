/*
 * I2C Scanner by Charles Burns
 *  - 320x240 color display, SPI ST7789 driver
 *  - Adafruit ANO Rotary Navigation Encoder Breakout - Pre-Soldered Encoder Product ID: 6311 
 * Scans all I2C addresses
 *  - Optional perform multiple scans ay 100khz, 400khz and 1mhz
 *  - Optional continuous scan
 *  - Display details of selected device
 *    - Step through list of devices at the selected address
 *
 * Libraries used:
 * Adafruit GFX Library                 1.12.4 
 * Adafruit ST7735 and ST7789 Library   1.11.0 
 * SPI                                  2.0.0  
 * Wire                                 1.0    
 * Adafruit seesaw Library              1.7.9  
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"
#include "deviceinfo.h"
#include "bitmaps.h"


/*
 * Definitions used for SPI display
 */
#define TFT_CS    15
#define TFT_RST   04
#define TFT_DC    14
#define TFT_BL    27
#define TFT_MOSI  23
#define TFT_SCLK  18

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

/*
 * Definitions used for second I2C port
 */
#define SDA_2 25
#define SCL_2 26

byte i2c_map[128];

/*
 * Definitions used for Adafruit Rotary Encoder
 */
#define SS_SWITCH_SELECT 1
#define SS_SWITCH_UP     2
#define SS_SWITCH_LEFT   3
#define SS_SWITCH_DOWN   4
#define SS_SWITCH_RIGHT  5

#define SEESAW_ADDR      0x4A

Adafruit_seesaw ss;

/*
 * Definitions used for UI screen control
 */
enum UISTATE {
  SCREENMAIN,
  SCREENSCAN,
  SCREENDETAIL
};

int8_t uiState = SCREENMAIN;

int8_t mainCursor = 0;
int8_t mainPrevCursor = 0;

int8_t scanCursor = 0;
int8_t scanPrevCursor = 0;

int8_t cursorXGrid = 0;
int8_t cursorYGrid = 0;
int8_t cursorPrevXGrid = 0;
int8_t cursorPrevYGrid = 0;
boolean gridMode = false;

int8_t detailCursor = 0;
int8_t detailPrevCursor = 0;
int8_t detailIndex = 0;
int deviceIndex;

boolean speedTest = false;
boolean continuousTest = false;

/*
 * Screen backlight control
 */
int backlightLevel = 255;

void setBrightness(int level) {
  level = constrain(level, 0, 255);
  backlightLevel = level;
  ledcWrite(TFT_BL, level);
}

void setup(void) {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(TFT_BL, HIGH);
  digitalWrite(LED_BUILTIN, LOW);

  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, 150);
  setBrightness(255);

  tft.init(240, 320); 
  tft.setSPISpeed(80000000);
  tft.setRotation(3);

  Wire1.begin(SDA_2, SCL_2, 100000);
  Wire.begin(); 

  if (! ss.begin(SEESAW_ADDR)) {
    Serial.println("Couldn't find seesaw");
  }
  Serial.println("seesaw started");
  uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
  if (version  != 5740){
    Serial.print("Wrong firmware loaded? ");
    Serial.println(version);
  }

  ss.pinMode(SS_SWITCH_UP, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_DOWN, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_LEFT, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_RIGHT, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_SELECT, INPUT_PULLUP);

  tft.fillScreen(ST77XX_BLACK);
  for(int i = 0; i < 10; i++){
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(TFT_BL, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(TFT_BL, HIGH);
    delay(100);
  }
  clearI2CMap();
  drawMainPage();
}

/*
 * I2C scan functions
 */
enum I2CSTATE {
  I2CFOUND1KHZ,
  I2CFOUND4KHZ,
  I2CFOUND1MHZ,
  I2CILLEGAL,
  I2CERROR,
  I2CNOTFOUND
};

void clearI2CMap(){
  for(int i = 0; i < 128; i++)
    i2c_map[i] = I2CNOTFOUND;
}

void setI2CBlock(int a, int s){
  i2c_map[a] = s;
}

/* --------------------------------------------------------
 * Main Page
 * --------------------------------------------------------
 */
void drawMainPage(){
  uiState = SCREENMAIN;
  mainCursor = 0;

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setFont(&FreeSansBold12pt7b);

  tft.setCursor(100, 35); tft.print("I2C Scanner");

  tft.drawRoundRect( 60,  65, 200, 50, 10, 0x64bd);
  tft.setCursor(     95,  95); tft.print("Scan");
  tft.drawBitmap(    64,  75, bmp_play_arrow_24, 24, 24, ST77XX_WHITE);

  tft.drawRoundRect( 60, 125, 200, 50, 10, 0x64bd);
  tft.setCursor(     95, 155); tft.print("Speed Scan");
  if(speedTest){
    tft.drawBitmap(    64, 135, bmp_toggle_on_24, 24, 24, ST77XX_GREEN);
  } else{
    tft.drawBitmap(    64, 135, bmp_toggle_off_24, 24, 24, ST77XX_WHITE);
  }

  tft.drawRoundRect( 60, 185, 200, 50, 10, 0x64bd);
  tft.setCursor(     95, 215); tft.print("Continuous");
  if(continuousTest){
    tft.drawBitmap(    64, 195, bmp_toggle_on_24, 24, 24, ST77XX_GREEN);
  } else {
    tft.drawBitmap(    64, 195, bmp_toggle_off_24, 24, 24, ST77XX_WHITE);
  }
}

void drawCursorMain(){
  tft.drawRoundRect(59,  64 + (mainPrevCursor * 60), 202, 52, 10, ST77XX_BLACK);
  tft.drawRoundRect(59,  64 + (mainCursor * 60), 202, 52, 10, ST77XX_YELLOW);
}

void cursorUpMain(){
  mainPrevCursor = mainCursor;
  mainCursor -= 1;
  if(mainCursor < 0) mainCursor = 2;
  drawCursorMain();
}

void cursorDownMain(){
  mainPrevCursor = mainCursor;
  mainCursor += 1;
  if(mainCursor > 2) mainCursor = 0;
  drawCursorMain();  
}

void cursorSelectMain(){
  switch(mainCursor){
    case 0:
      drawScanPage();
      break;
    case 1:
      speedTest = !speedTest;
    if(speedTest){
      tft.drawBitmap(    64, 135, bmp_toggle_off_24, 24, 24, ST77XX_BLACK);
      tft.drawBitmap(    64, 135, bmp_toggle_on_24, 24, 24, ST77XX_GREEN);
    } else{
      tft.drawBitmap(    64, 135, bmp_toggle_on_24, 24, 24, ST77XX_BLACK);
      tft.drawBitmap(    64, 135, bmp_toggle_off_24, 24, 24, ST77XX_WHITE);
    }
      break;
    case 2:
      continuousTest = !continuousTest;
      if(continuousTest){
        tft.drawBitmap(    64, 195, bmp_toggle_off_24, 24, 24, ST77XX_BLACK);
        tft.drawBitmap(    64, 195, bmp_toggle_on_24, 24, 24, ST77XX_GREEN);
      } else {
        tft.drawBitmap(    64, 195, bmp_toggle_on_24, 24, 24, ST77XX_BLACK);
        tft.drawBitmap(    64, 195, bmp_toggle_off_24, 24, 24, ST77XX_WHITE);
      }
      break;
  }
}

/* --------------------------------------------------------
 * Scan Page
 * --------------------------------------------------------
 */
void drawScanPage(){
  uiState = SCREENSCAN;
  scanCursor = 0;

  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(10, 35); tft.print("Scanner");
  tft.drawBitmap(165, 1, bmp_run_24, 24, 24, continuousTest?ST77XX_GREEN:ST77XX_BLACK);
  tft.drawBitmap(191, 1, bmp_speed_24, 24, 24, speedTest?ST77XX_GREEN:ST77XX_BLACK);
  tft.drawBitmap(243, 1, bmp_home_24, 24, 24, ST77XX_WHITE);
  tft.drawBitmap(269, 1, bmp_grid_view_24, 24, 24, ST77XX_WHITE);
  tft.drawBitmap(295, 1, bmp_restart_24, 24, 24, ST77XX_WHITE);

  drawBase();
}

char hexit(int i){
  if(i < 10) return('0' + i);
  else return('A' + (i - 10));
}

void drawBase(){
  int x, y;

  tft.setFont(NULL);
  for(x = 0; x < 16; x++){
    tft.drawChar(52 + (x * 16), 65, hexit(x), ST77XX_YELLOW, ST77XX_BLACK, 1);
  }

  for(y = 0; y < 8; y++){
    tft.drawChar(38, 81 + (y * 16), hexit(y), ST77XX_YELLOW, ST77XX_BLACK, 1);
  }

  for(y = 0; y < 8; y++){
    for(x = 0; x < 16; x++){
      tft.drawRect(50 + (x * 16), 81 + (y * 16), 12, 12, ST77XX_WHITE);
    }
  }
  i2cScan();
}

void drawCursorScan(){
  tft.drawRect(242 + (scanPrevCursor * 26),  1, 26, 26, ST77XX_BLACK);
  tft.drawRect(242 + (scanCursor * 26),  1, 26, 26, ST77XX_YELLOW);
}

void cursorLeftScan(){
  scanPrevCursor = scanCursor;
  scanCursor -= 1;
  if(scanCursor < 0) scanCursor = 2;
  drawCursorScan();
}

void cursorRightScan(){
  scanPrevCursor = scanCursor;
  scanCursor += 1;
  if(scanCursor > 2) scanCursor = 0;
  drawCursorScan();  
}

void cursorSelectScan(){
  switch(scanCursor){
    case 0:
      gridMode = false;
      drawMainPage();
      break;
    case 1:
      gridMode = true;
      break;
    case 2:
      i2cScan();
      break;
  }
}

void drawCursorGrid(){
  int x, y;

  x = (cursorPrevXGrid * 16) + 48;
  y = (cursorPrevYGrid * 16) + 79;
  tft.drawRect(x, y, 16, 16, ST77XX_BLACK);

  x = (cursorXGrid * 16) + 48;
  y = (cursorYGrid * 16) + 79;
  tft.drawRect(x, y, 16, 16, ST77XX_YELLOW);
}

void cursorUpGrid(){
  cursorPrevXGrid = cursorXGrid;
  cursorPrevYGrid = cursorYGrid;
  cursorYGrid -= 1;
  if(cursorYGrid < 0) cursorYGrid = 7;
  tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_BLACK);
  drawCursorGrid();
}

void cursorDownGrid(){
  cursorPrevXGrid = cursorXGrid;
  cursorPrevYGrid = cursorYGrid;
  cursorYGrid += 1;
  if(cursorYGrid > 7) cursorYGrid = 0;
  tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_BLACK);
  drawCursorGrid();
}

void cursorLeftGrid(){
  cursorPrevXGrid = cursorXGrid;
  cursorPrevYGrid = cursorYGrid;
  cursorXGrid -= 1;
  if(cursorXGrid < 0) cursorXGrid = 15;
  tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_BLACK);
  drawCursorGrid();
}

void cursorRightGrid(){
  cursorPrevXGrid = cursorXGrid;
  cursorPrevYGrid = cursorYGrid;
  cursorXGrid += 1;
  if(cursorXGrid > 15) cursorXGrid = 0;
  tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_BLACK);
  drawCursorGrid();
}

void cursorSelectGrid(){
  int index = (cursorYGrid * 16) + cursorXGrid;

  if((i2c_map[index] == I2CILLEGAL) ||
     (i2c_map[index] == I2CERROR) ||
     (i2c_map[index] == I2CNOTFOUND)){
    tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_RED);
  } else {
    tft.drawBitmap(217, 1, bmp_error_24, 24, 24, ST77XX_BLACK);
    drawDetailPage();
  }
}

void drawI2CBlock(int addr, int status){
  int x, y, clr;

  switch(status){
    case I2CFOUND1KHZ:
      clr = ST77XX_GREEN;
      break;
    case I2CFOUND4KHZ:
      clr = ST77XX_CYAN;
      break;
    case I2CFOUND1MHZ:
      clr = ST77XX_BLUE;
      break;
    case I2CILLEGAL:
      clr = ST77XX_YELLOW;
      break;
    case I2CERROR:
      clr = ST77XX_RED;
      break;
    case I2CNOTFOUND:
      clr = 0x7412;
      break;
  }
  x = ((addr & 0x0F) * 16) + 50;
  y = (((addr >> 4) & 0x07) * 16) + 81;
  tft.fillRect(x, y, 12, 12, clr);
  tft.drawRect(x, y, 12, 12, ST77XX_WHITE);
}

void drawI2C(){
  for(int i = 0; i < 128; i++)
    drawI2CBlock(i, i2c_map[i]);
}

/* --------------------------------------------------------
 * Detail Page
 * --------------------------------------------------------
 */
void drawDetailPage(){
  uiState = SCREENDETAIL;
  detailCursor = 0;
  gridMode = false;

  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(10, 35); tft.print("Detail");
  tft.drawBitmap(243, 1, bmp_home_24, 24, 24, ST77XX_WHITE);
  tft.drawBitmap(269, 1, bmp_previous_24, 24, 24, ST77XX_WHITE);
  tft.drawBitmap(295, 1, bmp_next_24, 24, 24, ST77XX_WHITE);

  tft.setCursor(10,  80); tft.print("Address:");
  tft.setCursor(10, 108); tft.print("Device:");
  tft.setCursor(10, 136); tft.print("Description:");

  drawDetailBlock();
}

void drawDetailBlock(){
  char pbuf[16];
  deviceIndex = (cursorYGrid * 16) + cursorXGrid;
  tft.setTextColor(ST77XX_YELLOW);
  tft.fillRect( 119,  60, 201, 26, ST77XX_BLACK);
  tft.drawRect( 119,  60, 201, 26, ST77XX_CYAN);
  tft.fillRect(  99,  88, 221, 26, ST77XX_BLACK);
  tft.drawRect(  99,  88, 221, 26, ST77XX_CYAN);
  tft.fillRect(   0, 143, 319, 97, ST77XX_BLACK);
  tft.drawRect(   0, 143, 319, 97, ST77XX_CYAN);
  sprintf(pbuf, "%03d (0x%02x)", deviceIndex, deviceIndex);
  tft.setCursor(120,  80); tft.print(pbuf);
  tft.setCursor(100, 108); tft.print(partlist[partLUT[deviceIndex][detailIndex]]);
  tft.setCursor( 10, 160); tft.print(partdesc[partLUT[deviceIndex][detailIndex]]);
}

void drawCursorDetail(){
  tft.drawRect(242 + (detailPrevCursor * 26),  1, 26, 26, ST77XX_BLACK);
  tft.drawRect(242 + (detailCursor * 26),  1, 26, 26, ST77XX_YELLOW);
}

void cursorLeftDetail(){
  detailPrevCursor = detailCursor;
  detailCursor -= 1;
  if(detailCursor < 0) detailCursor = 2;
  drawCursorDetail();
}

void cursorRightDetail(){
  detailPrevCursor = detailCursor;
  detailCursor += 1;
  if(detailCursor > 2) detailCursor = 0;
  drawCursorDetail();
}

void detailNext(){
  detailIndex += 1;
  if(partLUT[deviceIndex][detailIndex] == -1)
    detailIndex -= 1;
  drawDetailBlock();
}

void detailPrevious(){
  detailIndex -= 1;
  if(detailIndex < 0)
    detailIndex = 0;
  drawDetailBlock();
}

void cursorSelectDetail(){
  switch(detailCursor){
    case 0:
      gridMode = false;
      drawMainPage();
      break;
    case 1:
      detailPrevious();
      break;
    case 2:
      detailNext();
      break;
  }
}

/*
 * UI event loop dispatch display functions
 */
void loop() {
  if (!ss.digitalRead(SS_SWITCH_UP)) {
    switch(uiState){
      case SCREENMAIN:
        cursorUpMain();
        break;
      case SCREENSCAN:
        if(gridMode){
          cursorUpGrid();
        }
        break;
      case SCREENDETAIL:
        break;
    }
  }
  if (!ss.digitalRead(SS_SWITCH_LEFT)) {
    switch(uiState){
      case SCREENMAIN:
        break;
      case SCREENSCAN:
        if(gridMode){
          cursorLeftGrid();
        } else {
          cursorLeftScan();
        }
        break;
      case SCREENDETAIL:
        cursorLeftDetail();
        break;
    }
  } 
  if (!ss.digitalRead(SS_SWITCH_RIGHT)) {
    switch(uiState){
      case SCREENMAIN:
        break;
      case SCREENSCAN:
        if(gridMode){
          cursorRightGrid();
        } else {
          cursorRightScan();
        }
        break;
      case SCREENDETAIL:
        cursorRightDetail();
        break;
    }
  }
  if (!ss.digitalRead(SS_SWITCH_DOWN)) {
    switch(uiState){
      case SCREENMAIN:
        cursorDownMain();
        break;
      case SCREENSCAN:
        if(gridMode){
          cursorDownGrid();
        }
        break;
      case SCREENDETAIL:
        break;
    }
  } 
  if (! ss.digitalRead(SS_SWITCH_SELECT)) {
    switch(uiState){
      case SCREENMAIN:
        cursorSelectMain();
        break;
      case SCREENSCAN:
        if(gridMode){
          cursorSelectGrid();
        } else {
          cursorSelectScan();
        }
        break;
      case SCREENDETAIL:
        cursorSelectDetail();
        break;
    }
  }

  int32_t encDelta = ss.getEncoderDelta();
  if (encDelta < 0) {
    backlightLevel -= 16;
    if(backlightLevel < 0) backlightLevel = 0;
    setBrightness(backlightLevel);
  } else if(encDelta > 0){
    backlightLevel += 16;
    if(backlightLevel > 255) backlightLevel = 255;
    setBrightness(backlightLevel);
  }

  if(continuousTest && (uiState == SCREENSCAN)) i2cScan();
  delay(250);
}

/*
 * I2C scan functions
 */
void i2cScan(){
  if(speedTest){
    i2cSpeedScan();
  } else {
    i2cNormalScan();
  }
}

void i2cNormalScan() {
  i2cScanner(100000, I2CFOUND1KHZ);
  drawI2C();
}

void i2cSpeedScan() {
  i2cScanner(100000, I2CFOUND1KHZ);
  i2cScanner(400000, I2CFOUND4KHZ);
  i2cScanner(1000000, I2CFOUND1MHZ);
  drawI2C();
}

/*
 * Wire.endTransmission returns:
 * 0: success.
 * 1: data too long to fit in transmit buffer.
 * 2: received NACK on transmit of address.
 * 3: received NACK on transmit of data.
 * 4: other error.
 * 5: timeout.
*/

void i2cScanner(int speed, int found) {
  byte error, address;

  Wire1.setClock(speed);

  for(address = 0; address < 128; address++ ){
    Wire1.beginTransmission(address);
    error = Wire1.endTransmission();

    switch(error){
      case 0:
        if((address < 0x07) || (address > 0x78)){
          setI2CBlock(address, I2CILLEGAL);
        } else {
          setI2CBlock(address, found);
          for(int i = 0; i < 32; i++){
            if(partLUT[address][i] == -1) break;
          }
        }
        break;
      case 1:
        setI2CBlock(address, I2CERROR);
        break;
      case 2:
        setI2CBlock(address, I2CNOTFOUND);
        break;
      case 3:
      case 4:
        setI2CBlock(address, I2CERROR);
        break;
      case 5:
        setI2CBlock(address, I2CNOTFOUND);
        break;
    }
  }
}
