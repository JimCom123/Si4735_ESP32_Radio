//  V3.0 27-07-2020 Bug-support and little improvements.
//  This sketch is based on the SI4735 Library of Ricardo PU2CLR. Thanks for the very nice work.

//  This sketch uses  a 2.8 inch 240*320 touch-screen with ILI9341, ESP32 WROOM-32 and Rotary Encoder.
//  The radio is fully controlled by the (Touch)Screen and Rotary Encoder
//  This sketch uses the Rotary Encoder Class implementation from Ben Buxton (the source code is included
//  together with this sketch).
//  For the touch-screen the library TFT_eSPI is used. The configuration setup-file: setup1_ILI9341 is also
//  included.
//  Also a schematic drawing is available.

//  ABOUT SSB PATCH:
//  This sketch will download a SSB patch to your SI4735 device (patch_init.h). It will take about 8KB of the Arduino memory.

//  In this context, a patch is a piece of software used to change the behavior of the SI4735 device.
//  There is little information available about patching the SI4735. The following information is the understanding of the author of
//  this project and it is not necessarily correct. A patch is executed internally (run by internal MCU) of the device.
//  Usually, patches are used to fixes bugs or add improvements and new features of the firmware installed in the internal ROM of the device.
//  Patches to the SI4735 are distributed in binary form and have to be transferred to the internal RAM of the device by
//  the host MCU (in this case Arduino). Since the RAM is volatile memory, the patch stored into the device gets lost when you turn off the system.
//  Consequently, the content of the patch has to be transferred again to the device each time after turn on the system or reset the device.

//  ATTENTION: The author of this project does not guarantee that procedures shown here will work in your development environment.
//  Given this, it is at your own risk to continue with the procedures suggested here.
//  This library works with the I2C communication protocol and it is designed to apply a SSB extension PATCH to CI SI4735-D60.
//  Once again, the author disclaims any liability for any damage this procedure may cause to your SI4735 or other devices that you are using.
//
//  Library TFT_eSPI you may download from here : https://github.com/Bodmer/TFT_eSPI
//  Library Rotary is provided with the program
//  Library SI4735 you may download from here   : https://github.com/pu2clr/SI4735
//
//  *********************************
//  **   Display connections etc.  **
//  *********************************
//  |------------|------------------|------------|------------|------------|
//  |Display 2.8 |      ESP32       |   Si4735   |  Encoder   |  Beeper    |
//  |  ILI9341   |                  |            |            |            |        Encoder        1,2,3
//  |------------|------------------|------------|------------|------------|        Encoder switch 4,5
//  |   Vcc      |     3V3     | 01 |    Vcc     |            |            |        pin 33 with 18K to 3.3 volt and 18K to ground.
//  |   GND      |     GND     | 02 |    GND     |     2,4    |            |        pin 32 (Beeper) via 2K to base V1  BC547
//  |   CS       |     15      | 03 |            |            |            |        Collector via beeper to 5v
//  |   Reset    |      4      | 04 |            |            |            |        Emmitor to ground
//  |   D/C      |      2      | 05 |            |            |            |
//  |   SDI      |     23      | 06 |            |            |            |        Encoder        1,2,3
//  |   SCK      |     18      | 07 |            |            |            |        Encoder switch 4,5
//  |   LED Coll.|     14 2K   | 08 |            |            |            |        Display LED
//  |   SDO      |             | 09 |            |            |            |        Emmitor  V2 BC557 to 3.3 V
//  |   T_CLK    |     18      | 10 |            |            |            |        Base with 2K to pin 14 (Display_Led)
//  |   T_CS     |      5      | 11 |            |            |            |        Collector to led pin display
//  |   T_DIN    |     23      | 12 |            |            |            |
//  |   T_DO     |     19      | 13 |            |            |            |
//  |   T_IRQ    |     34      | 14 |            |            |            |
//  |            |     12      |    |   Reset    |            |            |
//  |            |     21      |    |    SDA     |            |            |
//  |            |     22      |    |    SCL     |            |            |
//  |            |     16      |    |            |      1     |            |
//  |            |     17      |    |            |      3     |            |
//  |            |     33      |    |            |      5     |            |
//  |            |     32 2K   |    |            |            |     In     |
//  |            |     27 Mute |    |see schematics           |            |
//  |------------|-------------|----|------------|------------|------------|

#include <TFT_eSPI.h>
#include "LCD_Colors.h"
#include "DSEG7_Classic_Mini_Regular_20.h"
#include "DSEG7_Classic_Mini_Regular_30.h"
#include "DSEG7_Classic_Mini_Regular_34.h"
#include <SPI.h>
#include <SI4735.h>
#include "EEPROM.h"
#include "Rotary.h"
#include "Button.h"

// Test it with patch_init.h or patch_full.h. Do not try load both.
//#include "patch_init.h" // SSB patch for whole SSBRX initialization string
//#include "patch_full.h"    // SSB patch for whole SSBRX full download
#include "patch_3rd.h" // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

//// GPIO definitions
#define ESP32_I2C_SDA    21  // I2C bus pin on ESP32
#define ESP32_I2C_SCL    22  // I2C bus pin on ESP32
#define RESET_PIN        12
#define ENCODER_PIN_A    16 ////  // http://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
#define ENCODER_PIN_B    17 ////
#define ENCODER_SWITCH   33
#define BEEPER           32
#define Display_Led      14
#define AUDIO_MUTE       27 ////

#define BANDSW_SW        13  ////
#define BANDSW_SW2        0  ////
#define BANDSW_MW        26  ////
////

#define displayon         0
#define displayoff        1
#define beepOn            1
#define beepOff           0

#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

#define MIN_ELAPSED_TIME             100
#define MIN_ELAPSED_RSSI_TIME        150
#define MIN_ELAPSED_DISPL_TIME      2000 //// 10min   1000
//#define MIN_ELAPSED_RDS_TIME           5
#define DEFAULT_VOLUME                100 //// // change it for your favorite start sound volume
#define MIN_ELAPSED_VOLbut_TIME     1000

#define SWFREQ 1749    ////
#define MWFREQ 513     ////
#define SWLFREQ 3200   ////

////#define BFORANGE 10000  ////16000      //Hz before resetting maintuning on the SI4735 BFO

#define FM          0
#define LSB         1
#define USB         2
#define AM          3

bool bfoOn          = false;
bool ssbLoaded      = false;

bool FirstLayer = true;
bool SecondLayer = false;
bool ThirdLayer = false;
bool ForthLayer = false;
bool HamBand = false;
bool Modebut = false;
bool FREQbut = false;
bool Decipoint = false;
bool STEPbut = false;
bool BroadBand;
bool BandWidth;
bool MISCbut = false;
bool PRESbut = false;
bool VOLbut = false;
bool DISplay = false;
bool Mutestat = false;
bool AGCgainbut = false;
bool writingEeprom = false;

bool pressed;
bool press1;
bool audioMuteOn = true;
bool audioMuteOff = false;
bool RDS = true; // RDS on  or  off
bool SEEK =  false;

int16_t currentBFO;
int16_t previousBFO = 0;
int nrbox       = 0;
int OldRSSI;
int NewRSSI;
int NewSNR,   OldSNR;
int NewMULT,  OldMULT;  ////
int NewBLEND, OldBLEND;  ////
int encBut;
int AGCgain     = 0;

long elapsedRSSI        = millis();
long elapsedRDS         = millis();
long stationNameElapsed = millis();
long DisplayOnTime      = millis();
long VOLbutOnTime       = millis();

volatile int encoderCount  = 0;

uint16_t previousFrequency;
uint8_t currentStep        =  1;

uint8_t currentBFOStep     = 100;  ////25;
int16_t DisplayBFO = 0;   ////
int16_t BFORange;  ////

int8_t currentPRES        =  0;  ////
int8_t previousPRES       =  0;  ////
int8_t currentPRESStep    =  1;  ////

uint8_t currentAGCgain     =  1;
uint8_t previousAGCgain    =  1;
uint8_t currentAGCgainStep =  1;
uint8_t MaxAGCgain;
uint8_t MaxAGCgainFM       = 26;
uint8_t MaxAGCgainAM       = 37;
uint8_t MinAGCgain         =  1;

uint8_t currentVOL         =  0;
uint8_t previousVOL        =  0;
uint8_t currentVOLStep     =  1;
uint8_t MaxVOL             = 63;
uint8_t MinVOL             = 20;

uint8_t currentAGCAtt      =  0;
uint8_t bwIdxSSB;
uint8_t bwIdxAM;
uint8_t bandIdx;
uint8_t currentMode = FM;
uint8_t previousMode;
uint16_t x = 0, y = 0; // To store the touch coordinates
uint8_t encoderStatus;
int   freqDec = 0;
float Displayfreq      = 0;
float currentFrequency = 0;
float dpfrq            = 0;
float fact             = 1;

String BWtext;
String RDSbuttext;
String AGCgainbuttext;

const char *bandwidthSSB[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};
const char *bandwidthAM[]  = {"6.0", "4.0", "3.0", "2.0", "1.0", "1.8", "2.5"};
const char *Keypathtext[]  = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "SET", "CLR"};  ////
const char *bandModeDesc[] = {"FM ", "LSB", "USB", "A M"};

char buffer[64]; // Useful to handle string
char buffer1[64];

const char *stationName;
char bufferStatioName[40];

const int ledChannel = 0;
const int resolution = 1;

int16_t si4735Addr;

//=======================================================   Buttons First and Third Layer   ==========================
typedef struct // Buttons first layer
{
  const char *ButtonNam;
  
  int    ButtonNum;       // Button location at display from 0 to 11. To move around buttons freely at first layer.
  const char *ButtonNam1;
  int    ButtonNum1;      // Button location at display from 0 to 11. To move around buttons freely at third layer.
  int    XButos;          // Xoffset
  long   YButos;          // Yoffset
} Button;

int ytotoffset = 0;

//  Button table
int Xbutst  =   0;               // X Start location Buttons
int Ybutst  = 160 + ytotoffset;  // Y Start location Buttons

int Xsmtr   =   0;
int Ysmtr   =  85 + ytotoffset;  // S meter

int XVolInd =   0;
int YVolInd = 135 + ytotoffset;  // Volume indicator

int XFreqDispl  =   0;
int YFreqDispl  =   0 + ytotoffset;  // display

int Xbutsiz =  80;  //size of buttons first & third layer
int Ybutsiz =  40;

int Xbut0  = 0 * Xbutsiz ; int Ybut0   = 0 * Ybutsiz; // location calqualation for 12 first layer buttons
int Xbut1  = 1 * Xbutsiz ; int Ybut1   = 0 * Ybutsiz;
int Xbut2  = 2 * Xbutsiz ; int Ybut2   = 0 * Ybutsiz;
int Xbut3  = 3 * Xbutsiz ; int Ybut3   = 0 * Ybutsiz;
int Xbut4  = 0 * Xbutsiz ; int Ybut4   = 1 * Ybutsiz;
int Xbut5  = 1 * Xbutsiz ; int Ybut5   = 1 * Ybutsiz;
int Xbut6  = 2 * Xbutsiz ; int Ybut6   = 1 * Ybutsiz;
int Xbut7  = 3 * Xbutsiz ; int Ybut7   = 1 * Ybutsiz;
int Xbut8  = 3 * Xbutsiz ; long Ybut8  =-4 * Ybutsiz;
int Xbut9  = 3 * Xbutsiz ; long Ybut9  =-3 * Ybutsiz;
int Xbut10 = 3 * Xbutsiz ; long Ybut10 =-2 * Ybutsiz;
int Xbut11 = 3 * Xbutsiz ; long Ybut11 =-1 * Ybutsiz;

#define HAM       0
#define BFO       1
#define FREQ      2
#define AGC       3
#define MUTE      4
#define VOL       5
#define MODE      6
#define BANDW     7
#define STEP      8
#define BROAD     9
#define PRESET   10
#define NEXT     11

#define SEEKUP    0
#define SEEKDN    1
#define STATUS    2
#define RDSbut    3
#define AGCset    4
#define NR4       5
#define NR5       6
#define NR6       7
#define NR7       8   //                                                 |----|
#define NR8       9   //                                                 |  8 |
#define NR9      10   //                                                 |----|
#define PREV     11   //                                                 |  9 |
                      //                                                 |----|
                      //                                                 | 10 |
                      //                                                 |----|
Button bt[] = {       //                                                 | 11 |
  { "HAM"   ,  0 , "SEEKUP", 0 , Xbut0 , Ybut0  }, //     |----|----|----|----|
  { "BFO"   ,  4 , "SEEKDN", 4 , Xbut1 , Ybut1  }, //     |  0 |  1 |  2 |  3 |
  { "FREQ"  ,  2 , "STATUS", 6 , Xbut2 , Ybut2  }, //     |----|----|----|----|
  { "AGC"   ,  5 , "RDS"   , 8 , Xbut3 , Ybut3  }, //     |  4 |  5 |  6 |  7 |
  { "MUTE"  ,  9 , "AGCset", 1 , Xbut4 , Ybut4  }, //     |----|----|----|----|
  { "VOL"   ,  8 , ""      , 5 , Xbut5 , Ybut5  },
  { "MODE"  ,  3 , ""      ,10 , Xbut6 , Ybut6  },
  { "FILTER",  6 , ""      ,11 , Xbut7 , Ybut7  },
  { "STEP"  , 11 , ""      , 2 , Xbut8 , Ybut8  },
  { "BAND"  ,  1 , ""      , 3 , Xbut9 , Ybut9  },
  { "PRESET", 10 , ""      , 9 , Xbut10, Ybut10 },
  { "MENU"  ,  7 , "MENU"  , 7 , Xbut11, Ybut11 }
};

// You may freely move around the button (blue) position on the display to your flavour by changing the position in ButtonNum and ButtonNum1
// You have to stay in the First or Third Layer
//======================================================= End  Buttons First  and Third Layer   ======================

//======================================================= Tunings Steps     ===============================
typedef struct // Tuning steps
{
  uint16_t stepFreq;
  uint16_t Xstepos;          //Xoffset
  uint16_t Xstepsr;          //X size rectang
  uint16_t Ystepos;          //Yoffset
  uint16_t Ystepsr;          //Y size rectang
  uint16_t Ystepnr;          //Y next rectang
} Step;

//  Tuning steps table

  uint16_t Xfstep = 110;
  uint16_t Yfstep = 60;

Step sp[] = {
  { 1 , Xfstep, 100, Yfstep, 30,  0},
  { 5 , Xfstep, 100, Yfstep, 30, 30},
  { 9 , Xfstep, 100, Yfstep, 30, 60},
  { 10, Xfstep, 100, Yfstep, 30, 90}
};
//======================================================= End Tunings Steps     ===============================

//======================================================= Modulation Types     ================================
typedef struct // MODULATION
{
  uint16_t Modenum;
  uint16_t Xmodos;          //Xoffset
  uint16_t Xmodsr;          //X size rectang
  uint16_t Ymodos;          //Yoffset
  uint16_t Ymodsr;          //Y size rectang
  uint16_t Ymodnr;          //Y next rectang
} Mode;

//  Modulation table

  uint16_t Xfmod = 110;
  uint16_t Yfmod = 45;

Mode md[] = {
  { 0  , Xfmod, 100, Yfmod, 30,  0},
  { 1  , Xfmod, 100, Yfmod, 30, 30},
  { 2  , Xfmod, 100, Yfmod, 30, 60},
  { 3  , Xfmod, 100, Yfmod, 30, 90}
};
//======================================================= End Modulation Types     ============================

//======================================================= Keypath     =========================================
typedef struct // Keypath
{
  uint16_t KeypNum;
  uint16_t Xkeypos;          //Xoffset
  uint16_t Xkeypsr;          //X size rectang
  uint16_t Xkeypnr;          //Y next rectang
  uint16_t Ykeypos;          //Yoffset
  uint16_t Ykeypsr;          //X size rectang
  uint16_t Ykeypnr;          //Y next rectang
} Keypath;

//  Keypath table

uint16_t Xpath = 30;
uint16_t Ypath = 30;

Keypath kp[] = {
  {  0 , Xpath,  50 ,   0 , Ypath , 50 ,   0},
  {  1 , Xpath,  50 ,  50 , Ypath , 50 ,   0},
  {  2 , Xpath,  50 , 100 , Ypath , 50 ,   0},
  {  3 , Xpath,  50 ,   0 , Ypath , 50 ,  50},
  {  4 , Xpath,  50 ,  50 , Ypath , 50 ,  50},
  {  5 , Xpath,  50 , 100 , Ypath , 50 ,  50},
  {  6 , Xpath,  50 ,   0 , Ypath , 50 , 100},
  {  7 , Xpath,  50 ,  50 , Ypath , 50 , 100},
  {  8 , Xpath,  50 , 100 , Ypath , 50 , 100},
  {  9 , Xpath,  50 ,   0 , Ypath , 50 , 150},
  { 10 , Xpath,  50 ,  50 , Ypath , 50 , 150},
  { 11 , Xpath,  50 , 100 , Ypath , 50 , 150},
};

//======================================================= End Keypath     =====================================

//======================================================= Bandwidth AM & FM     ===============================
typedef struct // Bandwidth AM & SSB
{
  uint16_t BandWidthAM;
  uint16_t BandWidthSSB;
  uint16_t Xos;          //Xoffset
  uint16_t Xsr;          //X size rectang
  uint16_t Yos;          //Yoffset
  uint16_t Ysr;          //X size rectang
  uint16_t Ynr;          //Y next rectang
} Bandwidth;

//  Bandwidth table
uint16_t XfBW = 110;
uint16_t YfBW = 23;

Bandwidth bw[] = {
  { 4 , 4 , XfBW, 100, YfBW, 30,   0},
  { 5 , 5 , XfBW, 100, YfBW, 30,  30},
  { 3 , 0 , XfBW, 100, YfBW, 30,  60},
  { 6 , 1 , XfBW, 100, YfBW, 30,  90},
  { 2 , 2 , XfBW, 100, YfBW, 30, 120},
  { 1 , 3 , XfBW, 100, YfBW, 30, 150},
  { 0 , 0 , XfBW, 100, YfBW, 30, 180}
};
//======================================================= End Bandwidth AM & FM     ===========================

//======================================================= Broad Band Definitions     ==========================
typedef struct // Broad-Band switch
{
  uint16_t BbandNum; // bandIdx
  uint16_t Xbbandos;          //Xoffset
  uint16_t Xbbandsr;          //X size rectang
  uint16_t Xbbandnr;          //X next rectang
  uint16_t Ybbandos;          //Yoffset
  uint16_t Ybbandsr;          //X size rectang
  uint16_t Ybbandnr;          //Y next rectang
} BBandnumber;

//  Bandnumber table for the broad-bands

uint16_t Xfbband = 40;
uint16_t Yfbband = 15;

BBandnumber bb[] = {
  {  0 , Xfbband, 80 ,   0 , Yfbband , 30 ,   0}, // 0
  {  1 , Xfbband, 80 ,   0 , Yfbband , 30 ,  30}, // 1
  {  2 , Xfbband, 80 ,   0 , Yfbband , 30 ,  60}, // 2
  {  6 , Xfbband, 80 ,   0 , Yfbband , 30 ,  90}, // 3
  {  7 , Xfbband, 80 ,   0 , Yfbband , 30 , 120}, // 4
  {  9 , Xfbband, 80 ,   0 , Yfbband , 30 , 150}, // 5
  { 11 , Xfbband, 80 ,   0 , Yfbband , 30 , 180}, // 6
  { 13 , Xfbband, 80 ,   80 , Yfbband , 30 , 0}, // 7
  { 14 , Xfbband, 80 ,   80 , Yfbband , 30 , 30}, // 8
  { 16 , Xfbband, 80 , 80 , Yfbband , 30 ,   60}, // 9
  { 17 , Xfbband, 80 , 80 , Yfbband , 30 ,  90}, //10
  { 19 , Xfbband, 80 , 80 , Yfbband , 30 ,  120}, //11
  { 21 , Xfbband, 80 , 80 , Yfbband , 30 ,  150}, //12
  { 22 , Xfbband, 80 , 80 , Yfbband , 30 , 180}, //13
  { 24 , Xfbband, 80 , 160 , Yfbband , 30 , 0}, //14
  { 26 , Xfbband, 80 , 160 , Yfbband , 30 , 30}, //15
  { 27 , Xfbband, 80 , 160 , Yfbband , 30 , 60}, //16
  { 29 , Xfbband, 80 , 160 , Yfbband , 30 , 90}, //17

};
//======================================================= End Broad Band Definitions     ======================

//======================================================= Ham Band Definitions     ============================
typedef struct // Ham Band switch
{
  uint16_t BandNum; // bandIdx
  uint16_t HamBandTxt;
  uint16_t Xbandos;          //Xoffset
  uint16_t Xbandsr;          //X size rectang
  uint16_t Xbandnr;          //X next rectang
  uint16_t Ybandos;          //Yoffset
  uint16_t Ybandsr;          //Y size rectang
  uint16_t Ybandnr;          //Y next rectang
} Bandnumber;

//  Bandnumber table for the hambands

  uint16_t Xfband = 50;
  uint16_t Yfband = 30;

Bandnumber bn[] = {
  {  3 , 0 , Xfband, 110 ,   0 , Yfband , 30 ,   0},
  {  4 , 1 , Xfband, 110 ,   0 , Yfband , 30 ,  30},
  {  5 , 2 , Xfband, 110 ,   0 , Yfband , 30 ,  60},
  {  8 , 3 , Xfband, 110 ,   0 , Yfband , 30 ,  90},
  { 10 , 4 , Xfband, 110 ,   0 , Yfband , 30 , 120},
  { 12 , 5 , Xfband, 110 ,   0 , Yfband , 30 , 150},
  { 15 , 6 , Xfband, 110 , 110 , Yfband , 30 ,   0},
  { 18 , 7 , Xfband, 110 , 110 , Yfband , 30 ,  30},
  { 20 , 8 , Xfband, 110 , 110 , Yfband , 30 ,  60},
  { 23 , 9 , Xfband, 110 , 110 , Yfband , 30 ,  90},
  { 25 , 10 , Xfband, 110 , 110 , Yfband , 30 , 120},
  { 28 , 11 , Xfband, 110 , 110 , Yfband , 30 , 150}
};
//======================================================= End Ham Band Definitions     ========================

//======================================================= THE Band Definitions     ============================
typedef struct // Band data
{
  const char *bandName; // Bandname
  uint8_t  bandType;    // Band type (FM, MW or SW)
  uint16_t prefmod;     // Pref. modulation
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Default step (increment and decrement)
  //float BFOf1;            // BFO set for f1
  //float F1b;              // Freq1 in kHz
  //float BFOf2;            // BFO set for f2
  //float F2b;              // Freq2 in kHz
} Band;

//   Band table

Band band[] = {  ////
  {   "FM", FM_BAND_TYPE,  FM,  6400, 10800,  8810,10}, //  FM          0  ////
  {   "LW", LW_BAND_TYPE,  AM,   150,   280,   164, 1}, //  LW          1  ////
  {   "MW", MW_BAND_TYPE,  AM,   522,  1701,   666, 9}, //  MW          2  ////
  {"2220M", LW_BAND_TYPE, LSB,   130,   140,   135, 5}, // Ham          3  ////
  { "630M", LW_BAND_TYPE, LSB,   420,   520,   475, 5}, // Ham  630M    4
  { "160M", SW_BAND_TYPE, LSB,  1800,  1920,  1910, 5}, // Ham  160M    5  ////
  { "120M", SW_BAND_TYPE,  AM,  2300,  2860,  2500, 5}, //      120M    6
  {  "90M", SW_BAND_TYPE,  AM,  3200,  3400,  3250, 5}, //       90M    7  ////
  {  "80M", SW_BAND_TYPE, LSB,  3500,  3810,  3540, 5}, // Ham   80M    8  ////
  {  "75M", SW_BAND_TYPE,  AM,  3900,  4000,  3925, 5}, //       75M    9  ////
  {  "60M", SW_BAND_TYPE, USB,  5330,  5410,  5375, 5}, // Ham   60M   10
  {  "49M", SW_BAND_TYPE,  AM,  5900,  6200,  6055, 5}, //       49M   11  ////
  {  "40M", SW_BAND_TYPE, LSB,  7000,  7200,  7100, 5}, // Ham   40M   12  ////
  {  "41M", SW_BAND_TYPE,  AM,  7200,  7600,  7580, 5}, //       41M   13  ////
  {  "31M", SW_BAND_TYPE,  AM,  9400,  9900,  9665, 5}, //       31M   14  ////
  {  "30M", SW_BAND_TYPE, USB, 10100, 10150, 10120, 5}, // Ham   30M   15
  {  "25M", SW_BAND_TYPE,  AM, 11600, 12100, 11680, 5}, //       25M   16  ////
  {  "22M", SW_BAND_TYPE,  AM, 13570, 13870, 13700, 5}, //       22M   17
  {  "20M", SW_BAND_TYPE, USB, 14000, 14350, 14200, 5}, // Ham   20M   18
  {  "19M", SW_BAND_TYPE,  AM, 15100, 15830, 15280, 5}, //       19M   19
  {  "17M", SW_BAND_TYPE, USB, 18000, 18170, 18100, 5}, // Ham   17M   20
  {  "16M", SW_BAND_TYPE,  AM, 17480, 17900, 17600, 5}, //       16M   21
  {  "15M", SW_BAND_TYPE,  AM, 18900, 19020, 18950, 5}, //       15M   22
  {  "15M", SW_BAND_TYPE, USB, 21000, 21450, 21250, 5}, // Ham   15M   23  ////
  {  "13M", SW_BAND_TYPE,  AM, 21450, 21850, 21500, 5}, //       13M   24
  {  "12M", SW_BAND_TYPE, USB, 24890, 24990, 24940, 5}, // Ham   12M   25
  {  "11M", SW_BAND_TYPE,  AM, 25670, 26100, 25800, 5}, //       11M   26
  {   "CB", SW_BAND_TYPE,  AM, 26200, 28000, 27000, 1}, // CB band     27  ////
  {  "10M", SW_BAND_TYPE, USB, 28000, 30000, 28500, 5}, // Ham   10M   28
  {   "SW", SW_BAND_TYPE,  AM,  1730, 30000, 10000, 5}  // Whole SW    29  ////
};
//======================================================= End THE Band Definitions     ========================

//======================================================= FM Presets     ======================================
typedef struct // Preset data
{
  float      presetIdx;
  const char *PresetName;
} FM_Preset ;

FM_Preset preset[] = {   ////
  7920  , "NHK-BK Bay", // 01 
  7800  , "NHK-PP Bay", // 02 
  7870  , "Sakura FM",  // 03 
  7650  , "FM COCOLO",  // 04 
  8020  , "FM802",      // 05 
  8510  , "FM OSAKA",   // 06 
  8250  , "KissFM Bay", // 07 
  8340  , "a-Station",  // 08 
  8650  , "NHK-PP",     // 09 
  8810  , "NHK-BK",     // 10 
  8990  , "Kiss FM",    // 11 
  9110  , "AM558",      // 12 
  9060  , "MBS",        // 13 
  9330  , "ABC",        // 14 
  9190  , "OBC",        // 15 
  7970  , "JimCom",     // 
};

//======================================================= END FM Presets     ======================================

const int lastButton = (sizeof bt / sizeof(Button)) - 1;
const int lastBand   = (sizeof band / sizeof(Band)) - 1;
const int lastHam = (sizeof bn / sizeof(Bandnumber)) - 1;
const int lastBroad = (sizeof bb / sizeof(BBandnumber)) - 1;
const int lastMod = (sizeof md / sizeof(Mode)) - 1;
const int lastBW = (sizeof bw / sizeof(Bandwidth)) - 1;
const int lastStep = (sizeof sp / sizeof(Step)) - 1;
const int lastKPath = (sizeof kp / sizeof(Keypath)) - 1;
const int lastPreset = (sizeof preset / sizeof (FM_Preset)) - 1;

#define offsetEEPROM       0x30
#define EEPROM_SIZE        150

struct StoreStruct {
  byte     chkDigit;
  byte     bandIdx; 
  uint16_t Freq; 
  uint8_t  currentMode;
  uint8_t  bwIdxSSB;
  uint8_t  bwIdxAM;
  uint8_t  currentStep;
  int16_t  currentBFO;  ////  int      currentBFO;
  uint8_t  currentAGCAtt;
  uint8_t  currentVOL;
  uint8_t  currentBFOStep;
  uint8_t  RDS;
};

StoreStruct storage = {
  '!',  //First time check
    0,  //bandIdx 
 8810,  //Freq  ////
    0,  //mode
    1,  //bwIdxSSB
    1,  //bwIdxAM  //// 3
    5,  //currentStep //// 9
    0,  //currentBFO  //// -125
    2,  //currentAGCAtt
   100,  //currentVOL //// 45
   100,  //currentBFOStep  ////25
    1   //RDS
};

uint8_t snr = 0;  ////
uint8_t mult = 0; ////
uint8_t blend = 0; ////
uint8_t rssi = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

class MyCustomSI4735 : public SI4735 { // extending the original class SI4735
#define FM_DEEMPHASIS 0x1100
#define FM_DEEMPHASIS_50US 0x0001
#define FM_DEEMPHASIS_75US 0x0002

#define FM_BLEND_RSSI_ST_THR 0x1800
#define FM_BLEND_RSSI_MN_THR 0x1801
#define FM_BLEND_SNR_ST_THR  0x1804
#define FM_BLEND_SNR_MN_THR  0x1805
#define FM_BLEND_MULTIPATH_ST_THR 0x1808
#define FM_BLEND_MULTIPATH_MN_THR 0x1809

  public:
    void setFmDeemphasis(uint16_t de = FM_DEEMPHASIS_50US) {
        sendProperty(FM_DEEMPHASIS, de);
    };

    void setFmBlendRsStereo(uint16_t thr = 38) { //  // 49dBuV = 0x0031
        sendProperty(FM_BLEND_RSSI_ST_THR, thr);
    };

    void setFmBlendRsMono(uint16_t thr = 20) { // // 30dBuV = 0x001e
        sendProperty(FM_BLEND_RSSI_MN_THR, thr);
    };

    void setFmBlendSnrStereo(uint16_t thr = 26) { // default 26dB
        sendProperty(FM_BLEND_SNR_ST_THR, thr);
    };

    void setFmBlendSnrMono(uint16_t thr = 12) { // default 12dB
        sendProperty(FM_BLEND_SNR_MN_THR, thr);
    };
};
MyCustomSI4735 si4735; // the instance of your custom class based on SI4735 class
////SI4735 si4735;

//=======================================================================================
void IRAM_ATTR RotaryEncFreq() {
//=======================================================================================
  // rotary encoder events
  if (!writingEeprom){
    encoderStatus = encoder.process();
    
    if (encoderStatus)
    {
      if (encoderStatus == DIR_CW)// Direction clockwise
      {
        encoderCount = 1;
      }
      else
      {
        encoderCount = -1;
      }
    }
  }
}

//=======================================================================================
void setup() {
//=======================================================================================
  pinMode(Display_Led, OUTPUT);  ////
  digitalWrite(Display_Led, displayoff);  ////

  pinMode(BANDSW_SW, OUTPUT);  ////
  pinMode(BANDSW_SW2, OUTPUT);  ////
  pinMode(BANDSW_MW, OUTPUT);  ////
  digitalWrite(BANDSW_SW, 0);  ////
  digitalWrite(BANDSW_SW2, 0);  ////
  digitalWrite(BANDSW_MW, 0);  ////

  pinMode(BEEPER, OUTPUT);
////  Serial.begin(115200);  ////
  DISplay = true;
  Beep(2, 200);

  tft.init();
  tft.setRotation(1);
  
  //tft.setRotation(0); // Rotate 0
  //tft.setRotation(1); // Rotate 90
  //tft.setRotation(2); // Rotate 180
  //tft.setRotation(3); // Rotate 270

    // Use this calibration code in setup():
  uint16_t calData[5] = { 387, 3530, 246, 3555, 7 };
////     uint16_t calData[5] = { 370, 3492, 277, 3472, 7 };
  tft.setTouch(calData);

  tft.fillScreen(COLOR_BACKGROUND); ////
  digitalWrite(Display_Led, displayon);  ////

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    tft.fillScreen(COLOR_BACKGROUND); ////
    tft.setCursor(0, 0);
    tft.println(F("failed to initialise EEPROM"));
////    Serial.println(F("failed to initialise EEPROM"));
    while(1); 
  }

  if (EEPROM.read(offsetEEPROM) != storage.chkDigit){
////    Serial.println(F("Writing defaults...."));
    saveConfig();
  }
  loadConfig();
  printConfig();
  

  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL); //I2C for SI4735

  // Encoder pins
  pinMode(ENCODER_PIN_A , INPUT_PULLUP); //Rotary encoder Freqency/bfo/preset
  pinMode(ENCODER_PIN_B , INPUT_PULLUP);
  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), RotaryEncFreq, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), RotaryEncFreq, CHANGE);

  si4735.setAudioMuteMcuPin(AUDIO_MUTE);

  si4735Addr = si4735.getDeviceI2CAddress(RESET_PIN);
  if (si4735Addr == 17)
  {
    si4735.setDeviceI2CAddress(0);
  }
  else
  {
    si4735.setDeviceI2CAddress(1);
  }

  // Setup the radio from last setup in EEPROM
 
  bandIdx                   = storage.bandIdx;
  band[bandIdx].currentFreq = storage.Freq;
  currentMode               = storage.currentMode;
  bwIdxSSB                  = storage.bwIdxSSB;
  bwIdxAM                   = storage.bwIdxAM;
  currentStep               = storage.currentStep;
  currentBFO                = storage.currentBFO;
  currentAGCAtt             = storage.currentAGCAtt;
  currentVOL                = storage.currentVOL;
  currentBFOStep            = storage.currentBFOStep;
  RDS                       = storage.RDS;
  BFORange = currentStep*1000;  ////

  si4735.setRefClock(32768);
  si4735.setRefClockPrescaler(1);   // will work with 32768  

  if (bandIdx == 0)  si4735.setup(RESET_PIN, -1, POWER_UP_FM, SI473X_ANALOG_AUDIO, XOSCEN_RCLK); // Start in FM
  else si4735.setup(RESET_PIN, -1, POWER_UP_AM, SI473X_ANALOG_AUDIO, XOSCEN_RCLK); // Start in AM
////  if (bandIdx == 0)  si4735.setup(RESET_PIN, 0); // Start in FM
////  else si4735.setup(RESET_PIN, 1); // Start in AM

  if (bandIdx != 0) si4735.setAM();

  previousBFO = -1;
  si4735.setVolume(currentVOL);
  previousVOL = currentVOL;

  BandSet();
  if (currentStep != band[bandIdx].currentStep ) band[bandIdx].currentStep = currentStep;
  currentFrequency = previousFrequency = si4735.getFrequency();
  encBut = 600;
  x = y = 0;
  DrawFila();
  si4735.setSeekFmSpacing(10);        
  si4735.setSeekFmLimits(6400,10800); ////
  si4735.setSeekAmRssiThreshold(50);
  si4735.setSeekAmSrnThreshold(20);
  si4735.setSeekFmRssiThreshold(5);
  si4735.setSeekFmSrnThreshold(5);

  xTaskCreate(SaveInEeprom, "SaveInEeprom", 2048, NULL, 1, NULL); 

}// end setup
//=======================================================================================
//=======================================================================================

//=======================================================================================
void SaveInEeprom (void* arg)  {
//=======================================================================================  
  while(1) {     
    storage.bandIdx = bandIdx;
    storage.Freq =  band[bandIdx].currentFreq;
    storage.currentMode = currentMode;
    storage.bwIdxSSB = bwIdxSSB;
    storage.bwIdxAM = bwIdxAM;
    storage.currentStep = currentStep;
    storage.currentBFO = currentBFO;
    storage.currentAGCAtt = currentAGCAtt;
    storage.currentVOL = currentVOL;
    storage.currentBFOStep = currentBFOStep;
    storage.RDS = RDS;
    for (unsigned int t = 0; t < sizeof(storage); t++) {
      delay(1);
      if (EEPROM.read(offsetEEPROM + t) != *((char*)&storage + t)){
        delay(1);
        EEPROM.write(offsetEEPROM + t, *((char*)&storage + t));
      } 
    }  
    writingEeprom = true;
    EEPROM.commit();
    writingEeprom = false;
    vTaskDelay(5000 / portTICK_RATE_MS);
  }
}

//=======================================================================================
void saveConfig() {
//=======================================================================================
  delay(10);
  for (unsigned int t = 0; t < sizeof(storage); t++) {
    if (EEPROM.read(offsetEEPROM + t) != *((char*)&storage + t)){
      EEPROM.write(offsetEEPROM + t, *((char*)&storage + t));
    } 
  }  
  EEPROM.commit();
}

//=======================================================================================
void loadConfig() {
//=======================================================================================  
  if (EEPROM.read(offsetEEPROM + 0) == storage.chkDigit) {
    for (unsigned int t = 0; t < sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(offsetEEPROM + t);
////    Serial.println("Load config done");  
  }    
}

//=======================================================================================
void printConfig() {
//=======================================================================================
////  Serial.println(sizeof(storage));
  if (EEPROM.read(offsetEEPROM) == storage.chkDigit){
    for (unsigned int t = 0; t < sizeof(storage); t++)
     ;
////      Serial.write(EEPROM.read(offsetEEPROM + t)); 
////    Serial.println();
    //setSettings(0);
  }
}

//=======================================================================================
void BandSet()  {
//=======================================================================================
  if (bandIdx == 0) currentMode = 0;// only mod FM in FM band
  if ((currentMode == AM) or (currentMode == FM)) ssbLoaded = false;

  if ((currentMode == LSB) or  (currentMode == USB))
  {
    if (ssbLoaded == false) {
      loadSSB();
    }
  }
  useBand();
  setBandWidth();
}

//=======================================================================================
void useBand()  {
//=======================================================================================
  tft.setTextColor(COLOR_PANEL_TEXT, COLOR_BACKGROUND);  ////
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    bfoOn = false;
    si4735.setTuneFrequencyAntennaCapacitor(0);
    delay(100);
    si4735.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
////    si4735.setFMDeEmphasis(1);
////
    si4735.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    si4735.setFmDeemphasis(FM_DEEMPHASIS_50US);
    si4735.setFmBlendRsStereo();
    si4735.setFmBlendRsMono();
    si4735.setFmBlendSnrStereo();
    si4735.setFmBlendSnrMono();
////
    ssbLoaded = false;
    si4735.RdsInit();
    si4735.setRdsConfig(1, 2, 2, 2, 2);
    si4735.setTuneFrequencyAntennaCapacitor(0); ////
  }
  else
  {
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) {
      si4735.setTuneFrequencyAntennaCapacitor(0);
    } else { //SW_BAND_TYPE
      si4735.setTuneFrequencyAntennaCapacitor(0); ////1);
    }
    if (ssbLoaded)
    {
      si4735.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      si4735.setSSBAutomaticVolumeControl(1);
      //si4735.setSsbSoftMuteMaxAttenuation(0); // Disable Soft Mute for SSB    
      //si4735.setSSBDspAfc(0);
      //si4735.setSSBAvcDivider(3);
      //si4735.setSsbSoftMuteMaxAttenuation(8); // Disable Soft Mute for SSB
      //si4735.setSBBSidebandCutoffFilter(0);
      currentStep =band[bandIdx].currentStep;  ////
      BFORange = currentStep*1000;  ////
      currentBFO = 0; ////
      si4735.setSSBBfo(currentBFO);     
      si4735.setTuneFrequencyAntennaCapacitor(0);  ////
    }
    else
    {
      si4735.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      currentStep =band[bandIdx].currentStep;  ////
      BFORange = currentStep*1000;  ////
      currentBFO = 0; ////
      //si4735.setAutomaticGainControl(1, 0);
      //si4735.setAmSoftMuteMaxAttenuation(0); // // Disable Soft Mute for AM
      bfoOn = false;
      si4735.setTuneFrequencyAntennaCapacitor(0);  ////
    }

  }
  delay(100);
}// end useband

//=======================================================================================
void setBandWidth()  {
//=======================================================================================
  if (currentMode == LSB || currentMode == USB)
  {
    si4735.setSSBAudioBandwidth(bwIdxSSB);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bwIdxSSB == 0 || bwIdxSSB == 4 || bwIdxSSB == 5)
      si4735.setSBBSidebandCutoffFilter(0);
    else
      si4735.setSBBSidebandCutoffFilter(1);
  }
  else if (currentMode == AM)
  {
    si4735.setBandwidth(bwIdxAM, 1); //// , 0);
  }
}

//=======================================================================================
void loadSSB()  {
//=======================================================================================
  si4735.reset();
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.setI2CFastMode(); // Recommended
  //si4735.setI2CFastModeCustom(500000); // It is a test and may crash.
  //  si4735.setI2CFastModeCustom(800000); // It is a test and may crash. ////
  si4735.downloadPatch(ssb_patch_content, size_content);
  si4735.setI2CStandardMode(); // goes back to default (100kHz)

  // delay(50);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bwIdxSSB, 1, 0, 0, 0, 1);
  delay(25);
  ssbLoaded = true;
  bfoOn = true; ////

}

//=======================================================================================
void Freqcalq(int keyval)  {
//=======================================================================================
  if (Decipoint) {
    dpfrq = dpfrq + keyval / fact;
  }
  else Displayfreq = (Displayfreq + keyval) * 10;
  fact = fact * 10;
  tft.setTextSize(2);
  tft.setTextColor(COLOR_INDICATOR_FREQ, COLOR_BACKGROUND);  ////
  tft.setCursor(50, 0);
  if (Decipoint) {
    tft.print((Displayfreq / 10) + dpfrq, 4);
    tft.print(" MHz");
  }
  else {
    tft.print((Displayfreq / 10) + dpfrq, 0);
    if (((Displayfreq / 10) + dpfrq) <= 30000) tft.print(" kHz");
    else tft.print(" Hz");
  }
}

//=======================================================================================
void Smeter() {
//=======================================================================================
  int spoint;
  if (currentMode != FM) {
    //dBuV to S point conversion HF
    if ((rssi >= 0) and (rssi <=  1)) spoint =  12;                    // S0
    if ((rssi >  1) and (rssi <=  1)) spoint =  24;                    // S1
    if ((rssi >  2) and (rssi <=  3)) spoint =  36;                    // S2
    if ((rssi >  3) and (rssi <=  4)) spoint =  48;                    // S3
    if ((rssi >  4) and (rssi <= 10)) spoint =  48+(rssi- 4)*2;        // S4
    if ((rssi > 10) and (rssi <= 16)) spoint =  60+(rssi-10)*2;        // S5
    if ((rssi > 16) and (rssi <= 22)) spoint =  72+(rssi-16)*2;        // S6
    if ((rssi > 22) and (rssi <= 28)) spoint =  84+(rssi-22)*2;        // S7
    if ((rssi > 28) and (rssi <= 34)) spoint =  96+(rssi-28)*2;        // S8
    if ((rssi > 34) and (rssi <= 44)) spoint = 108+(rssi-34)*2;        // S9
    if ((rssi > 44) and (rssi <= 54)) spoint = 124+(rssi-44)*2;        // S9 +10
    if ((rssi > 54) and (rssi <= 64)) spoint = 140+(rssi-54)*2;        // S9 +20
    if ((rssi > 64) and (rssi <= 74)) spoint = 156+(rssi-64)*2;        // S9 +30
    if ((rssi > 74) and (rssi <= 84)) spoint = 172+(rssi-74)*2;        // S9 +40
    if ((rssi > 84) and (rssi <= 94)) spoint = 188+(rssi-84)*2;        // S9 +50
    if  (rssi > 94)                   spoint = 204;                    // S9 +60
    if  (rssi > 95)                   spoint = 208;                    //>S9 +60
////
    mult = 0;
    blend = 0;

  }
  else
  {
    //dBuV to S point conversion FM
    if  (rssi <  1) spoint = 36;
    if ((rssi >  1) and (rssi <=  2)) spoint =  60;                    // S6
    if ((rssi >  2) and (rssi <=  8)) spoint =  84+(rssi- 2)*2;        // S7 
    if ((rssi >  8) and (rssi <= 14)) spoint =  96+(rssi- 8)*2;        // S8
    if ((rssi > 14) and (rssi <= 24)) spoint = 108+(rssi-14)*2;        // S9
    if ((rssi > 24) and (rssi <= 34)) spoint = 124+(rssi-24)*2;        // S9 +10
    if ((rssi > 34) and (rssi <= 44)) spoint = 140+(rssi-34)*2;        // S9 +20
    if ((rssi > 44) and (rssi <= 54)) spoint = 156+(rssi-44)*2;        // S9 +30
    if ((rssi > 54) and (rssi <= 64)) spoint = 172+(rssi-54)*2;        // S9 +40
    if ((rssi > 64) and (rssi <= 74)) spoint = 188+(rssi-64)*2;        // S9 +50
    if  (rssi > 74)                   spoint = 204;                    // S9 +60
    if  (rssi > 76)                   spoint = 208;                    //>S9 +60
  }
  
////  tft.fillRect(Xsmtr + 15, Ysmtr + 38 , (2 + spoint), 6, TFT_RED);
////  tft.fillRect(Xsmtr + 17 + spoint, Ysmtr + 38 , 212 - (2 + spoint), 6, TFT_GREEN);
////
  tft.fillRect(Xsmtr + 15, Ysmtr + 31 , (2 + spoint), 6, COLOR_RSMTRF);
  tft.fillRect(Xsmtr + 17 + spoint, Ysmtr + 31 , 212 - (2 + spoint), 6, COLOR_RSMTRB);

  tft.fillRect(Xsmtr + 15, Ysmtr + 41 , (2 + snr), 6, COLOR_SNRMTRF);   ////
  tft.fillRect(Xsmtr + 17 + snr, Ysmtr + 41 , 212 - (2 + snr), 6, COLOR_SNRMTRB);  ////

  tft.fillRect(Xsmtr + 15, Ysmtr + 61 , (2 + mult), 6, COLOR_MULTMTRF);   ////
  tft.fillRect(Xsmtr + 17 + mult, Ysmtr + 61 , 212 - (2 + mult), 6, COLOR_MULTMTRB);  ////

  tft.fillRect(Xsmtr + 15, Ysmtr + 71 , (2 + blend), 6, COLOR_BLNDMTRF);   ////
  tft.fillRect(Xsmtr + 17 + blend, Ysmtr + 71 , 212 - (2 + blend), 6, COLOR_BLNDMTRB);  ////
////
}

//=======================================================================================
void VolumeIndicator(int vol) {
//=======================================================================================
  vol = map(vol, 20, 63, 0, 212);
  tft.fillRect(XVolInd + 15, Ysmtr+ 58 , (2 + vol), 2, COLOR_VOLF); ////16 , (2 + vol), 2, COLOR_VOLF); ////
  tft.fillRect(XVolInd + 17 + vol, Ysmtr+ 58 , 212 - (2 + vol), 2, COLOR_VOLB); ////16 , 212 - (2 + vol), 2, COLOR_VOLB); ////
}

//=======================================================================================
void loop() {
//=======================================================================================
 
  if ((FirstLayer == true) or (ThirdLayer == true)) VolumeIndicator(si4735.getVolume());

  // Pressed will be set true is there is a valid touch on the screen
  while (((pressed == false) and (encoderCount == 0) and (encBut > 500)) or (writingEeprom)) {  // wait loop  
    pressed = tft.getTouch(&x, &y);
    encBut = analogRead(ENCODER_SWITCH);
    showtimeRSSI();
    DisplayRDS();
    Dispoff();
  }

  encoderCheck();        // Check if the encoder has moved.
  encoderButtonCheck();  // Check if encoderbutton is pressed

  if (pressed) {
    pressed = false;
    PRESbut = false; // Preset stopped after other button is pressed

    DisplayOnTime = millis();
    if (DISplay == false) {
      Beep(1, 0);
      delay(400);
      digitalWrite(Display_Led, displayon);
      DISplay = true;
      x = y = 0; // no valid touch only for switch on display led
    }
    if (FirstLayer) { //==================================================
      //Check which button is pressed in First Layer.
      for (int n = 0 ; n <= lastButton; n++) {
        if ((x > (Xbutst + (bt[bt[n].ButtonNum].XButos))) and (x < Xbutst + (bt[bt[n].ButtonNum].XButos) + Xbutsiz) and (y > Ybutst + (bt[bt[n].ButtonNum].YButos)) and (y < Ybutst + (bt[bt[n].ButtonNum].YButos) + Ybutsiz)) {
          Beep(1, 0);
          delay(400);
          x = 0;
          y = 0;

          if ((VOLbut == true) and (n != VOL)) {
            VOLbut = false;
            drawVOL();
            DrawDispl ();
          }

          if ((Mutestat == true) and (n != MUTE)) {
            Mutestat = false;
            drawMUTE();
          }

           if ((bfoOn == true) and (n == VOL)) {
            bfoOn = false;
            drawBFO();
          } 

          if (n == HAM) {
            delay(400);  //HamBand button
            HamBand = true;
            HamBandlist();
            FirstLayer = false;
            SecondLayer = true;
          }

          if (n == BFO) {           //==============================  // BFO button
            delay(400);

            if (currentMode == LSB || currentMode == USB)  {
              if (bfoOn == false) {
                bfoOn = true;
              } else {
                bfoOn = false;
              }
              drawBFO();
              DrawDispl ();
            } else Beep(4, 100);
          }

          if (n == FREQ) {          //============================  // Frequency input
            delay(400);
            FREQbut = true;
            Decipoint = false;
            Displayfreq = 0;
            dpfrq = 0;
            drawKeyPath();
            FirstLayer = false;
            SecondLayer = true;
          }

          if (n == AGC) {           //============================//AGC switch
            si4735.getAutomaticGainControl();
            AGCgain = 0;
            if  (si4735.isAgcEnabled()) {
              si4735.setAutomaticGainControl(1, 0);     //    disabled
            } else {
              AGCgainbut = false;
              si4735.setAutomaticGainControl(0, 0);      //   enabled
            }
            drawAGC();
            DrawDispl ();
          }

          if (n == MODE) {    //============================= MODE
            if (currentMode != FM)  {
              delay(400);// Mode
              Modebut = true;
              Modelist();
              FirstLayer = false;
              SecondLayer = true;
            } else Beep(4, 100);
          }

          if (n == BANDW) {        //=========================BANDWIDTH
            delay(400);
            if (currentMode != FM)  {
              BandWidth = true;
              BWList();
              FirstLayer = false;
              SecondLayer = true;
            } else Beep(4, 100);
          }

          if (n == STEP) {            //========================= STEPS for tune and bfo
            delay(400);
            if (currentMode != FM)  {
              if (bfoOn) setStep();
              else {
                STEPbut = true;
                Steplist();
                FirstLayer = false;
                SecondLayer = true;
              }
            } else Beep(4, 100);
          }

          if (n == BROAD)  {
            delay(400);
            BroadBand = true;
            BroadBandlist();
            FirstLayer = false;
            SecondLayer = true;
          }
          if (n == PRESET) {
            delay(400);
            x = 0;
            y = 0;
            PRESbut = true;
            //tft.fillScreen(COLOR_BACKGROUND);  ////
            FirstLayer = false;
            SecondLayer = true;
          }

          if (n == VOL) {
            delay(400);
            x = 0;
            y = 0;
            if (VOLbut == false) {
              VOLbut = true;
              currentVOL = si4735.getVolume();
              previousVOL = currentVOL;
              FirstLayer = false;
              SecondLayer = true;
            }
            else {
              VOLbut = false;
              FirstLayer = true;
              SecondLayer = false;
            }
            FreqDispl();
            drawVOL();
          }

          if (n == MUTE) {
            delay(200);
            x = 0;
            y = 0;
            if (Mutestat == false)  {
              Mutestat = true;
            }
            else  {
              Mutestat = false;
            }
            drawMUTE();
          }

          if (n == NEXT) {
            delay(200);
            x = 0;
            y = 0;
            FirstLayer  = false;
            SecondLayer = false;
            ThirdLayer  = true;
            ForthLayer  = false;
            DrawThla();
          }
        }
      }
    } // end FirstLayer

    if (SecondLayer) {  //===============================================================
      if (Modebut == true) {
        for (int n = 1 ; n <= lastMod; n++) {
          if ((x > md[n].Xmodos) and (x < ((md[n].Xmodos) + (md[n].Xmodsr))) and (y > ((md[n].Ymodos) + (md[n].Ymodnr))) and (y < ((md[n].Ymodos) + (md[n].Ymodsr) + (md[n].Ymodnr)))) {
            Beep(1, 0);
            delay(400);
            Modebut = false;
            currentMode = n;
            BandSet();
            DrawFila();
          }
        }
      }

      if (BandWidth == true) {
        if ( currentMode == AM) {
          for (int n = 0 ; n < 7; n++) {
            if ((x > bw[n].Xos) and (x < ((bw[n].Xos) + (bw[n].Xsr))) and (y > ((bw[n].Yos) + (bw[n].Ynr))) and (y < ((bw[n].Yos) + (bw[n].Ysr) + (bw[n].Ynr)))) {
              Beep(1, 0);
              delay(400);
              bwIdxAM = bw[n].BandWidthAM;
              BandWidth = false;
              BandSet();
              //setBandWidth();
              DrawFila();

            }
          }
        }
        else {
          for (int n = 0 ; n < 6; n++) {
            if ((x > bw[n].Xos) and (x < ((bw[n].Xos) + (bw[n].Xsr))) and (y > ((bw[n].Yos) + (bw[n].Ynr))) and (y < ((bw[n].Yos) + (bw[n].Ysr) + (bw[n].Ynr)))) {
              Beep(1, 0);
              delay(400);
              bwIdxSSB = bw[n].BandWidthSSB;
              BandWidth = false;
              BandSet();
              DrawFila();
            }
          }
        }
      }

      if (STEPbut == true) {
        for (int n = 0 ; n < 4; n++) {
          if ((x > sp[n].Xstepos) and (x < ((sp[n].Xstepos) + (sp[n].Xstepsr))) and (y > ((sp[n].Ystepos) + (sp[n].Ystepnr))) and (y < ((sp[n].Ystepos) + (sp[n].Ystepsr) + (sp[n].Ystepnr)))) {
            Beep(1, 0);
            delay(400);
            STEPbut = false;
            currentStep = sp[n].stepFreq;
            BFORange = currentStep*1000;  ////
            currentBFO = 0; ////
            setStep();
            DrawFila();
          }
        }
      }

      if (BroadBand == true) {
        for (int n = 0 ; n <= lastBroad; n++) {
          if ((x > ((bb[n].Xbbandos) + (bb[n].Xbbandnr))) and (x < ((bb[n].Xbbandos) + (bb[n].Xbbandsr) + (bb[n].Xbbandnr))) and (y > ((bb[n].Ybbandos) + (bb[n].Ybbandnr))) and (y < ((bb[n].Ybbandos) + (bb[n].Ybbandsr) + (bb[n].Ybbandnr)))) {
            Beep(1, 0);
            delay(400);
            BroadBand = false;
            bandIdx = bb[n].BbandNum;
            if ((bandIdx == 0) and (currentAGCgain >=28)) currentAGCgain = previousAGCgain = 26; // currentAGCgain in FM max. 26
            si4735.setAM();
            delay(50);
            currentMode = band[bandIdx].prefmod;
            bwIdxAM =  1;  //// 3;  ////  default BW4kHz
            BandSet();
            DrawFila(); //Draw first layer
          }
        }
      }

      if (HamBand == true) {
        for (int n = 0 ; n <= lastHam; n++) {
          if ((x > ((bn[n].Xbandos) + (bn[n].Xbandnr))) and (x < ((bn[n].Xbandos) + (bn[n].Xbandsr) + (bn[n].Xbandnr))) and (y > ((bn[n].Ybandos) + (bn[n].Ybandnr))) and (y < ((bn[n].Ybandos) + (bn[n].Ybandsr) + (bn[n].Ybandnr)))) {
            Beep(1, 0);
            delay(400);
            HamBand = false;
            bandIdx = bn[n].BandNum;
            if (ssbLoaded == false) {
              si4735.setAM();
              delay(50);
            }
            currentMode = band[bandIdx].prefmod;
            bwIdxSSB = 1;
            BandSet();
            DrawFila();
          }
        }
      }

      if (PRESbut == true) {
        delay(200);
        if (currentMode != 0) { // geen fm ?
          bandIdx = 0;
          currentMode = 0;
          bfoOn = false;
          drawBFO();
          previousPRES = -1;
        }
        FirstLayer  =  true;
        SecondLayer = false;
        previousPRES = -2;
      }

      if (VOLbut == true) {
        delay(200);
        currentVOL = si4735.getVolume();
        previousVOL = currentVOL;
        FirstLayer  =  true;
        SecondLayer = false;
        FreqDispl();
      }

      if (FREQbut == true) {
        spr.createSprite(But_Key_Width-6, But_Key_Height-5);
        spr.fillScreen(COLOR_BACKGROUND);
        for (int n = 0 ; n < 12; n++) { // which keys are pressed?
          if ((x > ((kp[n].Xkeypos) + (kp[n].Xkeypnr))) and (x < ((kp[n].Xkeypos) + (kp[n].Xkeypsr) + (kp[n].Xkeypnr))) and (y > ((kp[n].Ykeypos) + (kp[n].Ykeypnr))) and (y < ((kp[n].Ykeypos) + (kp[n].Ykeypsr) + (kp[n].Ykeypnr)))) {
            if ( n == 11) { // Send button is red
                spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Green);
            } else {
                spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Red);
            }
            spr.setTextColor(COLOR_BUTTON_TEXT);
            spr.setTextSize(2);
            spr.setTextDatum(BC_DATUM);
            spr.setTextPadding(0);
            spr.drawString((Keypathtext[kp[n].KeypNum]), (But_Key_Width/2)-2, (But_Key_Height/2)+9);
            spr.pushSprite((kp[n].Xkeypos + kp[n].Xkeypnr + 3), (kp[n].Ykeypos + kp[n].Ykeypnr  + 3));

            Beep(1, 0);
            delay(100);

            if ( n == 11) { // Send button is red
                spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Red);
            } else {
                spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Green);
            }
            spr.setTextColor(COLOR_BUTTON_TEXT);
            spr.setTextSize(2);
            spr.setTextDatum(BC_DATUM);
            spr.setTextPadding(0);
            spr.drawString((Keypathtext[kp[n].KeypNum]), (But_Key_Width/2)-2, (But_Key_Height/2)+9);
            spr.pushSprite((kp[n].Xkeypos + kp[n].Xkeypnr + 3), (kp[n].Ykeypos + kp[n].Ykeypnr  + 3));
            spr.deleteSprite();

            if ((n >= 0) and (n <= 8)) Freqcalq(n + 1);
            if (n == 10) Freqcalq(0);
            if (n == 9) {
              Decipoint = true;
              fact = 10;
            }
            if (n == 11) {// Send button
              FREQbut = false;
              Displayfreq = (Displayfreq / 10) + dpfrq;
              if(Displayfreq < 1) {
                tft.setCursor(7, 25);
                tft.print("Freqency < 1 has no function");
                ErrorBeep();
              }else{
                 if ((Displayfreq > 30) and (Displayfreq < 64 )) {   ////
                  tft.setCursor(7, 25);
                  tft.print("Freqency > 30MHz and < 64 MHz");   ////
                  ErrorBeep();
                }else{
                  if ((Displayfreq >= 108) and (Displayfreq < 153 )) {
                    tft.setCursor(7, 25);
                    tft.print("Freqency >= 108 and < 153 ");
                    ErrorBeep();
                  }else{
                    if (Displayfreq > 30000) Displayfreq = Displayfreq / 1000000;
                    if ((Displayfreq <= 30000) and (Displayfreq >= 153) and (Decipoint == false )) Displayfreq = Displayfreq / 1000;
                    if ((Displayfreq >= 64) and (Displayfreq <= 108)) {   ////
                      currentFrequency = Displayfreq * 100;
                      bandIdx = 0;
                      band[bandIdx].currentFreq = currentFrequency;
                    }else{
                      currentFrequency = Displayfreq * 1000;
                      for (int q = 1 ; q <= lastBand; q++) {
                        if (((currentFrequency) >= band[q].minimumFreq) and ((currentFrequency) <= band[q].maximumFreq)) {
                          bandIdx = q;
                          currentMode = band[q].prefmod;
                          currentStep = band[bandIdx].currentStep;
                          BFORange = currentStep*1000;  ////
                          currentBFO = 0; ////
                          break;
                        }
                      }
                      delay(100);
                      band[bandIdx].currentFreq = currentFrequency;
                    }
                  }
                }
              }  
              BandSet();
              DrawFila();
            }//   End   n=11 Send button
          }
        }
      }//end freq
    }// end second layer

    if (ThirdLayer) { //==================================================
      //Check which button is pressed in Third Layer.
      for (int n = 0 ; n <= lastButton; n++) {
        if ((x > (Xbutst + (bt[bt[n].ButtonNum1].XButos))) and (x < Xbutst + (bt[bt[n].ButtonNum1].XButos) + Xbutsiz) and (y > Ybutst + (bt[bt[n].ButtonNum1].YButos)) and (y < Ybutst + (bt[bt[n].ButtonNum1].YButos) + Ybutsiz)) {
          Beep(1, 0);
          delay(400);
          x = 0;
          y = 0;

          if (n == SEEKUP) {
            delay(200);
            x = 0;
            y = 0;
            SEEK = true;
            if ((currentMode != LSB) and (currentMode != USB))   {
              if (currentMode != FM) {     // No FM
                if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) {
                  si4735.setSeekAmSpacing(band[bandIdx].currentStep);     //9 kHz
                  si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
                } 
                else {
                  bandIdx = 29;// all sw
                  si4735.setSeekAmSpacing(band[bandIdx].currentStep);     // 5 kHz
                  si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);  
                }
              }
              si4735.seekStationProgress(SeekFreq, checkStopSeeking,  SEEK_UP);// 1 is up ////
              delay(300);
              currentFrequency = si4735.getFrequency();
              band[bandIdx].currentFreq = currentFrequency ;
              if (currentFrequency != previousFrequency)
              {
                previousFrequency = currentFrequency;
                DrawDispl();
                delay(300); 
              }
            }  
            SEEK = false;
          }

         if (n == SEEKDN) {
           delay(200);
           x = 0;
           y = 0;
           SEEK = true;
           if ((currentMode != LSB) and (currentMode != USB))   {
             if (currentMode != FM) {     // No FM
               if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) {
                 si4735.setSeekAmSpacing(band[bandIdx].currentStep);     //9 kHz
                 si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
               } else {
                 bandIdx = 29;// all sw
                 si4735.setSeekAmSpacing(band[bandIdx].currentStep);     // 5 kHz
                 si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);  
               }
           }
        
          si4735.seekStationProgress(SeekFreq,checkStopSeeking,  SEEK_DOWN);  ////
          delay(300);
          currentFrequency = si4735.getFrequency();
          band[bandIdx].currentFreq = currentFrequency ;
          if (currentFrequency != previousFrequency)
          {
            previousFrequency = currentFrequency;
            DrawDispl();
            delay(300); 
          }
          }
          SEEK = false;
        }

          if (n == STATUS) {
            delay(200);
            x = 0;
            y = 0;
            subrstatus();
            DrawThla();
          }

          if (n == PREV) {
            delay(200);
            x = 0;
            y = 0;
            AGCgainbut = false;
            FirstLayer  = true;
            SecondLayer = false;
            ThirdLayer  = false;
            ForthLayer  = false;
            DrawFila();
          }

          if (n == RDSbut) {
            delay(200);
            x = 0;
            y = 0;
            if (RDS) RDS = false;
            else RDS = true;
            //DrawRDSbut();
            //DrawThla();
            DrawButThla(); 
            DrawAGCgainbut();
          }

          if (n == AGCset) {
            delay(200);
            x = 0;
            y = 0;
             if (AGCgainbut) AGCgainbut = false;
             else {
               bfoOn = false; // only AGC function at the rotory encoder
               AGCgainbut = true;
               si4735.getAutomaticGainControl();
               previousAGCgain = 37; // force to setup AGC gain
             }  
             FreqDispl();
             DrawThla();  
          }  
        }
      }
    } // end ThirdLayer

    if (ForthLayer) { //===============================================================

    }
  }// end pressed

  if (currentMode == LSB || currentMode == USB) // set BFO value in si4735
  {
    if (currentBFO != previousBFO)
    {
      previousBFO = currentBFO;
      si4735.setSSBBfo(currentBFO);
      if (bfoOn) FreqDispl();
    }
  }

  if (currentPRES != previousPRES)
  {
    si4735.getCurrentReceivedSignalQuality();
    if (si4735.isCurrentTuneFM() == false) {
      bandIdx = 0;
      band[bandIdx].currentFreq = ((preset[currentPRES].presetIdx));
      BandSet();
    }
    tft.setCursor(0, 20);
    if (currentPRES > lastPreset) currentPRES = 0;
    if (currentPRES < 0) currentPRES = lastPreset;
    previousPRES = currentPRES;
    DrawDispl();
    tft.fillRect(XFreqDispl +6, YFreqDispl + 20 , 228, 34, COLOR_BACKGROUND); ////
    AGCfreqdisp();
    tft.setTextColor(COLOR_BUTTON_TEXT, COLOR_BACKGROUND); ////
    tft.setTextSize(2);
    tft.setTextDatum(BC_DATUM);

    tft.drawString(String(currentPRES) + ": " + String(((preset[currentPRES].presetIdx) / 100), 1), 60, 51);   ////
    tft.setTextColor(COLOR_PANEL_TEXT, COLOR_BACKGROUND); ////
    tft.drawString(String(preset[currentPRES].PresetName), 175, 51);
    bandIdx = 0;
    si4735.setFrequency((preset[currentPRES].presetIdx));
    band[bandIdx].currentFreq = si4735.getFrequency();
  }

  if (currentVOL != previousVOL)
  {
    currentVOL = currentVOL + (currentVOL - previousVOL);
    tft.setCursor(0, 20);
    if (currentVOL > MaxVOL) currentVOL = MaxVOL;
    if (currentVOL < MinVOL) currentVOL = MinVOL;
    previousVOL = currentVOL;
    si4735.setVolume(currentVOL);
    FreqDispl();
  }

  if (currentAGCgain != previousAGCgain)
  {
    AGCgain = 1;
    //currentAGCgain = currentAGCgain + (currentAGCgain - previousAGCgain);
    tft.setCursor(0, 20);
    if (si4735.isCurrentTuneFM())  MaxAGCgain = MaxAGCgainFM;
    else MaxAGCgain = MaxAGCgainAM;

    if (currentAGCgain > MaxAGCgain) currentAGCgain = MaxAGCgain;
    if (currentAGCgain < MinAGCgain) currentAGCgain = MinAGCgain;

    previousAGCgain = currentAGCgain;
    si4735.setAutomaticGainControl(1,currentAGCgain);
    DrawDispl();
    DrawAGCgainbut();
  }

  
  
//=======================================================================================
}// end loop
//=======================================================================================

//=======================================================================================
void Dispoff()  {
//=======================================================================================
  if (((millis() - DisplayOnTime) > MIN_ELAPSED_DISPL_TIME * 300) and (DISplay == true)) {
    DISplay = false;
    digitalWrite(Display_Led, displayoff);
////    Serial.println("Display off");
    PRESbut = false;
    
    DrawDispl();
    DisplayOnTime = millis();
  }
}

//=======================================================================================
void VOLbutoff()  {
//=======================================================================================
  if (((millis() - VOLbutOnTime) > MIN_ELAPSED_VOLbut_TIME * 30) and (VOLbut == true)) {
    VOLbut = false;
    drawVOL();
    FreqDispl();
  }
  if (VOLbut == false) VOLbutOnTime = millis();
}

//=======================================================================================
void DisplayRDS()  {
//=======================================================================================
  if (( currentMode == FM) and((FirstLayer) or (ThirdLayer))){
      if ( currentFrequency != previousFrequency ) {
        previousFrequency = currentFrequency;
        //bufferStatioName[0] = '\0';
        //stationName = '\0';
        tft.fillRect(XFreqDispl + 60, YFreqDispl + 54, 140, 20, COLOR_BACKGROUND); //// // clear RDS text
      }
      if ((RDS) and  (NewSNR >= 12)) checkRDS(); 
      else  tft.fillRect(XFreqDispl + 60, YFreqDispl + 54, 140, 20, COLOR_BACKGROUND); //// // clear RDS text
  }
}

//=======================================================================================
void showtimeRSSI() {
//=======================================================================================
  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 3) // 150 * 10  = 1.5 sec refresh time RSSI
  {
    uint8_t disp_flag = 0;  ////
    si4735.getCurrentReceivedSignalQuality();
    NewRSSI = si4735.getCurrentRSSI();
    NewSNR = si4735.getCurrentSNR();
    NewMULT = si4735.getCurrentMultipath();  ////
    NewBLEND = si4735.getCurrentStereoBlend(); ////
    if(NewMULT > 100) NewMULT = 100;  ////
    if(NewBLEND > 100) NewBLEND = 100; ////
////
   if (OldRSSI != NewRSSI)
    {
      OldRSSI = NewRSSI;
       disp_flag = 1;
    }
    else if (OldSNR != NewSNR)
    {
      OldSNR = NewSNR;
      disp_flag = 1;
    }
    else if (OldMULT != NewMULT)
    {
      OldMULT = NewMULT;
      disp_flag = 1;
    }
    else if (OldBLEND != NewBLEND)
    {
      OldBLEND = NewBLEND;
      disp_flag = 1;
    }
   
   if (disp_flag == 1) {
      showRSSI();
    }
////
    elapsedRSSI = millis();
  }
}

//=======================================================================================
void showRSSI() {
//=======================================================================================
 uint8_t stat;  ////
  if ((  currentMode == FM ) and ((FirstLayer) or (ThirdLayer))) {
    sprintf(buffer, "%s", (stat = si4735.getCurrentPilot()) ? "STEREO" : "MONO"); ////
    tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);
    tft.setTextPadding(0);
    tft.fillRect(XFreqDispl + 180, YFreqDispl + 10 , 50, 12, COLOR_BACKGROUND); //// // STEREO MONO
    //tft.drawString(buffer, XFreqDispl + 50, YFreqDispl + 20);
    if(stat == 1)  tft.setTextColor(COLOR_INDICATOR_TXT3, COLOR_BACKGROUND);  //// STEREO Orange
    else tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND);  ////
    tft.drawString(buffer, XFreqDispl + 200, YFreqDispl + 20);
  }
  rssi = NewRSSI;
  snr  = map(NewSNR , 0, 50, 0, 212);  ////
  mult = map(NewMULT , 0, 100, 0, 212);  ///
  blend = map(NewBLEND , 0, 100, 0, 212);  ////

  if ((FirstLayer) or (ThirdLayer)) Smeter();
  tft.setTextSize(1);
  tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
  if ((FirstLayer) or (ThirdLayer)) {  // dBuV and volume at freq. display
    tft.fillRect(XFreqDispl + 7, YFreqDispl + 75 , 173, 10, COLOR_BACKGROUND); ////
    tft.setTextDatum(TL_DATUM);
    tft.drawString("RSSI = "+ String(NewRSSI) + " dBuV" , XFreqDispl + 8, YFreqDispl + 75);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("SNR = " + String(NewSNR) + " dB", XFreqDispl + 180, YFreqDispl + 75);

    tft.setTextDatum(TL_DATUM); ////
    tft.drawString("Ant. Cap = " + String(si4735.getAntennaTuningCapacitor()) ,  XFreqDispl + 8, YFreqDispl + 85); ////
  }
  VOLbutoff();
}

//=======================================================================================
void encoderCheck()  {
//=======================================================================================
  if (encoderCount != 0)
  {
    if (DISplay == false) { //  Wake-up  Display
      DisplayOnTime = millis();
      digitalWrite(Display_Led, displayon);
      DISplay = true;
    }

    int mainpurp = 1;
////
    if (bfoOn)  {
      currentBFO = (encoderCount == 1) ? (currentBFO - currentBFOStep) : (currentBFO + currentBFOStep);
      currentBFO = currentBFO-(currentBFO%currentBFOStep);
      si4735.setFrequencyStep(currentStep);
      BFORange = currentStep*1000;
      if(currentBFO>=BFORange) {    //// Freq Up
         currentBFO = currentBFO - BFORange;
         si4735.frequencyDown();
      }
      if(currentBFO<=-BFORange) {    //// Freq Down
         currentBFO = BFORange + currentBFO;
         si4735.frequencyUp();
      }
      FreqDispl();
      band[bandIdx].currentFreq = si4735.getFrequency();
////      si4735.setFrequencyStep(currentStep);
      mainpurp = 0;
    }
////

    if (PRESbut) {     // FM preset
      currentPRES = (encoderCount == 1) ? (currentPRES + currentPRESStep) : (currentPRES - currentPRESStep);
      mainpurp = 0;
    }

    if (VOLbut) {     // Volume control
      currentVOL = (encoderCount == 1) ? (currentVOL + currentVOLStep) : (currentVOL - currentVOLStep);
      mainpurp = 0;
    }

    if (AGCgainbut) {     // AGC gain control
      currentAGCgain = (encoderCount == 1) ? (currentAGCgain + currentAGCgainStep) : (currentAGCgain - currentAGCgainStep);
      mainpurp = 0;
    }
    
    
    if (mainpurp == 1)
    {
      if (encoderCount == 1) {
        si4735.frequencyUp();
      } else {
        si4735.frequencyDown();
      }
      FreqDispl();
      band[bandIdx].currentFreq = si4735.getFrequency();
    }
////    if ( !FirstLayer )  DrawFila(); 
    encoderCount = 0;
  }
}

//=======================================================================================
void encoderButtonCheck()  {
//=======================================================================================
  //Encoder button
  encBut = analogRead(ENCODER_SWITCH);
  if (encBut < 500) {
    Beep(1, 0);
    delay(400);
    if (DISplay == false) { //  Wake-up  Display
      DisplayOnTime = millis();
      digitalWrite(Display_Led, displayon);
      DISplay = true;
      return;
    }
    if (PRESbut == true) {// FM preset selection
      PRESbut = false;
      DrawDispl();
      return;
    }
    else {
      if (ssbLoaded) {  // SSB is on
         currentBFOStep = (currentBFOStep == 100) ? 10 : 100;  ////
////        if (bfoOn) {
////          bfoOn = false;
////        }
////        else {
////          bfoOn = true;
////        }
        //if (currentMode == FM) bfoOn = false;
        drawBFO();
        DrawDispl();
      }
    }
  }
}

//=======================================================================================
void setStep()  {
//=======================================================================================
  // This command should work only for SSB mode
  if (bfoOn && (currentMode == LSB || currentMode == USB))
  {
    currentBFOStep = (currentBFOStep == 100) ? 10 : 100;  ////
  }
  else
  {
    si4735.setFrequencyStep(currentStep);
    band[bandIdx].currentStep = currentStep;
    BFORange = currentStep*1000;  ////
    currentBFO = 0; ////
  }
  DrawDispl();
}

//=======================================================================================
void Beep(int cnt, int tlb) {
//=======================================================================================
  int tla = 100;
  for (int i = 0; i < cnt; i++) {
    digitalWrite(BEEPER, beepOn);
    delay(tla);
    digitalWrite(BEEPER, beepOff);
    delay(tlb);
  }
}

//=======================================================================================
void DrawFila()   {  // Draw of first layer
//=======================================================================================
  FirstLayer = true;
  SecondLayer = false;
  tft.fillScreen(COLOR_BACKGROUND); ////
  DrawButFila();
  DrawDispl();
  DrawSmeter();
  DrawVolumeIndicator();
}

//=======================================================================================
void DrawThla()  {  // Draw of Third layer
//=======================================================================================
  ThirdLayer = true;
  ForthLayer = false;
  tft.fillScreen(COLOR_BACKGROUND); ////
  DrawButThla();
  DrawDispl();
  DrawSmeter();
  DrawVolumeIndicator();
  DrawRDSbut();
  DrawAGCgainbut();
}

//=======================================================================================
void DrawButFila() { // Buttons first layer
//=======================================================================================
  //tft.fillScreen(COLOR_BACKGROUND); ////
   spr.createSprite(Xbutsiz,Ybutsiz);
  for (int n = 0 ; n <= lastButton; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
     spr.setTextColor(COLOR_BUTTON_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString((bt[n].ButtonNam),(Xbutsiz/2)+2, (Ybutsiz/2)+9);
     spr.pushSprite(bt[bt[n].ButtonNum].XButos + Xbutst, bt[bt[n].ButtonNum].YButos + Ybutst);
  }
     spr.deleteSprite();
  drawBFO();
  drawAGC();
}

//=======================================================================================
void DrawButThla() { // Buttons Third layer
//=======================================================================================
  //tft.fillScreen(COLOR_BACKGROUND); ////
  spr.createSprite(Xbutsiz,Ybutsiz);
  for (int n = 0 ; n <= lastButton; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
     spr.setTextColor(COLOR_BUTTON_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString((bt[n].ButtonNam1),(Xbutsiz/2)+2, (Ybutsiz/2)+9);
     spr.pushSprite(bt[bt[n].ButtonNum1].XButos + Xbutst, bt[bt[n].ButtonNum1].YButos + Ybutst);
  }
     spr.deleteSprite();
  DrawRDSbut();
}

//=======================================================================================
void DrawVolumeIndicator()  {
//=======================================================================================
/*
  tft.setTextSize(1);
  tft.fillRect(XVolInd, YVolInd, 240, 30, COLOR_FRAME); ////
  tft.fillRect(XVolInd + 5, YVolInd + 5, 230, 20, COLOR_BACKGROUND); ////
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND); ////
  tft.setCursor(XVolInd +  11, YVolInd + 7);
  tft.print("0%");
  tft.setCursor(XVolInd + 116, YVolInd + 7);
  tft.print("50%");
  tft.setCursor(XVolInd + 210, YVolInd + 7);
  tft.print("100%");
*/
}

//=======================================================================================
void DrawSmeter()  {
//=======================================================================================
  String IStr;
  tft.setTextSize(1);
  tft.fillRect(Xsmtr, Ysmtr, 240, 80, COLOR_FRAME); ////
  tft.fillRect(Xsmtr + 5, Ysmtr + 5, 230, 75, COLOR_BACKGROUND); ////
////  tft.fillRect(Xsmtr, Ysmtr, 240, 55, COLOR_FRAME); ////
////  tft.fillRect(Xsmtr + 5, Ysmtr + 5, 230, 45, COLOR_BACKGROUND); ////
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND); ////
  tft.setTextDatum(BC_DATUM);
  for (int i = 0; i < 10; i++) {
    tft.fillRect(Xsmtr + 15 + (i * 12), Ysmtr + 24, 2, 4, TFT_YELLOW);  //// 4, 8, TFT_YELLOW);
    IStr = String(i);
    tft.setCursor((Xsmtr + 14 + (i * 12)), Ysmtr + 13);
    tft.print(i);
  }
  for (int i = 1; i < 7; i++) {
    tft.fillRect((Xsmtr + 123 + (i * 16)), Ysmtr + 24, 2, 4, TFT_RED);  //// 4, 8, TFT_RED);
    IStr = String(i * 10);
    tft.setCursor((Xsmtr + 117 + (i * 16)), Ysmtr + 13);
    if ((i == 2) or (i == 4) or (i == 6))  {
      tft.print("+");
      tft.print(i * 10);
    }  
  }
  tft.fillRect(Xsmtr + 15, Ysmtr + 28 , 112, 2, TFT_YELLOW); ////  4, TFT_YELLOW);
  tft.fillRect(Xsmtr + 127, Ysmtr + 28 , 100, 2, TFT_RED);   ////  4, TFT_RED);
  // end Smeter

    tft.setCursor(Xsmtr + 8, Ysmtr + 30);  ////
    tft.print("R");  ////
    tft.setCursor(Xsmtr + 8, Ysmtr + 40);  ////
    tft.print("S");  ////
    tft.setCursor(Xsmtr + 8, Ysmtr + 60);  ////
    tft.print("M");  ////
    tft.setCursor(Xsmtr + 8, Ysmtr + 70);  ////
    tft.print("B");  ////

  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND); ////
  tft.setCursor(XVolInd +  11, Ysmtr + 49); ////
  tft.print("0%");
  tft.setCursor(XVolInd + 116, Ysmtr + 49); ////
  tft.print("50%");
  tft.setCursor(XVolInd + 210, Ysmtr + 49); ////
  tft.print("100%");

}

//=======================================================================================
void drawVOL()   {
//=======================================================================================
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (VOLbut) {
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Amber);
  } else {
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString((bt[5].ButtonNam), (Xbutsiz/2)+2, (Ybutsiz/2)+9);
  spr.pushSprite(bt[bt[5].ButtonNum].XButos + Xbutst, bt[bt[5].ButtonNum].YButos + Ybutst);
  spr.deleteSprite();
}

//=======================================================================================
void DrawAGCgainbut()  {
//=======================================================================================
 if (ThirdLayer)  {  
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (AGCgainbut) {
    AGCgainbuttext = currentAGCgain;
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Red);
  }else { 
    AGCgainbuttext = currentAGCgain;
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString("AGCatt", (Xbutsiz/2)+2, (Ybutsiz/2)+1);
  spr.drawString(AGCgainbuttext, (Xbutsiz/2)+2, (Ybutsiz/2)+16);
  spr.pushSprite(bt[bt[4].ButtonNum1].XButos + Xbutst, bt[bt[4].ButtonNum1].YButos + Ybutst);
  spr.deleteSprite();
 }
}

//=======================================================================================
void DrawRDSbut()  {
//=======================================================================================
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (RDS) {
    RDSbuttext = "ON";
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Red);
  } else {
    RDSbuttext = "OFF";
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString((bt[3].ButtonNam1), (Xbutsiz/2)+2, (Ybutsiz/2)+9);
  spr.pushSprite(bt[bt[3].ButtonNum1].XButos + Xbutst, bt[bt[3].ButtonNum1].YButos + Ybutst);
  spr.deleteSprite();
}

//=======================================================================================
void drawMUTE()  {
//=======================================================================================
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (Mutestat) {
    si4735.setAudioMute(audioMuteOn);
    spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Amber);
  } else {
    si4735.setAudioMute(audioMuteOff);
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString((bt[4].ButtonNam), (Xbutsiz/2)+2, (Ybutsiz/2)+9);
  spr.pushSprite(bt[bt[4].ButtonNum].XButos + Xbutst, bt[bt[4].ButtonNum].YButos + Ybutst);
  spr.deleteSprite();
}

//=======================================================================================
void drawAGC()  {
//=======================================================================================
  si4735.getAutomaticGainControl();
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (si4735.isAgcEnabled()) {
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Red);
  } else {
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString((bt[3].ButtonNam), (Xbutsiz/2)+2, (Ybutsiz/2)+9);
  spr.pushSprite(bt[bt[3].ButtonNum].XButos + Xbutst, bt[bt[3].ButtonNum].YButos + Ybutst);
  spr.deleteSprite();
}

//=======================================================================================
void drawBFO ()  {
//=======================================================================================
  spr.createSprite(Xbutsiz,Ybutsiz);
  spr.fillScreen(COLOR_BACKGROUND);
  if (bfoOn) {
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Red);
  } else {
     spr.pushImage(0, 0, But_Width, But_Height, (uint16_t *)But_Blue);
  }
  spr.setTextColor(COLOR_BUTTON_TEXT);
  spr.setTextSize(2);
  spr.setTextDatum(BC_DATUM);
  spr.setTextPadding(0);
  spr.drawString((bt[1].ButtonNam), (Xbutsiz/2)+2, (Ybutsiz/2)+9);
  spr.pushSprite(bt[bt[1].ButtonNum].XButos + Xbutst, bt[bt[1].ButtonNum].YButos + Ybutst);
  spr.deleteSprite();
}

//=======================================================================================
void drawKeyPath() {
//=======================================================================================
  tft.fillScreen(TFT_BLACK);
  spr.createSprite(But_Key_Width-6, But_Key_Height-5);
  spr.fillScreen(COLOR_BACKGROUND);

  for (int n = 0 ; n <= lastKPath; n++) {
    if ( n == 11) { // Send button is red
      spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Red);
    } else {
      spr.pushImage(0, 0, But_Key_Width, But_Key_Height, (uint16_t *)But_Key_Green);
    }

    spr.setTextColor(COLOR_BUTTON_TEXT);
    spr.setTextSize(2);
    spr.setTextDatum(BC_DATUM);
    spr.setTextPadding(0);
    spr.drawString((Keypathtext[kp[n].KeypNum]), (But_Key_Width/2)-2, (But_Key_Height/2)+9);
    spr.pushSprite((kp[n].Xkeypos + kp[n].Xkeypnr + 3), (kp[n].Ykeypos + kp[n].Ykeypnr  + 3));
  }
  spr.deleteSprite();
}

//=======================================================================================
void HamBandlist() {
//=======================================================================================
  tft.fillScreen(COLOR_BACKGROUND);  ////
  spr.createSprite(But_Mode_Width, But_Mode_Height);
  for (int n = 0 ; n <= lastHam; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Mode_Width, But_Mode_Height, (uint16_t *)But_Mode_Green);
     spr.setTextColor(COLOR_PANEL_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString(band[bn[n].BandNum].bandName, But_Mode_Width/2, But_Mode_Height-2);
     spr.pushSprite((bn[n].Xbandos + bn[n].Xbandnr + 3) , (bn[n].Ybandos) + (bn[n].Ybandnr + 3));
  }
     spr.deleteSprite();
}

//=======================================================================================
void BroadBandlist() {
//=======================================================================================  
  tft.fillScreen(COLOR_BACKGROUND); ////
  spr.createSprite(But_Band_Width, But_Band_Height);
  for (int n = 0 ; n <= lastBroad; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     if (n==0)  spr.pushImage(0, 0, But_Band_Width, But_Band_Height, (uint16_t *)But_Band_Red);
     else if (n==1)  spr.pushImage(0, 0, But_Band_Width, But_Band_Height, (uint16_t *)But_Band_Amber);
     else if (n==2)  spr.pushImage(0, 0, But_Band_Width, But_Band_Height, (uint16_t *)But_Band_Blue);
     else  spr.pushImage(0, 0, But_Band_Width, But_Band_Height, (uint16_t *)But_Band_Green);
     spr.setTextColor(COLOR_PANEL_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString(band[bb[n].BbandNum].bandName, But_Band_Width/2, But_Band_Height-2);
     spr.pushSprite((bb[n].Xbbandos + bb[n].Xbbandnr + 3) , (bb[n].Ybbandos) + (bb[n].Ybbandnr + 3));
  }
     spr.deleteSprite();
}

//=======================================================================================
void Steplist() {
//=======================================================================================
  tft.fillScreen(COLOR_BACKGROUND);  ////
  spr.createSprite(But_Mode_Width, But_Mode_Height);
  for (int n = 0 ; n <= lastStep; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Mode_Width, But_Mode_Height, (uint16_t *)But_Mode_Green);
     spr.setTextColor(COLOR_PANEL_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString(String(sp[n].stepFreq) + "kHz", But_Mode_Width/2, But_Mode_Height-2);
     spr.pushSprite((sp[n].Xstepos) + 5, sp[n].Ystepos + 5 + sp[n].Ystepnr);

  }
     spr.deleteSprite();
}

//=======================================================================================
void Modelist() {
//=======================================================================================
  tft.fillScreen(COLOR_BACKGROUND);  ////
  spr.createSprite(But_Mode_Width, But_Mode_Height);
  for (int n = 1 ; n <= lastMod; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Mode_Width, But_Mode_Height, (uint16_t *)But_Mode_Green);
     spr.setTextColor(COLOR_PANEL_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     spr.drawString(bandModeDesc[md[n].Modenum], But_Mode_Width/2, But_Mode_Height-2);
     spr.pushSprite((md[n].Xmodos + 5), md[n].Ymodos + 5 + (md[n].Ymodnr));
  }
     spr.deleteSprite();
}

//=======================================================================================
void BWList()  {
//=======================================================================================
  tft.fillScreen(COLOR_BACKGROUND);  ////
  if ( currentMode == AM) nrbox = 7;
  else nrbox = 6;
  spr.createSprite(But_Mode_Width, But_Mode_Height);
  for (int n = 0 ; n < nrbox; n++) {
     spr.fillScreen(COLOR_BACKGROUND);
     spr.pushImage(0, 0, But_Mode_Width, But_Mode_Height, (uint16_t *)But_Mode_Green);
     spr.setTextColor(COLOR_PANEL_TEXT);
     spr.setTextSize(2);
     spr.setTextDatum(BC_DATUM);
     spr.setTextPadding(0);
     if ( currentMode == AM) spr.drawString(bandwidthAM[bw[n].BandWidthAM], But_Mode_Width/2, But_Mode_Height-2);
     else spr.drawString(bandwidthSSB[bw[n].BandWidthSSB], But_Mode_Width/2, But_Mode_Height-2);
     spr.pushSprite((bw[n].Xos + 3), bw[n].Yos + 3 + (bw[n].Ynr));
  }
     spr.deleteSprite();

  tft.setTextColor(COLOR_BUTTON_TEXT, COLOR_BACKGROUND); ////
  if ( currentMode == AM)  tft.drawString("AM Filter in kHz"  , XfBW + 50, YfBW - 30);  ////
  if ( currentMode == USB) tft.drawString("USB Filter in kHz" , XfBW + 50, YfBW - 30);  ////
  if ( currentMode == LSB) tft.drawString("LSB Filter in kHz" , XfBW + 50, YfBW - 30);  ////
}

//=======================================================================================
void subrstatus() {
//=======================================================================================
  tft.fillScreen(COLOR_BACKGROUND);  ////
  while (x == 0) {
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.setTextColor(COLOR_BUTTON_TEXT, COLOR_BACKGROUND); ////
    tft.drawString("Mod.     : " + String(bandModeDesc[band[bandIdx].prefmod]), 5, 10);
    if ( currentMode != FM)  tft.drawString("Freq.    : " + String(currentFrequency, 0) + " kHz", 5, 20);
    else tft.drawString("Freq.    : " + String(currentFrequency / 1, 1) + " MHz", 5, 20); ////
    si4735.getCurrentReceivedSignalQuality();
    tft.drawString("RSSI     : " + String(si4735.getCurrentRSSI()) + "dBuV  ", 5, 30); // si4735.getCurrentSNR()
    tft.drawString("SNR      : " + String(si4735.getCurrentSNR()) + "dB  ", 5, 40);
    if (  currentMode == FM ) {
////
//    tft.drawString("Multipath: " + String(si4735.getCurrentMultipath()) + "%", 5, 97); ////
      sprintf(buffer, "%s", (si4735.getCurrentPilot()) ? "STEREO" : "MONO");
      tft.drawString("         : " + String(buffer), 5, 50);
    }
    si4735.getAutomaticGainControl();
    si4735.getCurrentReceivedSignalQuality();
    tft.drawString("LNA GAIN index: " + String(si4735.getAgcGainIndex()) + "/" + String(currentAGCAtt), 5, 60);
    tft.drawString("Volume   : )" + String(si4735.getVolume()), 5, 70);
    sprintf(buffer, "%s", (si4735.isAgcEnabled()) ? "AGC ON " : "AGC OFF");
    tft.drawString(buffer, 5, 80);
    if (bfoOn) tft.drawString("BFO ON  ", 5, 90);
    else tft.drawString("BFO OFF ", 5, 90);
    tft.drawString("AVC max GAIN  : " + String(si4735.getCurrentAvcAmMaxGain()), 5, 100);
    tft.drawString("Ant. Cap = " + String(si4735.getAntennaTuningCapacitor()) , 5, 110);

    tft.setTextColor(COLOR_PANEL_TEXT, COLOR_BACKGROUND); ////
    tft.drawString("BandIdx  : " + String(bandIdx) + "  " + String(band[bandIdx].bandName) , 5, 140);
    tft.drawString("BwIdxSSB : " + String(bandwidthSSB[bwIdxSSB]) + " kHz", 5, 150);
    tft.drawString("BwIdxAM  : " + String(bandwidthAM[bwIdxAM]) + " kHz", 5, 160);
    tft.drawString("Stepsize : " + String(currentStep), 5, 170);
    uint16_t vsupply = analogRead(ENCODER_SWITCH);
    tft.drawString("Power Supply : " + String(((1.66 / 1850)*vsupply) * 2) + " V.", 5, 180);
////    tft.drawString("Power Supply : " + String(vsupply*0.0017582) + " V", 5, 310); //// (3.6*((15k+15k)/15k)))/4096

    tft.setCursor(140, 10);
    tft.println("Version 3.4.0-JimCom");
    tft.setCursor(140, 20);
    tft.println("Jul-25-2021");
    tft.setCursor(140,30);
    tft.print("Si473X addr :  ");
    tft.println(si4735Addr, HEX);

    tft.setCursor(140, 50);
    tft.println("Firmware Information.");
    tft.setCursor(140, 60);
    tft.print("Part Number (HEX)........: ");
    tft.println(si4735.getFirmwarePN(), HEX);
    tft.setCursor(140, 70);
    tft.print("Firmware Major Revision..: ");
    tft.println(si4735.getFirmwareFWMAJOR());
    tft.setCursor(140, 80);
    tft.print("Firmware Minor Revision..: ");
    tft.println(si4735.getFirmwareFWMINOR());
    tft.setCursor(140, 90);
    tft.print("Patch ID ................: ");
    tft.print(si4735.getFirmwarePATCHH(), HEX);
    tft.println(si4735.getFirmwarePATCHL(), HEX);
    tft.setCursor(140, 100);
    tft.print("Component Major Revision.: ");
    tft.println(si4735.getFirmwareCMPMAJOR());
    tft.setCursor(140, 110);
    tft.print("Component Minor Revision.: ");
    tft.println(si4735.getFirmwareCMPMINOR());
    tft.setCursor(140, 120);
    tft.print("Chip Revision............: ");
    tft.println(si4735.getFirmwareCHIPREV());

    press1 = tft.getTouch(&x, &y);  
  }
  x=y=0;
  Beep(1, 0);
  delay(400);
}

//=======================================================================================
void showRDSStation() {
//=======================================================================================
  //if (strncmp(bufferStatioName, stationName,3) == 0 ) return;
  if ((FirstLayer) or (ThirdLayer)) {
     //tft.drawString(stationName, XFreqDispl + 120, YFreqDispl + 71);
     tft.setCursor(XFreqDispl + 75, YFreqDispl + 55);
     tft.print(stationName);
  }
  delay(250);
}

//=======================================================================================
void checkRDS() {
//=======================================================================================
  si4735.getRdsStatus();
  if (si4735.getRdsReceived()) {
    if (si4735.getRdsSync() && si4735.getRdsSyncFound() ) {
      stationName = si4735.getRdsText0A();
      tft.setTextSize(2);
      tft.setTextColor(COLOR_RDS_TEXT, COLOR_BACKGROUND); ////
      tft.setTextDatum(BC_DATUM);
      if ( stationName != NULL)   showRDSStation();
    }
  }
}

//=======================================================================================
String zeroPad(uint8_t numLength, uint16_t num) {
  String NUM = String(num);
  String ZERO = "0";

  if (num == 0) {
    for (uint8_t i = 2; i <= numLength; i++) {
      ZERO.concat(NUM);
      NUM = ZERO;
      ZERO = "0";
    }
  } else {
    for (uint8_t i = 1; i <= numLength ; i++) {
      if (0 < num && num < pow(10, i - 1)) {
        ZERO.concat(NUM);
        NUM = ZERO;
        ZERO = "0";
      }
    }
  }
  return (NUM);
}
//=======================================================================================
void FreqDispl() {
//=======================================================================================  
  if ((FirstLayer) or (ThirdLayer)) {
    currentFrequency = si4735.getFrequency(); 
////
    if(currentFrequency > SWFREQ) digitalWrite(BANDSW_SW, 1);
    else  digitalWrite(BANDSW_SW, 0);
    if(currentFrequency > MWFREQ) digitalWrite(BANDSW_MW, 1);
    else  digitalWrite(BANDSW_MW, 0);
////
    tft.fillRect( XFreqDispl + 6, YFreqDispl + 22 , 228, 45, COLOR_BACKGROUND); //// // Black freq. field
    AGCfreqdisp(); 
    BFOfreqdisp();
    tft.setTextSize(4);
    tft.setTextColor(COLOR_INDICATOR_FREQ, COLOR_BACKGROUND); ////
    tft.setTextDatum(BC_DATUM);
    //tft.setTextPadding(0);
    if ((VOLbut) or (AGCgainbut)){
      if (VOLbut) {
        tft.setTextSize(3);
        tft.drawString(String(map(currentVOL, 20, 63, 0, 100)), XFreqDispl + 60, YFreqDispl + 53);
        tft.setTextSize(2);
          tft.drawString( " % Volume", XFreqDispl + 160, YFreqDispl + 53);
      }
      if (AGCgainbut){
        tft.setTextSize(3);
        tft.drawString(String(currentAGCgain), XFreqDispl + 60, YFreqDispl + 53);
        tft.setTextSize(2);
        tft.drawString("Attn-index", XFreqDispl + 160, YFreqDispl + 53);
      }
     
    } else {
      if ((band[bandIdx].bandType == MW_BAND_TYPE) || (band[bandIdx].bandType == LW_BAND_TYPE)) {
       if (currentMode == AM) {
          Displayfreq =  currentFrequency;
          tft.setTextSize(1);
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
          tft.drawString(String(Displayfreq, 0), XFreqDispl + 120, YFreqDispl + 61);
          tft.setFreeFont(NULL);  
          tft.setTextSize(2);     
          tft.drawString("kHz", XFreqDispl + 215, YFreqDispl + 61);
       }
       if ((currentMode == LSB) || (currentMode == USB)) {
            if (currentBFO<=0) {
               Displayfreq =  (currentFrequency+(-currentBFO/1000));
               DisplayBFO = -(currentBFO%1000);                                     //0<->BFORange
            }
            else {
               Displayfreq =  ((currentFrequency-1)+(((BFORange - currentBFO)-BFORange)/1000));
               DisplayBFO =   (BFORange - currentBFO)%1000;
            }
          tft.fillRect( XFreqDispl + 6, YFreqDispl + 26 , 228, 45, TFT_BLACK); // Black freq. field
          tft.setTextSize(1);  ////
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
          tft.drawString(String(Displayfreq, 0), XFreqDispl + 120, YFreqDispl + 61);
          tft.setFreeFont(NULL);  ////
          tft.setTextSize(1);
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_20);  ////
          tft.drawString(zeroPad(3,DisplayBFO), XFreqDispl + 212, YFreqDispl + 61);  ////
          tft.setFreeFont(NULL);  ////
          tft.setTextSize(2);
          tft.drawString("kHz", XFreqDispl + 215, YFreqDispl + 82);
          tft.setTextColor(COLOR_BFO, COLOR_BACKGROUND); ////
          tft.setTextSize(1);  ////
          tft.drawString(String(currentBFO), XFreqDispl + 215, YFreqDispl + 34); ////
       }
      }
      else {
       if (currentMode == AM) {
          Displayfreq =  currentFrequency / 1000;
          tft.setTextSize(1);   ////
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
          tft.drawString(String(Displayfreq, 3), XFreqDispl + 116, YFreqDispl + 61);
          tft.setFreeFont(NULL);  ////
          tft.setTextSize(2);
          tft.drawString("MHz", XFreqDispl + 215, YFreqDispl + 61);
       }
       if ((currentMode == LSB) || (currentMode == USB)) {
            if (currentBFO<=0) {
               Displayfreq =  (currentFrequency+(-currentBFO/1000))/1000;
               DisplayBFO = -(currentBFO%1000);                                     //0<->BFORange
            }
            else {
               Displayfreq =  ((currentFrequency-1)+(((BFORange - currentBFO)-BFORange)/1000))/1000;
               DisplayBFO =   (BFORange - currentBFO)%1000;
            }
          tft.fillRect( XFreqDispl + 6, YFreqDispl + 26 , 228, 45, TFT_BLACK); // Black freq. field
          tft.setTextSize(1);  ////
          dtostrf(Displayfreq,6,3,buffer);
          sprintf(buffer1,/*"%s.",*/ buffer);
          sprintf(buffer, "%02d",freqDec/10); 
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
          tft.drawString(String(buffer1)/*+ String(buffer)*/, XFreqDispl + 116, YFreqDispl + 61);
          tft.setFreeFont(NULL);  ////
          tft.setTextSize(1);
          tft.setFreeFont(&DSEG7_Classic_Mini_Regular_20);  ////
          tft.drawString(zeroPad(3,DisplayBFO), XFreqDispl + 212, YFreqDispl + 61);  ////
          tft.setFreeFont(NULL);  ////
          tft.setTextSize(2);
          tft.drawString("kHz", XFreqDispl + 215, YFreqDispl + 82);
          tft.setTextColor(COLOR_BFO, COLOR_BACKGROUND); ////
          tft.setTextSize(1);  ////
          tft.drawString(String(currentBFO), XFreqDispl + 215, YFreqDispl + 34); ////
       }
      }

      if (band[bandIdx].bandType == FM_BAND_TYPE) {
        Displayfreq =  currentFrequency / 100;
        tft.setTextSize(1);  ////
        tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);////
        tft.drawString(String(Displayfreq, 1), XFreqDispl + 120, YFreqDispl + 55);
        tft.setFreeFont(NULL);  ////
        tft.setTextSize(2);
        tft.drawString("MHz", XFreqDispl + 215, YFreqDispl + 55);
      }
    }
  }
}

/**
 * Checks the stop seeking criterias.  
 * Returns true if the user press the touch or rotates the encoder. 
 */
bool checkStopSeeking() {
  // Checks the touch and encoder
  return (bool) encoderCount || tft.getTouch(&x, &y);   // returns true if the user rotates the encoder or touches on screen
} 


//=======================================================================================
void SeekFreq (uint16_t freq)  {
//=======================================================================================
  if ((FirstLayer)or(ThirdLayer))  {
    currentFrequency = freq;
    tft.setTextSize(4);
    tft.setTextColor(COLOR_INDICATOR_FREQ, COLOR_BACKGROUND); ////
    tft.setTextDatum(BC_DATUM);
    tft.setTextPadding(0);
    tft.fillRect( XFreqDispl + 6, YFreqDispl +28 , 228, 32, COLOR_BACKGROUND); //// // Black freq. field
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) {
        Displayfreq =  currentFrequency;
////        tft.setTextSize(4);
        tft.setTextSize(1); ////
        tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
        tft.drawString(String(Displayfreq,0), XFreqDispl +120,YFreqDispl +61);
        tft.setFreeFont(NULL);  ////
        tft.setTextSize(2);
        tft.drawString("kHz", XFreqDispl +215,YFreqDispl +61);
      }
    if (band[bandIdx].bandType == FM_BAND_TYPE){
      Displayfreq =  currentFrequency/100;
////      tft.setTextSize(4);
      tft.setTextSize(1); ////
      tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
      tft.drawString(String(Displayfreq,1), XFreqDispl +120,YFreqDispl +55);
      tft.setFreeFont(NULL);  ////
      tft.setTextSize(2);
      tft.drawString("MHz", XFreqDispl +215,YFreqDispl +55);
    } 
    if (band[bandIdx].bandType == SW_BAND_TYPE){
        Displayfreq =  currentFrequency/1000;
////        tft.setTextSize(4);
        tft.setTextSize(1); ////
        tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  ////
        tft.drawString(String(Displayfreq,3), XFreqDispl +116,YFreqDispl +61);
        tft.setFreeFont(NULL);  ////
        tft.setTextSize(2);
        tft.drawString("MHz", XFreqDispl +215,YFreqDispl +61);
      }
     }    
   }

//=======================================================================================
void DrawDispl() {
//=======================================================================================
  tft.fillRect(XFreqDispl, YFreqDispl, 240, 90, COLOR_FRAME); ////
  tft.fillRect(XFreqDispl + 5, YFreqDispl + 5, 230, 80, COLOR_BACKGROUND); ////
  tft.setTextSize(1);
  tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
  tft.setTextDatum(BC_DATUM);
  tft.drawString(band[bandIdx].bandName, XFreqDispl + 160, YFreqDispl + 20);
  FreqDispl();
 
  if (band[bandIdx].bandType != FM_BAND_TYPE) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND);  ////
    tft.drawString(bandModeDesc[currentMode], XFreqDispl + 80, YFreqDispl + 20);
    tft.setTextPadding(tft.textWidth("2.2kHz"));
    if (currentMode == AM) BWtext = bandwidthAM[bwIdxAM];
    else BWtext = bandwidthSSB[bwIdxSSB];
    tft.drawString(BWtext + "kHz", XFreqDispl + 120, YFreqDispl + 20);
    tft.drawString(String(band[bandIdx].currentStep) + "kHz", XFreqDispl + 200, YFreqDispl + 20);
  }
}

//=======================================================================================
void AGCfreqdisp() {
//=======================================================================================
  tft.setTextSize(1);
  tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
  tft.setTextDatum(BC_DATUM);
  tft.drawString("AGC", XFreqDispl + 50, YFreqDispl + 16);
  si4735.getAutomaticGainControl();
  if (si4735.isAgcEnabled()) {
    tft.setTextColor(COLOR_INDICATOR_TXT2, COLOR_BACKGROUND);  ////
    tft.drawString("ON", XFreqDispl + 50, YFreqDispl + 26);
    tft.setTextColor(COLOR_INDICATOR_TXT3, COLOR_BACKGROUND);  ////
  } else {
    if (AGCgain == 0)   {
      tft.drawString("OFF", XFreqDispl + 50, YFreqDispl + 26);
    } else {
      tft.drawString(String(currentAGCgain), XFreqDispl + 50, YFreqDispl + 26);  
    }
  }
} 

//=======================================================================================
void BFOfreqdisp() {
//=======================================================================================
if (band[bandIdx].bandType != FM_BAND_TYPE) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
    tft.setTextDatum(BC_DATUM);
    tft.setTextPadding(tft.textWidth("XXX"));
    tft.drawString("BFO", XFreqDispl + 20, YFreqDispl + 16);
    tft.setTextPadding(tft.textWidth("88"));

    if (bfoOn) {
      tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
      tft.drawString(String(currentBFOStep), XFreqDispl + 20, YFreqDispl + 26);
      tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
    } else {
      tft.setTextColor(COLOR_INDICATOR_TXT1, COLOR_BACKGROUND); ////
      tft.drawString("  ", XFreqDispl + 20, YFreqDispl + 26);
    }
  }
}   

//=======================================================================================
void ErrorBeep()  {
//=======================================================================================
 Beep(2, 5); ////100);
//// delay(2000);
}
