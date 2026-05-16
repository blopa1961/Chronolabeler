/*********************************************************
  Chrono Labeler (TSPL version)
  (c) 2026 Pablo Montoreano

  @file       ChronoLabelerNimBLE_TSPL.ino
  @brief      Date/Time Sticker Printer
  @author     Pablo Montoreano
  @copyright  2026 Pablo Montoreano
  @version    1.0 - 12/May/26

  Microprocessor: ESP32 Dev Module (I used ESP32 mini)
  compile with: Arduino IDE 2.3.8 partition -> No OTA (2MB APP/2MB SPIFFS)

  Arduino libraries: WiFi, WebServer, Time, Ticker, LittleFS, ESPmDNS

  3rd party libraries:
  * NimBLE-Arduino by h2zero (Ryan Powell)
  * Font rendering routines from Thermal_Printer_Library by bitbank2 (Larry Bank)
  * Fonts: Adafruit GFX library (without headers)

  Please note I'm argentinian, so some variables are in Spanish (or "Spanglish"); they are fully explained in the comments though.
  The Spanish version was born first. It's more complicated because of the use of HTML Entities for accents (which were easily removed for this version)
  Regretfully I can't make a conditionally compiled multilanguage version, the WEB pages are too progam dependant and the code would be unreadable
  Furthermore, #defines are NOT processed within R"=====( string definitions (WEB pages)
*********************************************************/

#define MYGMTOFFSET -25200    // -7x3600 USA west coast GMT (change as needed)
#define MYTIMEZONE "PST8PDT"  // Time Zone for USA west coast (change as needed)
#define FORCETP // scan for BLE devices starting with "TP" only
#define MYPASS "Blopa1961" // Access Point password
#define FWVERSION "1.0" // firmware version
#define NTP_MIN_VALID_EPOCH 1777689600 // 1st of May 2026 Epoch (to validate NTP)

/*
  G&G or TPL thermal printers, BLE and UUIDs

// Serial characteristic service (here is where we send the data)
static BLEUUID serviceUUID("0000FF10-0000-1000-8000-00805F9B34FB");  // first reported characteristic
static BLEUUID charUUID("FFF1");  // RX Characteristic (serial RX)

// SERVICE (unknown)
static BLEUUID serviceUUID("49535343-FE7D-4AE5-8FA9-9FAFD205E455"); // (2nd UUID reported by the printer)
static BLEUUID charUUID("49535343-8841-43f4-a8d4-ecbe74729bb3");    // (WRITE, WRITE NO RESPONSE)
static BLEUUID charUUID("49535343-1e4d-4bd9-ba61-23c647249616");    // (NOTIFY) config 0x2902

// SERVICE (unknown)
static BLEUUID serviceUUID("0000FF12-0000-1000-8000-00805F9B34FB"); // (3rd UUID reported by the printer)
static BLEUUID charUUID("FF13"); // (WRITE, WRITE NO RESPONSE)
static BLEUUID charUUID("FF14"); // (NOTIFY) config 0x2902
static BLEUUID charUUID("FF16"); // (NOTIFY, READ) config 0x2902
static BLEUUID charUUID("FF15"); // (READ, WRITE NO RESPONSE)

// Battery Service
static BLEUUID serviceUUID("180F");
static BLEUUID charUUID("2a19"); // battery level (NOTIFY, READ) config 0x2902

// Tx Power
static BLEUUID serviceUUID("1804");
static BLEUUID charUUID("2A07"); // (NOTIFY, READ) config 0x2902

// Device information
static BLEUUID serviceUUID("180A");
static BLEUUID charUUID("2A24"); // (READ) Model Number String
static BLEUUID charUUID("2A25"); // (READ) Serial Number String
static BLEUUID charUUID("2A28"); // (READ) Software Revision String
static BLEUUID charUUID("2A29"); // (READ) Manufacturer Name String
*/

enum WifiStates {
  WIFIOFF,  // WIFI state OFF
  WIFIAPM,  // WIFI state AP mode
  WIFION,   // WIFI state trying to connect to router
  WIFICON   // WIFI state connected to Router
};

enum prnStates {
  STATNOPAIR,   // not paired
  STATDISCON,   // disconnected
  STATOFFLINE,  // Offline
  STATINVALTIME,  // invalid time
  STATONLINE,   // Online
  STATPRINTING  // printing
};

// Global definitions
static const unsigned int GPIObtn= 23;    // THE BUTTON (with 10K pullup)
static const unsigned int GPIOblue= 26;   // LED WiFi (1K resistor)
static const unsigned int GPIOyellow= 18; // LED NTP  (1K resistor)
static const unsigned int GPIOgreen= 19;  // LED Printing (1K resistor)
static const unsigned int DEFAULTWIFI= WIFIAPM;  // this is the default Wifi state saved in LittleFS if the file does not exist
static const unsigned int LEDON= HIGH;      // LED state ON
static const unsigned int LEDOFF= LOW;      // LED state OFF
static const unsigned int LEDWIFION= LEDON; // if WiFi is connected the blue LED is ON/OFF
static const unsigned int LEDWIFIOFF= LEDOFF;
static const unsigned int LEDBLINKOFF= 2; // blue LED mode, blinking (off)
static const unsigned int LEDBLINKON= 3;  // blue LED mode, blinking (on)
static const unsigned int LEDFLASHOFF= 4; // blue LED mode, flashing (off)
static const unsigned int LEDFLASHON= 5;  // blue LED mode, flashing (on)
static const unsigned int MYCHANNEL= 6;   // AP mode WiFi channel
static const unsigned int MAXCONNCT= 2;   // max number of WiFi connections in AP mode
static const unsigned int BTNPRESSED= LOW;     // pressing the button sends the signal to GND
static const unsigned int BTNRELEASED= HIGH;
static const unsigned long DEBTIME= 15;        // debounce time in milliseconds
static const unsigned long HOLDTIME= 1500;     // button held n mS > function "hold" (WiFi mode change)
static const unsigned long BLINKINGTIME= 250;  // blink N mS
static const unsigned long BLUEBLINK= 500;     // blue LED on/off time in blinking mode
static const unsigned long FLASHTIMEON= 50;    // blue LED flash ON time (AP mode)
static const unsigned long FLASHTIMEOFF= 1500; // blue LED flash OFF time
static const unsigned long DEFAULTTIMEOUT= 0;  // connection timeout (infinite by default)
static const unsigned long EPOCHRETRY= 500;    // NTP retry delay
// printer parameters
static const byte BLACK= 0;    // font black color
static const byte WHITE= 0xff; // and white
static const unsigned int chunkSize= 240; // BLE packet size
static const unsigned int MAXWIDTHPIXELS= 480;  // 57mm printable area by 8 pixels per mm = 456, adjusted to 32bit for BMPs (width must be divisible by 4)
static const unsigned int MAXHEIGHTPIXELS= 480; // 50mm+10mm extra footer space by 8 pixels per mm (no multiple restrictions)
static const unsigned int MAXBUFFERSIZE= MAXWIDTHPIXELS / 8 * MAXHEIGHTPIXELS;  // maximum buffer size in bytes
static const unsigned int MAXPROFILES= 12;  // max number of profiles
static const unsigned long INITDELAY= 350;  // printer Init delay

// filenames
#define MYLOGO "/BlopaLogo.webp"      // my logo (please do not remove)
#define ICON32 "/Favicon32.png"       // PC Favicon
#define ICON192 "/Favicon192.png"     // Android Favicon (tested in Firefox and Chrome)
#define FILEPRNCFG "/PrnCfg.ini"      // printer configuration file
#define FILEDATETIME "/DateTime.ini"  // Day/Time/NTP configuration file
#define FILEWIFI "/WifiCfg.ini"       // WiFi config file
#define FILESSID "/SsId.ini"          // SSID name file

// includes
#include "ChonoLabeler.h" // WEB pages
#include "WifiConf.h"     // WiFi config WEB page (please do not copy)
#include "about.h"        // about WEB page
#include <time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Ticker.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>

// bluetooth variables
static const unsigned int BLETIMEOUT= 12000;  // printer connection timeout in mS
static NimBLEUUID serviceUUID("0000FF10-0000-1000-8000-00805F9B34FB");  // first characteristic reported by TP-220 and TP-110
static NimBLEUUID charUUID("FFF1"); // RX Characteristic
static NimBLERemoteCharacteristic* pCharacteristic;
static NimBLEClient* pClient;
static NimBLERemoteService* pService;
static NimBLEScan* pScan;
static const unsigned int SCANTIME= 5000;   // max wait for BLE device scan
static const unsigned int MAXDEVICES= 20;   // max number of BLE devices to scan/list
static volatile unsigned int numDevices;    // number of BLE devices found (might be filtered by FORCETP)
static const unsigned int minBleNameLength= 4;  // min number of chars for a valid BLE printer name ("TP-x")
static String foundDevices[MAXDEVICES];     // should be volatile but the compiler doesn't allow it
static NimBLEAddress foundMacs[MAXDEVICES]; // MACs belonging to the BLE devices found
static const NimBLEAddress nullAddr{};      // Nimble null  address (not paired devices)

struct tm timeinfo; // NTP variables

struct swiCfg {
  unsigned int initialWifi; // WiFi initial mode
  unsigned long retryWifi;  // WiFi number of retries (0= infinite)
  byte staticIP[4];   // fixed IP (o 0.0.0.0 is DHCP)
  byte myGateway[4];  // router gateway
  byte subnet[4];     // subnet mask
};

struct prnCfg {
  unsigned int WIDTHmm;   // max sticker width in mm (all printers are metric, live with it!)
  unsigned int HEIGHTmm;  // max sticker height in mm
  unsigned int extraFooter;   // extra footer
  unsigned int topMargin[2];  // top margin for lines 1 and 2
  unsigned int leftMargin;    // left margin
  unsigned int chunkDelay;    // inter BLE packet delay
  unsigned int resetDelay;    // reconnection delay
  unsigned int fontNumber[2]; // selected font (you can use a different font for each line)
  uint8_t address[6]; // MAC address for the printer in use
  char prnBtName[30]; // printer BLE name
  char prnGap[10];    // inter sticker gap codes
  char prnLabel[26];  // this printer's profile name
  char prnInit[11];   // printer init string
  bool landscape;     // printer type (TP-110 landscape, TP-220 vertical)
  bool blackBackground; // is the background black?
  bool centered;      // centrered (ignore left margin)
};

struct prnCalc {  // calculated variables (not saved in config file)
  int32_t bitmapWidth;           // (WIDHmm+ExtraFooter)*8
  int32_t bitmapHeight;          // HEIGHTmm*8
  unsigned int myBufferSize;     // buffer size in use (bitmapWidth*bitmapHeight/8)
  unsigned int prnBitmapWidth;   // landscape ? WIDTHmm : HEIGHTmm
  unsigned int prnBitmapHeight;  // landscape ? bitmapHeight : bitmapWidth
  String prnBitHeader;  // Printer bitmap header
  String sPrnInit;      // printer init string (esc !S)
  String prnHeader;     // print command header (SIZE 48...)
  String prnFooter;     // footer command (PRINT 1,1...)
};

struct dateTimeCfg {
  byte dtFormats[8];    // date and time formats (see table in dtFormats below)
  long gmtOffset;       // UTC in milliseconds
  long daylight;        // daylight saving time, to add 1 hour change to 3600
  char ntpServer[31];   // NTP server name
  char timeZone[21];    // time Zone
};

// BMP header definition
static const unsigned int BMPHEADERSIZE= 62;
#pragma pack(push, 1)         // ensure packed byte alignment (not default 32 bit alignment)
struct BMPHeader {
    uint16_t bfType;          // Magic identifier: 'BM' (0x4D42)
    uint32_t bfSize;          // File size in  bytes
    uint16_t bfReserved1;     // Reserved, must be 0
    uint16_t bfReserved2;     // Reserved, must be 0
    uint32_t bfOffBits;       // Offset to pixel data
    uint32_t biSize;          // This header size (40 bytes)
    int32_t  biWidth;         // Image width in pixels
    int32_t  biHeight;        // Image height in pixels (positive = bottom to top)
    uint16_t biPlanes;        // must be 1
    uint16_t biBitCount;      // 1 for 1bpp  (2^1)
    uint32_t biCompression;   // 0 = BI_RGB (uncompessed)
    uint32_t biSizeImage;     // Pixel data size (may be 0 if not compressed)
    int32_t  biXPelsPerMeter; // X resolution (BPP)
    int32_t  biYPelsPerMeter; // Y resolution (BPP)
    uint32_t biClrUsed;       // 0 = 2^biBitCount (for 1bpp, 2 colors)
    uint32_t biClrImportant;  // 0 = all colors important
    uint32_t colorBlack;      // 0
    uint32_t colorWhite;      // 255,255,255,0 (little Endian -> 0x0FFFFFF)
};
#pragma pack(pop) // end packed structure alignment

// preview bitmap
static BMPHeader myBmpHeader; // header
static uint8_t myBitmap[MAXBUFFERSIZE]; // RAM buffer
// global config date and time
static dateTimeCfg myDT, tmpDT;  // date and time (file FILEDATETIME)
// printer variables
static prnCfg myPrn, tmpPrn;  // all saved printer parameters
static prnCalc myPrnCalc;     // and calculated values
static prnCfg prnProfiles[MAXPROFILES]; // printer profiles (file FILEPRNCFG)
static unsigned int myStatus;       // current state (Not Paired, OnLine, etc.)
static unsigned int numProfiles;    // number of defined profiles
static unsigned int selectedPrn;    // selected printer
static unsigned int bmpWidth4;      // bitmap width in multiples of 4 bytes
static unsigned int fullBufferSize; // footer size in bytes
static bool prnIsOnline;    // BLE connected flag
static unsigned int bfrPos; // print buffer position
static unsigned long ptrSize, chunks; // current BLE packet size
static uint8_t* ptrStart;   // pointer to start of print buffer
static String lineaMain[2]; // texts to print (main WEB page)

// fonts and rendering routines
#include "PrintFonts.h" // this include depends on some variable declarations, do not move it
#include "Fonts/FreeSansBold18pt7b.h" // height= 25 pixels
#include "Fonts/FreeSansBold24pt7b.h" // height= 35 pixels
#include "Fonts/FreeMonoBold18pt7b.h" // height= 28 pixels
#include "Fonts/FreeMonoBold24pt7b.h" // height= 31 pixels

static const char fontNames[][17]= {"Sans Bold 18","Sans Bold 24","Mono Bold 18","Mono Bold 24"};
static const unsigned int numFonts= 4;  // number of fonts
static const char *mySSID = "Chrono Labeler";  // my ESP32 WiFi name
static const char *passwordAP= MYPASS;  // config password

IPAddress staticIP(0,0,0,0);      // Router's IP (0 -> DHCP)
IPAddress myGateway(0,0,0,0);
IPAddress subnet(255,255,255,0);
IPAddress staticIPAP(10,0,0,1);   // IP in Access Point mode
IPAddress myGatewayAP(10,0,0,1);
IPAddress subnetAP(255,255,255,0);

static const char statusNames[][22]= {"Not Paired", "Disconnected", "OffLine", "Invalid Time", "OnLine", "Printing"};
static const char diaSem[][3]= {"Su","Mo","Tu","We","Th","Fr","Sa"};  // day of week
static const char dateFormats[][3]= {"DD","MM","YY","--"}; // date formats
static const char separators[][8]= {"/", ".", ",", ":", "-", "space", "letter","--"}; // day and time separators
static const unsigned int numSeparators= sizeof(separators) / sizeof(separators[0]) ; // number of separators
static const char hourFormats[][4]= {"H12","H24","MM","--"}; // time format
static const unsigned int numHourFormats= sizeof(hourFormats) / sizeof(hourFormats[0]); // number of time formats
static const char letterFormats[][2]= {"d", "m", "y", "h", "m"};  // letter as separation (i.e. 10h23m)
/*
dtFormats contents:
    | 0       | 1       | 2       | 3       | 4       | 5        | 6        | 7      |
PRG | pulldt0 | pulldt1 | pulldt2 | pulldt3 | pulldt4 | pulldt5  | pulldt6  | diasem |
WEB | PULLD1R | PULLD2R | PULLD3R | PULLT1R | PULLT2R | PULLDSEP | PULLTSEP | SEMANA |
----+---------+---------+ --------+ --------+ --------+ ---------+ ---------+------- +
V 0 | --      | --      | --      | --      | --      | /        | /        | T != 0 |
a 1 | DD      | DD      | DD      | H12     | H12     | .        | .        |        |
l 2 | MM      | MM      | MM      | H24     | H24     | ,        | ,        |        |
o 3 | YY      | YY      | YY      | MM      | MM      | :        | :        |        |
r 4 |         |         |         |         |         | -        | -        |        |
e 5 |         |         |         |         |         | space    | space    |        |
s 6 |         |         |         |         |         | letter   | letter   |        |
*/
static const byte defaultFormats[8]= {1,0,2,0,2,4,3,0}; // american format (default) MM-DD-YY - Day of week OFF - H12:MM
//static const byte defaultFormats[8]= {0,1,2,1,2,0,3,1};  // Argentina format DD/MM/YY - Day of week ON - H24:MM
static const char commonNTPs[][16]={"pool.ntp.org", "us.pool.ntp.org", "time.nist.gov", "ar.pool.ntp.org"};
static const char numNTPs= sizeof(commonNTPs) / sizeof(commonNTPs[0]);  // number of NTP servers defined
static const unsigned int DEFAULTNTP= 0;    // default NTP for USA
static const unsigned int MINNTPLENGTH= 12; // minmum NTP server name length
static const long DEFAULTGMTOFFSET= MYGMTOFFSET; // local time GMT offset
static const int DEFAULTDAYLIGHT= 0;  // default daylight saving offset
static const int MINGMT= -12; // minimum value for time zone
static const int MAXGMT= 14;  // max value
static const int MINDAYLIGHT= -1; // daylight saving min value
static const int MAXDAYLIGHT= 2;  // max
static GFXfont* fontPtr[numFonts];  // pointers to fonts
static unsigned int fontHeight[numFonts]= {25, 35, 28, 31};  // font heights in pixels (to determine margins, GFX fonts are drawn from the bottom up)
static time_t now;  // NTP time
static String myDate[2]; // current date and time (formatted to 2 digits)
static bool timeIsValid, yelIsBlinking; // NTP time OK and yellow LED state
static bool mDNS; // flag to initialize mDNS only once
Ticker blinkerBlue, blinkerYellow, blinkerGreen;  // variables to blink the LEDs
static int oldInp;    // last button state
static int boton;     // current button state
static bool btnHeld;  // if the button was held
static unsigned long debounce;      // elapsed time since last button state change
static unsigned long milliLoop;     // millis() value at the start of main loop
static unsigned long debounceTime;  // variable to compute delta time (button)
static unsigned long retryWifi;     // WiFi connection retry
static unsigned long lastTmTry;     // retry delay to get NTP time
static int lectura; // button port value
static unsigned int wifiState; // current WiFi state (see enum WifiStates)
static volatile unsigned int blinkStateB, blinkStateG, blinkStateY; // LED states
static File dataFile; // file in use
static unsigned int bytesRead;  // number of bytes read for a file
static String fileName; // name of file in use (only 1 file is open at a time)
static swiCfg savedCfg; // WiFi struct (FILEWIFI)
static String ssid;     // SSID saved (FILESSID)
static String password; // router password (FILESSID)
static int i, j; // general counters

WebServer server(80); // my WEB page Server

class MyScanCallbacks : public NimBLEScanCallbacks {  // BLE scan callback routine
String myName;

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    std::string deviceName= advertisedDevice->getName();
    myName= String(deviceName.c_str());
#ifdef FORCETP
    if ((myName.length() >= 2) && (myName.startsWith("TP"))) { // filter out BLE names not starting in "TP"
#else
    if (myName.length() != 0) {
#endif      
      if (numDevices < MAXDEVICES) {  // found a device, save it in the table
        foundDevices[numDevices]= myName;
        foundMacs[numDevices]= advertisedDevice->getAddress();
        numDevices= numDevices + 1; // can't use numDevices++ in volatile var
      }
      else {  // table is full
        foundDevices[numDevices - 1]= "Too many devices";
        foundMacs[numDevices - 1]= nullAddr;
        pScan->stop();
      }
    }
  }
};

// blink yellow LED
void IRAM_ATTR blinkeaY() {
  blinkStateY= (blinkStateY == LEDON) ? LEDOFF : LEDON;
  digitalWrite(GPIOyellow, blinkStateY);
}

// blink green LED
void IRAM_ATTR blinkeaG() {
  blinkStateG= (blinkStateG == LEDON) ? LEDOFF : LEDON;
  digitalWrite(GPIOgreen, blinkStateG);
}

// blink or flash blue LED
void IRAM_ATTR blinkeaB() {
  switch (blinkStateB) {
    case LEDBLINKON:
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDBLINKOFF;
      break;
    case LEDBLINKOFF:
      digitalWrite(GPIOblue, LEDON);
      blinkStateB= LEDBLINKON;
      break;
    case LEDFLASHON:
      blinkerBlue.attach_ms(FLASHTIMEOFF , blinkeaB);
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDFLASHOFF;
      break;
    case LEDFLASHOFF:
      blinkerBlue.attach_ms(FLASHTIMEON , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      blinkStateB= LEDFLASHON;
      break;
    default:
      blinkerBlue.detach();
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDOFF;
      break;
  }
}

// blue LED update
void updateBlueLED(int newState) {
  blinkStateB= newState;
  blinkerBlue.detach();
  switch (newState) {
    case LEDOFF:
      digitalWrite(GPIOblue, LEDOFF);
      break;
    case LEDON:
      blinkStateB= LEDON;
      digitalWrite(GPIOblue, LEDON);
      break;
    case LEDBLINKON:
      blinkerBlue.attach_ms(BLUEBLINK , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      break;
    case LEDFLASHON:
      blinkerBlue.attach_ms(FLASHTIMEON , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      break;
    default:
      newState= LEDOFF;
      digitalWrite(GPIOblue, LEDOFF);
      break;
  }  
}

bool validMask(String &mascara, IPAddress &validado) {  // WiFi mask validation, save in IPAddress
  unsigned int posi;
  unsigned int indice;
  bool expectDigit= true;
  if ((mascara.length() < 7) || (mascara.length() > 15)) return false;
  posi= 0;
  for (indice= 0; indice < mascara.length(); indice++) {
    if (expectDigit){
      expectDigit= false;
      if (isDigit(mascara.charAt(indice)))
        validado[posi]= (mascara.charAt(indice) - '0');
      else return false;
    }
    else {
      if (isDigit(mascara.charAt(indice))) {
        if (validado[posi] > 25) return false;
        validado[posi]*= 10;
        if ((validado[posi] == 250) && ((mascara.charAt(indice)) > '5')) return false;
        validado[posi]+= (mascara.charAt(indice) - '0');
      }
      else {
        if (mascara.charAt(indice) != '.') return false;
        expectDigit= true;
        posi++;
        if (posi>3) return false;
      }
    }
  }
  return true;
}

bool esAsteriscos(String &clave) {  // check if password is all asterisks
unsigned int ind;

  for (ind= 0; ind < clave.length(); ind++) {
    if (clave.charAt(ind) != '*') return false;
  }
  return true;
}

String typeOfEnc(int tipo) {  // type of ESP32 WiFi encryption
  switch (tipo) {
  case WIFI_AUTH_OPEN:
    return "&nbsp;---";
    break;
  case WIFI_AUTH_WEP:
    return "WEP";
    break;
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
    break;
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
    break;
  case  WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA+WPA2";
    break;
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
    break;
  }
  return "ERROR";
}

void respondeConfigWifi() { // Build WiFi configuration page (please don't use my code in your programs)
  unsigned int cantIDs;
  unsigned int ilista;
  String listaSSIDs;
  String miConfig= wifiCfg;

  miConfig.replace("defaultSSID", ssid);
  miConfig.replace("defaultPASS", (password == "") ? "" : "********");
  if (staticIP[0] == 0) {
    miConfig.replace("staticIP", "");
    miConfig.replace("myGateway", "");
  }
  else {
    miConfig.replace("staticIP", staticIP.toString());
    miConfig.replace("myGateway", myGateway.toString());
  }
  miConfig.replace("subnet", (subnet[0] == 0) ? "" : subnet.toString());
  miConfig.replace("TOUT", String(savedCfg.retryWifi / 60000));
  switch(savedCfg.initialWifi) {
    case WIFIAPM:
      miConfig.replace("CHECKAPM"," checked=\"checked\"");
      miConfig.replace("CHECKRTR","");
      miConfig.replace("CHECKOFF","");
      break;
    case WIFION:
      miConfig.replace("CHECKAPM","");
      miConfig.replace("CHECKRTR"," checked=\"checked\"");
      miConfig.replace("CHECKOFF","");
      break;
    default:
      miConfig.replace("CHECKAPM","");
      miConfig.replace("CHECKRTR","");
      miConfig.replace("CHECKOFF"," checked=\"checked\"");
      break;
  }
  if (server.arg("scan") == "1") {
    listaSSIDs= F("<br><p style=\"font-size:20px\"><b>WiFi networks detected:</b><br>");
    cantIDs= WiFi.scanNetworks();
    if (cantIDs != -1) {
      listaSSIDs+= F("<table><tr><th>Select your SSID</th><th>Signal</th><th>Encryption</th></tr>");
      for(ilista=0; ilista<cantIDs; ilista++) {
        listaSSIDs+= F("<tr><td><p style=\"font-size:15px\">&nbsp;");
        listaSSIDs+= F("<a href= \"javascript:window.scrollTo(0,0);\" onclick=\"document.getElementById('elssid').value = this.innerText\">");
        listaSSIDs+= WiFi.SSID(ilista);
        listaSSIDs+= F("</a>&nbsp;</td><td>&nbsp;");
        listaSSIDs+= WiFi.RSSI(ilista);
        listaSSIDs+= F("dBm&nbsp;</td><td>&nbsp;");
        listaSSIDs+= typeOfEnc(WiFi.encryptionType(ilista));
        listaSSIDs+= "</td><tr>";
      }
      listaSSIDs+= "</table></p>";
    }
    else listaSSIDs+= F("***None***</p>");
    listaSSIDs+= F("<br><input type=\"button\" value=\"Scan WiFi networks\" onclick=\"location.reload();\">");
    listaSSIDs+= ScrollEnd;
  }
  else listaSSIDs= F("<br><input type=\"button\" value=\"Scan WiFi networks\" onclick=\"location.replace('/configWifi?scan=1')\">");
  miConfig.replace("PLACEHLD", listaSSIDs);
  server.send(200, "text/html", miConfig);
}

void respondeGuardaWifi() {  // validate and save WiFi config (pease don't use my code in your programs)
  IPAddress newIP, newGate, newMask;
  bool todoValido, reconectar, cambioExplorador;
  String newSSID, newPass, newCfg;
  unsigned int newIniWifi;
  unsigned long newRetryWifi;
  String iniWifi, ipTemp, gateTemp, maskTemp;
  String invalpar= F("<td>&nbsp;Inv&aacute;lido&nbsp;</td>");

  reconectar= false;
  cambioExplorador= false;
  todoValido= true;
  newCfg= CfgHeader;
  newCfg+= F("<table><tr><td><b>SSID: </b></td>");
  newSSID= server.arg("mySSID");
  if (newSSID.length() == 0) {
    newCfg+= invalpar;
    todoValido= false;
  }
  else newCfg+= "<td>&nbsp;" + newSSID + "&nbsp;</td>";
  newCfg+= F("</tr><tr><td><b>Password:&nbsp;</b></td>");
  newPass= server.arg("myPASS");
  if (newPass.length() == 0 ) {
    newCfg+= invalpar;
    todoValido= false;
  }
  else newCfg+= F("<td>&nbsp;********&nbsp;</td>");
  ipTemp= server.arg("myIP");
  maskTemp= server.arg("myMask");
  gateTemp= server.arg("myGate");
  if ((validMask(ipTemp, newIP)) && (validMask(gateTemp, newGate)) && (validMask(maskTemp, newMask))) {
    if ((newIP[0] == 0) || (newIP[3] == 0) || (newGate[0] == 0) || (newGate[3] == 0) || (newMask[0] == 0)) {
      // invalid! -> DHCP!
      newIP= IPAddress(0,0,0,0);
      newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td><td>&nbsp;DHCP&nbsp;</td>");
    }
    else {
      newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newIP.toString() + "&nbsp;</td>";
      newCfg+= F("</tr><tr><td><b>Gateway:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newGate.toString() + "&nbsp;</td>";
      newCfg+= F("</tr><tr><td><b>Mask:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newMask.toString() + "&nbsp;</td>";
    }
  }
  else {
    // not even an IP -> DHCP!
    newIP= IPAddress(0,0,0,0);
    newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td><td>&nbsp;DHCP&nbsp;</td>");
  }
  newRetryWifi= server.arg("timeout").toInt() * 60000;
  newCfg+= F("</tr><tr><td><b>Timeout (min):&nbsp;</b></td><td>&nbsp;");
  if (newRetryWifi == 0)
    newCfg+= "Infinite";
  else newCfg+= "&nbsp;" + String(newRetryWifi / 60000);
  newCfg+= "&nbsp;</td>";
  iniWifi= server.arg("inicio");
  newCfg+= F("</tr><tr><td><b>WiFi startup:&nbsp;</b></td><td>&nbsp;");
  if (iniWifi == "router") {
    if (todoValido) {
      newIniWifi=WIFION;
      newCfg+= F("Router Connection");
    }
    else {
      newIniWifi=WIFIAPM;
      newCfg+= F("Local");
    }
  }
  else if (iniWifi == "ap") {
    newIniWifi= WIFIAPM;
    newCfg+= "Access Point mode";
  }
  else {
    newIniWifi= WIFIOFF;
    newCfg+= "OFF";
  }
  newCfg+="&nbsp;</td></tr></table></p>";
  if (esAsteriscos(newPass)) newPass= password;
  if (todoValido) {
    if ((staticIP == newIP) && (myGateway == newGate) && (subnet == newMask) && (savedCfg.retryWifi == newRetryWifi) && (savedCfg.initialWifi == newIniWifi) && (ssid == newSSID) && (password == newPass))
      newCfg+= F("<br><p style=\"font-size:20px\"><b>The configuration has not changed.</b><br><br><input type=\"button\" value=\"Back to configuration\" onclick=\"history.back();\">");
    else {
      reconectar= true;
      newCfg+= F("<br><p style=\"font-size:20px\"><b>The new configuration has been<br>saved</b>");
      if (((wifiState == WIFICON) && (newIniWifi != WIFION)) || ((wifiState != WIFICON) && (wifiState != newIniWifi))) {
        cambioExplorador= true;
        newCfg+= F("<br>Mode change.");
      }
      else if ((staticIP != newIP) || (ssid != newSSID) || (password != newPass)) {
        cambioExplorador= true;
        newCfg+= F("<br>Changing Router or IP.");
      }
      else newCfg+= F("<br><br><b></p><input type=\"button\" value=\"Accept\" onclick=\"history.go(-2);\"><br>");
    }
  }
  else {
    if (newSSID.length() == 0) {
      newIP= IPAddress(0,0,0,0);
      newGate= IPAddress(0,0,0,0);
      newMask= IPAddress(255,255,255,0);
      newIniWifi= WIFIAPM;
      newRetryWifi= DEFAULTTIMEOUT;
      newSSID="";
      newPass="";
      if ((staticIP == newIP) && (myGateway == newGate) && (subnet == newMask) && (savedCfg.retryWifi == newRetryWifi) && (savedCfg.initialWifi == newIniWifi) && (ssid == newSSID) && (password == newPass))
        newCfg+= F("<br><p style=\"font-size:20px\"><b>The configuration has not changed.</b><br><br><input type=\"button\" value=\"Back to configuration\" onclick=\"history.back();\">");
      else {
        newCfg= CfgHeader;
        newCfg+= F("<br><p style=\"font-size:20px\"><b>Configuration Deleted.</b></p><br><br>");
        reconectar= true;
        if (wifiState == WIFIAPM)
          newCfg+= F("<b><input type=\"button\" value=\"Accept\" onclick=\"history.go(-2);\"><br>");
        else {
          newCfg+= F("<br>Changing mode to AP.");
          cambioExplorador= true;
        }
      }
    }
    else newCfg+= F("<br><p style=\"font-size:20px\"><b>Invalid Configuration.</b><br><br><input type=\"button\" value=\"Back to configuration\" onclick=\"history.back();\">");
  }
  if (cambioExplorador) newCfg+= F(" This page is<br>no longer valid.<br><br><b>*** Close the Browser ***</b><br>");
  newCfg+="</body></html>";
  server.send(200, "text/html", newCfg);
  if (reconectar) {
    staticIP= newIP;
    myGateway= newGate;
    subnet= newMask;
    for (i=0; i<4; i++) {
      savedCfg.staticIP[i]= staticIP[i];
      savedCfg.myGateway[i]= myGateway[i];
      savedCfg.subnet[i]= subnet[i];
    }
    savedCfg.initialWifi= newIniWifi;
    savedCfg.retryWifi= newRetryWifi;
    ssid= newSSID;
    password= newPass;
    if (newSSID.length() == 0) {
      fileName= FILEWIFI;
      if (LittleFS.exists(fileName)) LittleFS.remove(fileName.c_str());
      fileName= FILESSID;
      if (LittleFS.exists(fileName)) LittleFS.remove(fileName.c_str());
      if (cambioExplorador) {
        delay(2000);
        nuevoModoWiFi(WIFIOFF);
        delay(2000);
        nuevoModoWiFi(savedCfg.initialWifi);
      }
    }
    else {
      fileName= FILEWIFI;
      dataFile = LittleFS.open(fileName.c_str(), "w");
      dataFile.write((byte *)&savedCfg, sizeof(savedCfg));
      dataFile.close();
      fileName= FILESSID;
      dataFile= LittleFS.open(fileName.c_str(), "w");
      dataFile.println(ssid);
      dataFile.println(password);
      dataFile.close();
      if (cambioExplorador) delay(2000);
      nuevoModoWiFi(WIFIOFF);
      delay(2000);
      nuevoModoWiFi(savedCfg.initialWifi);
    }
  }
}

void respondePreview() {  // make BMP to show preview using tmpPrn instead of myPrn
  String miWeb= Preview;

  lineaMain[0]= server.arg("line0");
  lineaMain[1]= server.arg("line1");
  switch (server.arg("prvw").toInt()) {
    case 7: // preview (main page)
      selectedPrn= server.arg("selPrn").toInt();
      tmpPrn= prnProfiles[selectedPrn];
      tmpPrn.blackBackground= (server.arg("color").toInt() == 1);
      tmpPrn.centered= (server.arg("centrar") == "on");
      tmpPrn.fontNumber[0]= server.arg("pullf0").toInt();;
      tmpPrn.fontNumber[1]= server.arg("pullf1").toInt();
      recalcPrinterHeaders(tmpPrn);
      if (lineaMain[0].length() == 0) // si el texto de la primer linea de la pagina principal esta vacio mostrar la hora
        formatDT(myDT, tmpPrn);
      else
        buildBitmap(tmpPrn, lineaMain);
      break;
    case 8: // preview (printer config)
      loadPrnFromWeb(tmpPrn);
      recalcPrinterHeaders(tmpPrn);
      formatDT(myDT, tmpPrn);
      break;
    case 9: // preview (date and time config)
      loadDtFromWeb(tmpDT);
      tmpPrn= prnProfiles[0];
      recalcPrinterHeaders(tmpPrn);
      formatDT(tmpDT, tmpPrn);
      break;
  }
  miWeb.replace("myWI", String(myPrnCalc.bitmapWidth));
  miWeb.replace("myHE", String(myPrnCalc.bitmapHeight));
  server.send(200, "text/html", miWeb);
}

void countProfiles() {  // count profiles
  numProfiles= 0;
  for (i=0; i < MAXPROFILES; i++) {  // count profiles in use
    if (prnProfiles[i].prnLabel[0] == 0) break;
    numProfiles++;
  }
}

void setPrnOffline() {  // set printer offline (to change profiles)
  if (pClient != nullptr) {
    if (pClient-> isConnected()) pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient= nullptr;
  }
  prnIsOnline= false;
}

void updateStatus() { // update state shown in main WEB page
  if (macIsZeros(myPrn.address))
    myStatus= STATNOPAIR;
  else if (prnIsOnline)
    myStatus= STATONLINE;
  else
    myStatus= STATDISCON;
}

void respondeConfigPrn() {  // Build printer config WEB page
  unsigned int ilista, newPrn;
  String listaPRNs, selecc;

  String miWeb= ConfPrn;
  newPrn= server.arg("selCfgPrn").toInt();
  if (newPrn != 0) {
    selectedPrn= newPrn - 1;
    setPrnOffline();
    myPrn= prnProfiles[selectedPrn];
    recalcPrinterHeaders(myPrn);
  }
  selecc= String(selectedPrn + 1)+"/"+String(numProfiles)+" (Max="+String(MAXPROFILES)+")";
  miWeb.replace("NUMPERF", selecc);
  miWeb.replace("myPROF", String(myPrn.prnLabel));
  miWeb.replace("myNAME", String(myPrn.prnBtName));
  miWeb.replace("PAIRED", macIsZeros(myPrn.address) ? "&nbsp;Not" : "");
  miWeb.replace("WMM", String(myPrn.WIDTHmm));
  miWeb.replace("HMM", String(myPrn.HEIGHTmm));
  miWeb.replace("EXFT", String(myPrn.extraFooter));
  miWeb.replace("MAXSUP", String(myPrn.HEIGHTmm << 3));
  miWeb.replace("MS1", String(myPrn.topMargin[0]));
  miWeb.replace("MS2", String(myPrn.topMargin[1]));
  miWeb.replace("MIZQ", String(myPrn.leftMargin));
  miWeb.replace("CENTERCHECK", myPrn.centered ? "checked" : "");
  miWeb.replace("CDELAY", String(myPrn.chunkDelay));
  miWeb.replace("RDELAY", String(myPrn.resetDelay));
  miWeb.replace("CHECKWHITE", myPrn.blackBackground ? "" : " checked=\"checked\"");
  miWeb.replace("CHECKBLACK", myPrn.blackBackground ? " checked=\"checked\"" : "");
  miWeb.replace("CHECKLAND", myPrn.landscape ? " checked=\"checked\"" : "");
  miWeb.replace("CHECKPORT", myPrn.landscape ? "" : " checked=\"checked\"");
// pulldown profiles
  selecc= "";
  for (i= 0; i < numProfiles; i++) {
    selecc+= "<option value=\""+String(i+1)+"\"";
    if (selectedPrn == i) selecc+= " selected";
    selecc+= ">"+String(prnProfiles[i].prnLabel)+"</option>\r\n";
  }
  miWeb.replace("PULLCFG", selecc);
  // pulldowns fonts
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numFonts; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myPrn.fontNumber[j] == i) selecc+= " selected";
      selecc+= ">"+String(fontNames[i])+"</option>\r\n";
    }
    miWeb.replace("PULLFR"+String(j), selecc);
  }
// scan BLE devices
  numDevices= 0;
  if (server.arg("scan")== "1") {
    pScan= NimBLEDevice::getScan();
    pScan->clearResults();  // en caso de que ya hayan escaneado
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setScanCallbacks(new MyScanCallbacks()); 
    pScan->start(SCANTIME);
    listaPRNs= F("<br><p style=\"font-size:20px\"><b>Detected BLE devices:</b><br>");
    while (pScan->isScanning()) yield();  // wait for async BLE scan to complete
    if (numDevices != 0) {  // build list of found printers
      listaPRNs+= F("<table><tr><th><p style=\"font-size:20px\">&nbsp;Select the Printer&nbsp;&nbsp;</th></tr>");
      for(ilista=0; ilista < numDevices; ilista++) {
        listaPRNs+= F("<tr><td>&nbsp;");
        listaPRNs+= F("<a href= \"javascript:window.scrollTo(0,0);\" onclick=\"document.getElementById('elname').value = this.innerText; document.getElementById('vincu').innerHTML = \'Paired\';\">");
        listaPRNs+= foundDevices[ilista];
        listaPRNs+= F("</a>&nbsp;</td><tr>");
      }
      listaPRNs+= "</table></p>";
    }
    else listaPRNs+= F("***None in Range***</p>");
    listaPRNs+= F("<br><input type=\"button\" value=\"Pair\" onclick=\"location.reload();\">");
    listaPRNs+= ScrollEnd;
  }
  else listaPRNs= F("<br><input type=\"button\" value=\"Pair\" onclick=\"location.replace('/configPrn?scan=1')\">");
  miWeb.replace("PLACEHLD", listaPRNs);
  server.send(200, "text/html", miWeb);
}

void respondeConfigDate() { // Build date and time confige page
  String selecc;
  String miWeb= ConfDate;
  // day month year
  for (j= 0; j < 3; j++) {
    selecc= "";
    for (i= 0; i < 4; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j] == i) selecc+= " selected";
      selecc+= ">"+String(dateFormats[i])+"</option>\r\n";
    }
    miWeb.replace("PULLDR"+String(j), selecc);
  }
  // hours and minutes
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numHourFormats; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j+3] == i) selecc+= " selected";
      selecc+= ">"+String(hourFormats[i])+"</option>\r\n";
    }
    miWeb.replace("PULLTR"+String(j), selecc);
  }
  // date and time separators
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numSeparators; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j+5] == i) selecc+= " selected";
      selecc+= ">"+String(separators[i])+"</option>\r\n";
    }
    miWeb.replace("PULLSEP"+String(j), selecc);
  }
  miWeb.replace("DIASECHECK", (myDT.dtFormats[7] == 0) ? "" :"checked");  // day of week checkmark
  miWeb.replace("NTPR",myDT.ntpServer); // NTP server
  selecc= "";
  for (i= 0; i < numNTPs; i++) {  // pulldown NTP
    selecc+= "<option value=\""+String(i)+"\"";
    if (String(commonNTPs[i]) == String(myDT.ntpServer)) selecc+= " selected";
    selecc+= ">"+String(commonNTPs[i])+"</option>\r\n";
  }
  miWeb.replace("PULLNTP", selecc);
  miWeb.replace("NTPTZR", myDT.timeZone); // NTP time zone
  miWeb.replace("HUSOR",String(myDT.gmtOffset/3600)); // time zone
  miWeb.replace("VERAR",String(myDT.daylight/3600));  // daylight saving time
  server.send(200, "text/html", miWeb);
}

void loadDtFromWeb(dateTimeCfg &thisDT) { // load date and time parameters from WEB page
String ntpt, ext;

  for (i= 0; i < 7; i++) thisDT.dtFormats[i]=server.arg("pulldt"+String(i)).toInt();
  thisDT.dtFormats[7]= (server.arg("diasema") == "on") ? 1 : 0;
  ntpt= server.arg("myNtp");
  i= ntpt.lastIndexOf('.');
  j= ntpt.substring(i + 1).length();
  if ((ntpt.length()< MINNTPLENGTH) || (ntpt.indexOf("..") != -1) || (i <0) || (j < 2))
    strcpy(thisDT.ntpServer, commonNTPs[DEFAULTNTP]);
  else
    strcpy(thisDT.ntpServer, ntpt.c_str());
  strcpy(thisDT.timeZone, server.arg("myTZ").c_str());
  i= server.arg("huso").toInt();
  thisDT.gmtOffset= ((i < MINGMT) || (i > MAXGMT)) ? DEFAULTGMTOFFSET : i * 3600;
  i= server.arg("verano").toInt();
  thisDT.daylight= ((i < MINDAYLIGHT) || (i > MAXDAYLIGHT)) ? DEFAULTDAYLIGHT : i * 3600;
}

void loadPrnFromWeb(prnCfg &thisPrn) {  // load printer config from WEB page
  thisPrn.landscape= server.arg("prnType").toInt();
  thisPrn.WIDTHmm= server.arg("anchomm").toInt();
  thisPrn.HEIGHTmm= server.arg("altomm").toInt();
  thisPrn.extraFooter= server.arg("piemm").toInt();
  thisPrn.topMargin[0]= server.arg("margensup1").toInt();
  thisPrn.topMargin[1]= server.arg("margensup2").toInt();
  thisPrn.leftMargin= server.arg("margenizq").toInt();
  thisPrn.centered= (server.arg("centrar") == "on");
  thisPrn.chunkDelay= server.arg("chunkdelay").toInt();
  thisPrn.resetDelay= server.arg("resetdelay").toInt();
  thisPrn.fontNumber[0]= server.arg("pullf0").toInt();
  thisPrn.fontNumber[1]= server.arg("pullf1").toInt();
  thisPrn.blackBackground= (server.arg("color").toInt() == 1);
}

void respondeRoot() { // Build main WEB page
  String miWeb; // pagina web
  String selecc;
  unsigned int opcion;
  int destino;

  miWeb= PaginaWeb; // load from flash
  lineaMain[0]= server.arg("line0");
  lineaMain[1]= server.arg("line1");
  opcion= server.arg("accion").toInt();
  i=  server.arg("selPrn").toInt();
  switch (opcion) {
    case 0: // initial WEB page load or change in pulldown (printer selection)
      selectedPrn= server.arg("selPrn").toInt();
      myPrn= prnProfiles[selectedPrn];
      recalcPrinterHeaders(myPrn);
      setPrnOffline();
      updateStatus();
      break;
    case 1: // Print (main page)
      myPrn.blackBackground= (server.arg("color").toInt() == 1);
      myPrn.centered= (server.arg("centrar") == "on");
      myPrn.fontNumber[0]= server.arg("pullf0").toInt();;
      myPrn.fontNumber[1]= server.arg("pullf1").toInt();
      recalcPrinterHeaders(myPrn);
      if (lineaMain[0].length() == 0) { // if no text try time
        if (timeIsValid)
          formatDT(myDT, myPrn);
        else myStatus= STATINVALTIME;
      }
      else
        buildBitmap(myPrn, lineaMain);  // there is text
      if (timeIsValid || (lineaMain[0].length() != 0)) {
        if ((!prnIsOnline) || (!pClient->isConnected())) prnIsOnline= connect2prn();
        if (prnIsOnline) {
          blinkStateG= LEDON;
          digitalWrite(GPIOgreen, blinkStateG);
          myStatus= STATPRINTING;
// server.handleClient() here would let us update main page status ("Printing" will not be shown in this verion)
// but can create problems with reentrant code; we would have to add restriction to all pages except state upate
// this comment is here in case we convert printing to asychronous (timer) in the future
          printBuffer();
          myStatus= STATONLINE;
        }
        else myStatus= STATOFFLINE;
      }
      blinkStateG= LEDOFF;
      digitalWrite(GPIOgreen, blinkStateG);
      break;
    case 2: // Save printer config (from page /ConfigPrn)
      selecc= server.arg("myProfile");  // Profile name
      selecc.trim();
      destino= -1;
      for (i= 0; i < numProfiles; i++) {  // search for profile by name in saved profiles
        if (selecc == String(prnProfiles[i].prnLabel)) {
          destino= i;
          break;
        }
      }
      if (destino < 0) {  // not found
        if (numProfiles < MAXPROFILES) {  // if there is room, create new profile
          destino= numProfiles;
          numProfiles++;
        }
        else
          destino= selectedPrn;  // if no room overwrite selected profile
      }
      if (destino != selectedPrn) prnProfiles[destino]= prnProfiles[selectedPrn]; // if destination is not the selected profile, replace
      if (destino != 0) { // select this profile
        myPrn= prnProfiles[destino];
        prnProfiles[destino]= prnProfiles[0]; // and move profile[0] to destination
      }
      if (selecc.length() != 0) strcpy(myPrn.prnLabel, selecc.c_str());  // if invalid we take selected's value
      selecc= server.arg("myBleName");  // printer's BLE name
      selecc.trim();
      if (selecc.length() >= minBleNameLength) { // if BLE name has at least 4 characters (TP-x), process it
        if (numDevices == 0) {  // the user clicked the pair button and we found BLE devices?
          if (selecc != String(myPrn.prnBtName)) {
            strcpy(myPrn.prnBtName, selecc.c_str());  // no, save this name (user will have to edit this profile in the future)
            clearMac(myPrn.address);  // clear the MAC
          }
//        else -> updating existing profile
        }
        else {  // we have a list of devices
          j= -1;
          for (i= 0; i < numDevices; i++) { // find exact name
            if (selecc == foundDevices[i]) {
              j= i;
              break;
            }
          }
          if (j == -1) {  // exact name not found, search for an upprcase name that starts like this
            selecc.toUpperCase();
            for (i= 0; i < numDevices; i++) {
              foundDevices[i].toUpperCase();
              if (foundDevices[i].startsWith(selecc)) {
                j= i;
                break;
              }
            }
          }
          if (j != -1) {  // this printer was found in BLE list (and the MAC is saved in MACs list)
            strcpy(myPrn.prnBtName, foundDevices[j].c_str()); // save
            foundMacs[j].reverseByteOrder();  // Nimble saves the MAC in little Endian, we must reverse it
            if (memcmp(myPrn.address, foundMacs[j].getVal(), 6) != 0) {  // is the MAC different from current printer's?
              setPrnOffline();
              myStatus= STATDISCON;
              memcpy(myPrn.address, foundMacs[j].getVal(), 6);
            }
            foundMacs[j].reverseByteOrder();  // reverse back Nimble address in case it's used somewhere else
          }
// there is no else clause here because we leave the original MAC (since a profile can be modified with the printer off)
        }
      }
      else {  // if the printer name has less than 4 characters it's considered null and we delete this printer from the profile
        setPrnOffline();
        clearMac(myPrn.address);
        myPrn.prnBtName[0]= 0;
        myStatus= STATNOPAIR;
      }
      loadPrnFromWeb(myPrn);  // load the data from WEB page
      prnProfiles[0]= myPrn;  // save new profile as default
      recalcPrinterHeaders(myPrn);  // update
      saveProfiles();
      selectedPrn= 0;
      updateStatus();
      break;
    case 3: // delete this printer profile (from page /ConfigPrn)
      if (numProfiles == 1) // if there's only 1 profile, load all defaults
        loadDefaultProfiles();
      else {
        if (selectedPrn != (numProfiles - 1)) { // if the profile we are deleting is not the last
          for (i= selectedPrn + 1; i < numProfiles; i++) prnProfiles[i - 1]= prnProfiles[i]; // move the rest
        }
        numProfiles--;
        prnProfiles[numProfiles].prnLabel[0]= 0;  // mark the last one as deleted (array starts in 0)
      }
      selectedPrn= 0;
      myPrn= prnProfiles[0];
      recalcPrinterHeaders(myPrn);
      saveProfiles();
      updateStatus();
      break;
    case 4: // cancel printer config (from page /ConfigPrn)
      setPrnOffline();
      myPrn= prnProfiles[selectedPrn];
      recalcPrinterHeaders(myPrn);
      updateStatus();
      break;
    case 5: // Save date and time config (from page /configDate)
      loadDtFromWeb(myDT);
      saveDateTime();
      break;
    case 6: // Cancel date and time config (from page /configDate), this CASE could be skipped since there's no "default:"
      break;
  }
// build WEB page
// profiles pulldown
  selecc= "";
  for (i= 0; i < numProfiles; i++) {
    selecc+= "<option value=\""+String(i)+"\"";
    if (selectedPrn == i) selecc+= " selected";
    selecc+= ">"+String(prnProfiles[i].prnLabel)+"</option>\r\n";
  }
  miWeb.replace("PULLOPTIONS", selecc);
  // fonts pulldowns
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numFonts; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myPrn.fontNumber[j] == i) selecc+= " selected";
      selecc+= ">"+String(fontNames[i])+"</option>\r\n";        
    }
    miWeb.replace("PULLFR"+String(j), selecc);
    miWeb.replace("MYLINE"+String(j), lineaMain[j]);
  }
  miWeb.replace("CENTERCHECK", prnProfiles[selectedPrn].centered ? "checked" : ""); // if centered
  miWeb.replace("CHECKWHITE", prnProfiles[selectedPrn].blackBackground ? "" : " checked=\"checked\"");  // update color radio buttons
  miWeb.replace("CHECKBLACK", prnProfiles[selectedPrn].blackBackground ? " checked=\"checked\"" : "");
  miWeb.replace("PRINTENA", macIsZeros(myPrn.address) ? " disabled" : "");  // disable print button if printer is not paired
  if (macIsZeros(myPrn.address)) {
    miWeb.replace("Disconnected", statusNames[0]);  // force state "Not Paired" (be carfeul if you change this string in the page!)
    myStatus= STATNOPAIR;
  }
  server.send(200, "text/html", miWeb); // send page to client
}

void actualizaEstado() {
  server.send(200, "text/plane", statusNames[myStatus]); // update state
}

void responde404() {  // reply undefined pages
  server.send(404, "text/html", F("<p style=\"font-size:30px\">404: Page not found!"));
}

void respondeAbout() { // reply "about"
  String miWeb= About;
  miWeb.replace("VERNUM", FWVERSION);
  server.send(200, "text/html", miWeb);
}

void enviaFavi32(){  // send Windows 32x32 favicon to client
  fileName= ICON32;
  dataFile = LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/png");
  dataFile.close();
}

void enviaFavi192(){  // send Android favicon 192x192 to client
  fileName= ICON192;
  dataFile = LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/png");
  dataFile.close();
}

void enviaLogo() { // send my logo to client
  fileName= MYLOGO;
  dataFile= LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/webp");
  dataFile.close();
}

void enviaPreview() {  // send preview
  uint8_t* ptrFrom;
  uint8_t* ptrTo;

  if (!tmpPrn.blackBackground) {  // if background is white draw frame
    drawLine(0, 0, myPrnCalc.bitmapWidth-1, 0, BLACK);  // top horizontal line
    drawLine(0, myPrnCalc.bitmapHeight-1, myPrnCalc.bitmapWidth-1, myPrnCalc.bitmapHeight-1, BLACK);  // bottom horizontal line
    drawLine(0, 0, 0, myPrnCalc.bitmapHeight-1, BLACK); // left vertical line
    drawLine(myPrnCalc.bitmapWidth-1, 0, myPrnCalc.bitmapWidth-1, myPrnCalc.bitmapHeight-1, BLACK); // right vertical line
  }
  // ajust bitmap lines to 32 bits (inside same buffer)
  if ((tmpPrn.WIDTHmm % 4) != 0) {
    ptrFrom= &myBitmap[0];
    ptrTo= ptrFrom+myPrnCalc.bitmapHeight*bmpWidth4;  // destination pointer of last line adjusted to 32 bits
    ptrFrom+= myPrnCalc.bitmapHeight*tmpPrn.WIDTHmm;  // source pointer las buffer line
    for (i= 0; i < myPrnCalc.bitmapHeight; i++) { // repeat for all lines
      ptrFrom-= tmpPrn.WIDTHmm;
      ptrTo-= bmpWidth4;
      memmove(ptrTo, ptrFrom, tmpPrn.WIDTHmm);  // we can't use memcpy() because source and destination overlap at the buffer start
    }
  }
  // send bitmap to server
  server.setContentLength(myBmpHeader.biSizeImage + BMPHEADERSIZE); // contents size
  server.send(200, "image/bmp", "");  // contents type
  server.sendContent((const char*)&myBmpHeader, BMPHEADERSIZE);  // send BMP header
  server.sendContent((const char*)&myBitmap, myBmpHeader.biSizeImage);  // send pixels
}

void iniciaPagina() { // start WEB pages
  server.on("/", respondeRoot);   // handle main page
  server.on("/updEstado", actualizaEstado);  // update state (main page AJAX)
  server.on("/configWifi", respondeConfigWifi);
  server.on("/save_Wifi_config" , HTTP_POST, respondeGuardaWifi);
  server.on("/configPrn", respondeConfigPrn);
  server.on("/configDate", respondeConfigDate);
  server.on("/preview", respondePreview);
  server.on("/bitmap.bmp", enviaPreview);
  server.on("/favicon32.png", enviaFavi32);
  server.on("/favicon192.png", enviaFavi192);
  server.on("/logo.webp", enviaLogo);
  server.on("/about", respondeAbout);
  server.onNotFound(responde404);
  server.begin();
}

void nuevoModoWiFi(int modoWiFi) { // change WiFi mode (AP, router, OFF)
  // first disconnect
  timeIsValid= false;
  blinkerYellow.detach();
  digitalWrite(GPIOyellow, LEDOFF);
  if ((wifiState == WIFION) || (wifiState == WIFICON)) {
    updateBlueLED(LEDWIFIOFF);
    WiFi.disconnect();
    delay(2000);
  }
  if (wifiState == WIFIAPM) {
    updateBlueLED(LEDWIFIOFF);
    WiFi.softAPdisconnect();
    delay(2000);
  }
  // now establish new connection
  switch (modoWiFi) {
    case WIFION:
      if (ssid.length() == 0) { // if the router is invalid switch to AP mode
        updateBlueLED(LEDFLASHON);
        WiFi.softAPConfig(staticIPAP, myGatewayAP, subnetAP);
        WiFi.softAP(mySSID, passwordAP, MYCHANNEL, false, MAXCONNCT);
        iniciaPagina();
        wifiState= WIFIAPM;
      }
      else {
        WiFi.hostname(mySSID);  // connect to Router
        updateBlueLED(LEDBLINKON);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname("ChronoLabeler");
        WiFi.begin(ssid.c_str(),password.c_str());
        if ((staticIP[0] != 0) && (staticIP[3] != 0) && (myGateway[0] != 0) && (myGateway[3] != 0)) WiFi.config(staticIP, myGateway, subnet, myGateway); // use gateway as DNS1
        wifiState= WIFION;
        retryWifi= milliLoop;
      }
      break;
    case WIFIAPM:  // AP mode
      updateBlueLED(LEDFLASHON);
      WiFi.softAPConfig(staticIPAP, myGatewayAP, subnetAP);
      WiFi.softAP(mySSID, passwordAP, MYCHANNEL, false, MAXCONNCT);
      iniciaPagina();
      wifiState= WIFIAPM;
      break;
    default:
      WiFi.mode(WIFI_OFF);
      wifiState= WIFIOFF;
      updateBlueLED(LEDWIFIOFF);
      break;
  }
}

void rotate90cw(uint8_t* source, int width, int height) {  // rotate bitmap 90° (for landscape printers)
  unsigned int in_bytes,out_bytes;
  unsigned int x,y,destX;
  uint8_t rotatedBitmap[myPrnCalc.myBufferSize];

  in_bytes = width >> 3;
  out_bytes = height >> 3;
  memset(rotatedBitmap, 0, myPrnCalc.myBufferSize);  // clear buffer
  //in_bytes * height
  for (y = 0; y < height; y++) {
    destX = height - y - 1;
    for (x = 0; x < width; x++) {
      if (source[y * in_bytes + (x >> 3)] & (1 << (7 - (x % 8)))) // pixel on?
        rotatedBitmap[x * out_bytes + (destX >> 3)] |= (1 << (7 - (destX % 8))); // turn on destination pixel
    }
  }
  memcpy(source, rotatedBitmap, myPrnCalc.myBufferSize);  // move rotated bitmap back into original buffer (this buffer's RAM will get released from the heap)
}

String mac2str(uint8_t addr[6]) {  // convert MAC address into string
String tmp;
  tmp="";
  for (i= 0; i<6; i++) {
    tmp+= String(addr[i],HEX);
    if (i<5) tmp+= ":";
  }
  return tmp;
}

bool macIsZeros(uint8_t* thisAddress) {  // check if MAC is zeros
  for (j= 0; j < 6; j++) if (thisAddress[j] != 0) return false;
  return true;
}

void clearMac(uint8_t addr[6]) {  // clear MAC
  memset(addr, 0, 6);
}

bool connect2prn() {  // connect to printer
  if (macIsZeros(myPrn.address)) {  // is it paired?
    myStatus= STATNOPAIR;
    return false;
  }
  blinkStateG= LEDON;
  digitalWrite(GPIOgreen, blinkStateG);
  blinkerGreen.attach_ms(BLINKINGTIME , blinkeaG);  // Green LED blinking while we connect
  if (pClient != nullptr) { // reconnect
// connection reset
    if (!pClient->connect()) {
      blinkerGreen.detach();
      return false;
    }
    delay(myPrn.resetDelay);  // TP-110 resets faster than the TP-220
  }
  else {  // crear conexion nueva
    pClient= NimBLEDevice::createClient();
    if (pClient == nullptr) {
      blinkerGreen.detach();
      return false;
    }
    pClient->setConnectTimeout(BLETIMEOUT);
    NimBLEAddress printerAddress((uint8_t *)myPrn.address, BLE_ADDR_PUBLIC); // our BLE printer's MAC
    if (!pClient->connect(printerAddress)) {
      blinkerGreen.detach();
      return false;
    }
  }
  // obtener servicio
  pService = pClient->getService(serviceUUID);
  if (pService == nullptr) {
    blinkerGreen.detach();
    return false;
  }
  // obtener characteristic
  pCharacteristic = pService->getCharacteristic(charUUID);
  blinkerGreen.detach();
  return (pCharacteristic != nullptr);  // returns true if the pointer is not null
}

String toStr2(unsigned int value) { // 2 characters formatting for dates and times
char fmtBuf[5];
  sprintf(fmtBuf, "%02d", value);
  return String(fmtBuf);
}

void buildBitmap(prnCfg &thisPrn, String line[2]) { // building the text bitmap in the buffer
  unsigned int myLeftMargin[2];
  unsigned int myTopMargin;
  int width, top, bottom;
  bool cropped;

  recalcPrinterHeaders(thisPrn);
  memset(myBitmap, thisPrn.blackBackground ? BLACK : WHITE, myPrnCalc.myBufferSize);
  for (i= 0; i < 2; i++) {
    cropped= false;
    do {  // cut the string until it fits within the printer's width
      getStringBox(fontPtr[thisPrn.fontNumber[i]], (char *)line[i].c_str(), &width, &top, &bottom);
      if (width > myPrnCalc.bitmapWidth) {
        cropped= true;
        line[i].remove(line[i].length() - 1);
      }
    } while (width > myPrnCalc.bitmapWidth);
    myLeftMargin[i]= (thisPrn.centered)? (myPrnCalc.bitmapWidth - width) >> 1 : thisPrn.leftMargin; // compute center or use margins
    if (cropped || ((myPrnCalc.bitmapWidth - width) <= fontPitch)) myLeftMargin[i]= 0;  // if we had to cut the text cancel left margin
    myTopMargin= thisPrn.topMargin[i]+fontHeight[thisPrn.fontNumber[i]];  // margin plus selected font's height (fonts built bottom up)
    // take the mean of top margins if one of the lines is blank
    if ((line[0].length() == 0) || (line[1].length() == 0)) myTopMargin= (thisPrn.topMargin[0]+fontHeight[thisPrn.fontNumber[0]]+thisPrn.topMargin[1]+fontHeight[thisPrn.fontNumber[1]]) >> 1;
    if (line[i].length() != 0) drawCustomText(fontPtr[thisPrn.fontNumber[i]], myLeftMargin[i], myTopMargin, (char *)line[i].c_str());
  }
}

void formatDT(dateTimeCfg &thisDT, prnCfg &thisPrn) { // create date time bitmap
String fh[3];

  if (timeIsValid) {
    getLocalTime(&timeinfo);
    fh[0]= toStr2(timeinfo.tm_mday);
    fh[1]= toStr2(timeinfo.tm_mon+1);
    fh[2]= toStr2(timeinfo.tm_year-100);
    // format day/month/year
    myDate[0]="";
    for (i= 0; i < 3; i++) {  // DD/MM/YY - MM-DD-YY, etc
      if (i > 0) {  // agregar separador
        if ((myDate[0].length() !=0) && (thisDT.dtFormats[i] != 3)) {
          switch (thisDT.dtFormats[5]) {  // date separators
            case 5:
              myDate[0]+= " ";  // space
              break;
            case 6: // letra
  //            myDate[0]+= letterFormats[thisDT.dtFormats[i-1]]; // this is a special case, handled below
              break;
            case 7: // "--"
              break;
            default:
              myDate[0]+= separators[thisDT.dtFormats[5]];
              break; 
          }
        }
      }
      if (thisDT.dtFormats[i] != 3) {
        myDate[0]+= fh[thisDT.dtFormats[i]];  // dtFormats[3] -> "--""
        if (thisDT.dtFormats[5] == 6) myDate[0]+= letterFormats[thisDT.dtFormats[i]]; // D/M/Y
      }
    }
    fh[0]= toStr2((timeinfo.tm_hour <= 12) ? timeinfo.tm_hour :  timeinfo.tm_hour - 12);
    fh[1]= toStr2(timeinfo.tm_hour);
    fh[2]= toStr2(timeinfo.tm_min);
    // format date and time
    myDate[1]= "";
    if (thisDT.dtFormats[7] != 0) { // day of week
      myDate[1]+= String(diaSem[timeinfo.tm_wday]);
      if (((thisDT.dtFormats[3] != 0) && (thisDT.dtFormats[4] != 0)) || (thisDT.dtFormats[3] == 3) || (thisDT.dtFormats[4] == 3)) myDate[1]+= " "; // if time format is not AM/PM add a space
    }
    for (i= 3; i <= 4; i++) {  // HH:MM - HH:MM am/pm, etc
      if (thisDT.dtFormats[i] != 3) {
        myDate[1]+= fh[thisDT.dtFormats[i]];
        if (thisDT.dtFormats[6] == 6) // separator = letter?
          myDate[1]+= (thisDT.dtFormats[i] == 2) ? letterFormats[4] : letterFormats[3];  // m o h
        else if ((i == 3) && (thisDT.dtFormats[4] != 3)) {  // no separator after minutes if not a letter or there are no minutes
          switch (thisDT.dtFormats[6]) { // time separators
            case 5:
              myDate[1]+= " ";
              break;
            case 7: // "--"
              break;
            default:
              myDate[1]+= separators[thisDT.dtFormats[6]];
              break; 
          }
        }
      }
    }
    if ((thisDT.dtFormats[3] == 0) || (thisDT.dtFormats[4] == 0)) {
      if ((thisDT.dtFormats[7] == 0) || (thisDT.dtFormats[3] == 3) || (thisDT.dtFormats[4] == 3)) myDate[1]+= " ";
      myDate[1]+= (timeinfo.tm_hour <= 12) ? "AM" : "PM";
    }
  }
  else {
    myDate[0]= F("Time");
    myDate[1]= F("not available");
  }
  buildBitmap(thisPrn, myDate);
}

void printBuffer() {  // print the buffer
  if (myPrn.landscape) rotate90cw(myBitmap, myPrnCalc.bitmapWidth, myPrnCalc.bitmapHeight); // if landscape rotate 90 degrees
  pCharacteristic->writeValue(myPrnCalc.sPrnInit.c_str(), myPrnCalc.sPrnInit.length()); // init printer
  delay(INITDELAY);
  pCharacteristic->writeValue(myPrnCalc.prnHeader.c_str(), myPrnCalc.prnHeader.length()); // send TSPL header (SIZE 50mm, etc)
  pCharacteristic->writeValue(myPrnCalc.prnBitHeader.c_str(), myPrnCalc.prnBitHeader.length());
  ptrStart= &myBitmap[0] + myPrnCalc.myBufferSize;
  fullBufferSize= myPrn.extraFooter * myPrnCalc.prnBitmapWidth;  // footer size
  memset(ptrStart, myPrn.blackBackground ? BLACK : WHITE, fullBufferSize);  // clear footer inside buffer
  fullBufferSize+= myPrnCalc.myBufferSize; // add whole bitmap size to length
  ptrStart= &myBitmap[0]; // prepare bitmap pointers
  ptrSize= chunkSize;
  chunks= fullBufferSize/ptrSize; // divide bitmap in BLE packets not bigger than chunkSize (240 bytes)
  bfrPos= 0;
  for (i= 0; i<chunks;i++) {  // send packets
    pCharacteristic->writeValue((uint8_t*)ptrStart, ptrSize);
    delay(myPrn.chunkDelay);  // wait for printer to save this BLE packet
    ptrStart+=chunkSize;
    bfrPos+= chunkSize;
  }
  ptrSize= fullBufferSize % chunkSize;
  if (ptrSize != 0) { // if the bitmap's size is not a multiple of chunkSize send reminder
    pCharacteristic->writeValue((uint8_t*)ptrStart, ptrSize);
    delay(myPrn.chunkDelay);
  }
  pCharacteristic->writeValue(myPrnCalc.prnFooter.c_str(), myPrnCalc.prnFooter.length()); // send TSPL print command "PRINT 1,1" etc
}

void loadDefaultDateTime(dateTimeCfg &thisDT) {
  memcpy(thisDT.dtFormats, defaultFormats, sizeof(thisDT.dtFormats));
  thisDT.gmtOffset= DEFAULTGMTOFFSET; // UTC time in seconds
  thisDT.daylight= DEFAULTDAYLIGHT;   // daylight saving time (to add 1 hour change to 3600)
  strcpy(thisDT.ntpServer, commonNTPs[DEFAULTNTP]);
  strcpy(thisDT.timeZone, MYTIMEZONE); // time zone
}

// default printer configurations
void loadDefaultPrnParams(int whichPrn) {
  /*
  completely clear the strings (not necessary because we do not compare elements of this struct)
  memset(myPrn.prnLabel, 0, sizeof(myPrn.prnLabel));
  memset(myPrn.prnBtName, 0, sizeof(myPrn.prnBtName));
  memset(myPrn.prnInit, 0, sizeof(myPrn.prnInit));
  memset(myPrn.prnGap, 0, sizeof(myPrn.prnGap));
  */
  myPrn.fontNumber[0]= 0;
  myPrn.fontNumber[1]= 0;
  myPrn.blackBackground= false;
  myPrn.centered= true;
  clearMac(myPrn.address);  // by default this printer is not paired
  switch (whichPrn) {
    case 0:
      strcpy(myPrn.prnLabel, "TP-110 (15x30mm)");
      strcpy(myPrn.prnBtName, "TP-110");
      myPrn.WIDTHmm= 30;      // maximum sticker size in mm
      myPrn.HEIGHTmm= 12;     // sticker height in mm
      myPrn.extraFooter= 0;   // footer
      myPrn.chunkDelay= 20;   // BLE packets delay
      myPrn.resetDelay= 500;  // init delay
      myPrn.topMargin[0]= 14; // top margin for line 1 (does not include font height)
      myPrn.topMargin[1]= 50; // top margin for line 2
      myPrn.leftMargin= 16;   // left margin
      strcpy(myPrn.prnInit, "\x1b!S");    // initial reset escape code
      strcpy(myPrn.prnGap, "2 mm,0 mm");  // inter sticker gap in mm (TP-220 does not accept mm in current firmware)
      myPrn.landscape= true;  // landscape printer type
      break;
    case 1:
      strcpy(myPrn.prnLabel, "TP-110 (15x50mm)");
      strcpy(myPrn.prnBtName, "TP-110");
      myPrn.WIDTHmm= 48;
      myPrn.HEIGHTmm= 12;
      myPrn.extraFooter= 48;
      myPrn.chunkDelay= 20;
      myPrn.resetDelay= 500;
      myPrn.topMargin[0]= 14;
      myPrn.topMargin[1]= 50;
      myPrn.leftMargin= 16;
      strcpy(myPrn.prnInit, "\x1b!S");
      strcpy(myPrn.prnGap, "2 mm,0 mm");
      myPrn.landscape= true;
      break;
    case 2:
      strcpy(myPrn.prnLabel, "TP-220 (50x25mm)");
      strcpy(myPrn.prnBtName, "TP-220");
      myPrn.WIDTHmm= 48;
      myPrn.HEIGHTmm= 25;
      myPrn.extraFooter= 0;
      myPrn.chunkDelay= 40;
      myPrn.resetDelay= 3500;
      myPrn.topMargin[0]= 15;
      myPrn.topMargin[1]= 55;
      myPrn.leftMargin= 16;
      strcpy(myPrn.prnInit, "\x1b!S");
      strcpy(myPrn.prnGap, "0.10,0");
      myPrn.landscape= false;
      break;
    default:
      myPrn.prnLabel[0]= 0; // invalidate this profile (marks end of profiles)
      break;
  }
}

// compute parameters that are not saved in config
void recalcPrinterHeaders(prnCfg &thisPrn) {
  myPrnCalc.bitmapWidth= thisPrn.WIDTHmm << 3;
  myPrnCalc.bitmapHeight= thisPrn.HEIGHTmm << 3;
  myPrnCalc.myBufferSize= thisPrn.WIDTHmm * myPrnCalc.bitmapHeight;
  if (thisPrn.landscape) {
    myPrnCalc.prnBitmapWidth= thisPrn.HEIGHTmm;
    myPrnCalc.prnBitmapHeight= myPrnCalc.bitmapWidth + thisPrn.extraFooter;
    myPrnCalc.prnHeader= "SIZE "+String(thisPrn.HEIGHTmm)+" mm,"+String(thisPrn.WIDTHmm);
  }
  else {
    myPrnCalc.prnBitmapWidth= thisPrn.WIDTHmm;
    myPrnCalc.prnBitmapHeight= myPrnCalc.bitmapHeight + thisPrn.extraFooter;
    myPrnCalc.prnHeader= "SIZE "+String(thisPrn.WIDTHmm)+" mm,"+String(thisPrn.HEIGHTmm);
  }
  fontPitch= (myPrnCalc.bitmapWidth + 7) >> 3;
  myPrnCalc.prnHeader+= " mm\r\nDENSITY 8\r\nSPEED 4.0\r\nGAP "+String(thisPrn.prnGap)+"\r\nCLS\r\n";
  myPrnCalc.prnBitHeader= "BITMAP 0,0,"+String(myPrnCalc.prnBitmapWidth)+","+String(myPrnCalc.prnBitmapHeight)+",0,";
  myPrnCalc.sPrnInit= String(thisPrn.prnInit);
  myPrnCalc.prnFooter= "\r\nPRINT 1,1\r\n";
// update BMP headers
  bmpWidth4= ((thisPrn.WIDTHmm + 3) >> 2) << 2; // convert to a multiple of 4
  myBmpHeader.bfType= 0x4D42; // 'BM'
  myBmpHeader.biSizeImage= bmpWidth4 * myPrnCalc.bitmapHeight;
  myBmpHeader.bfSize= myBmpHeader.biSizeImage + BMPHEADERSIZE;  // total size of BMP "file"
  myBmpHeader.bfReserved1= 0;
  myBmpHeader.bfReserved2= 0;
  myBmpHeader.bfOffBits= BMPHEADERSIZE; // 54 + 2*4 byte color table + 2 bytes (aligned to 32 bits)
  myBmpHeader.biSize= 40;  // header size (40 bytes)
  myBmpHeader.biWidth= myPrnCalc.bitmapWidth;
  myBmpHeader.biHeight= -myPrnCalc.bitmapHeight; // Positive -> bottom up
  myBmpHeader.biPlanes= 1;
  myBmpHeader.biBitCount= 1; // 1 bit per pixel
  myBmpHeader.biCompression= 0;
  myBmpHeader.biXPelsPerMeter= 0; // horizontal resolution
  myBmpHeader.biYPelsPerMeter= 0; // vertical resolution
  myBmpHeader.biClrUsed= 0;       // number of colors in palette (0 for 1bpp)
  myBmpHeader.biClrImportant= 0;
  myBmpHeader.colorBlack= 0;  // black color pixel value
  myBmpHeader.colorWhite= 0x0FFFFFF;  // white color pixel value
}

void loadDefaultProfiles() {  // initialize default profiles
  numProfiles= 0;
  for (i= 0; i < MAXPROFILES; i++) {
    loadDefaultPrnParams(i);
    prnProfiles[i]= myPrn;
    if (myPrn.prnLabel[0] == 0) break;
    numProfiles++;
  }
  selectedPrn= 0;
  myPrn= prnProfiles[0];
  recalcPrinterHeaders(myPrn);
}

void saveProfiles() { // save profiles in LittleFS
  fileName= FILEPRNCFG;
  dataFile = LittleFS.open(fileName.c_str(), "w");
  dataFile.write((byte *)&prnProfiles, sizeof(prnProfiles));
  dataFile.close();
}

void saveDateTime() { // save date and time configuration
  fileName= FILEDATETIME;
  dataFile = LittleFS.open(fileName.c_str(), "w");
  dataFile.write((byte *)&myDT, sizeof(myDT));
  dataFile.close();
}

void setup() {
  pinMode(GPIObtn, INPUT_PULLUP); // this pullup is actually not necessary because there is a 10K hardware pullup, but we add it just in case this software is tested in a bare ESP32
  pinMode(GPIOblue, OUTPUT);
  pinMode(GPIOyellow, OUTPUT);
  pinMode(GPIOgreen, OUTPUT);
  digitalWrite(GPIOblue, LEDOFF);
  digitalWrite(GPIOyellow, LEDOFF);
  digitalWrite(GPIOgreen, LEDOFF);
  /* debug
  Serial.begin(115200);
  while (!Serial) yield();
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println("Ready");
  end debug
  */
  oldInp= BTNRELEASED;  // last state of button pin (not pressed)
  boton= BTNRELEASED;   // current button state (with debounce)
  btnHeld= false;       // button has not been held
  wifiState= WIFIOFF;   // deafult WiFi mode
  WiFi.persistent(false);  // we save the configuration, persistent wears the flash unncessarily
  if (!LittleFS.begin()) { // start filesystem
    LittleFS.format();
    LittleFS.begin();
  }
  // load profiles
  prnProfiles[0].prnLabel[0]= 0;
  fileName= FILEPRNCFG;
  bytesRead= 0;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&prnProfiles, sizeof(prnProfiles));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(prnProfiles)) prnProfiles[0].prnLabel[0]= 0;
  // debug to cleanup profiles to defaults
  // prnProfiles[0].prnLabel[0]= 0;
  // end debug
  if (prnProfiles[0].prnLabel[0] == 0) {  // if no profiles use defaults
    loadDefaultProfiles();
    saveProfiles();
  }
  else countProfiles();
  // load date time config
  fileName= FILEDATETIME;
  bytesRead= 0;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&myDT, sizeof(myDT));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(myDT)) {  // if no date time config load default
    loadDefaultDateTime(myDT);
    saveDateTime();
  }
  fileName= MYLOGO;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)myPhoto, sizeof(myPhoto));
    dataFile.close();
  }
  fileName= ICON32;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)Favi32, sizeof(Favi32));
    dataFile.close();
  }
  fileName= ICON192;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)Favi192, sizeof(Favi192));
    dataFile.close();
  }
// load WiFi config
  bytesRead= 0;
  savedCfg.staticIP[0]= 0;
  fileName= FILEWIFI;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&savedCfg, sizeof(savedCfg));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(savedCfg)) {
    staticIP= IPAddress(0,0,0,0);
    myGateway= staticIP;
    subnet= IPAddress(255,255,255,0);
    savedCfg.initialWifi= WIFIAPM;
    savedCfg.retryWifi= DEFAULTTIMEOUT;
    ssid="";
    password="";
  }
  else {
    for (i=0; i < 4; i++) {
      staticIP[i]= savedCfg.staticIP[i];
      myGateway[i]= savedCfg.myGateway[i];
      subnet[i]= savedCfg.subnet[i];
    }
    if ((staticIP[0] != 0) && ((myGateway[0] == 0) || (subnet[0] == 0))) {
      myGateway= staticIP;
      myGateway[3]= 1;
      subnet= IPAddress(255,255,255,0);
    }
    // read SSID and password
    fileName= FILESSID;
    if (LittleFS.exists(fileName)) {
      dataFile = LittleFS.open(fileName.c_str(), "r");
      if (dataFile) {
        ssid= dataFile.readStringUntil('\n');
        if (ssid.length() > 0) ssid.remove(ssid.length()-1);
        password= dataFile.readStringUntil('\n');
        if (password.length() > 0) password.remove(password.length()-1);
        dataFile.close();
      }
      else {
        ssid="";
        password="";
      }
    }
    else {
      ssid="";
      password="";
    }
  }
  wifiState= WIFIOFF;
  mDNS= false;
  nuevoModoWiFi(savedCfg.initialWifi);
  timeIsValid= false;
  yelIsBlinking= false;
  pBackBuffer= (uint8_t *)myBitmap;  // pointer for font rendering routines (this variable is created in the PrintFonts #include)
  prnIsOnline= false;
  selectedPrn= 0;
  myPrn= prnProfiles[0];
  recalcPrinterHeaders(myPrn);
  fontPtr[0]= (GFXfont *)&FreeSansBold18pt7b;  // Sans Bold 18
  fontPtr[1]= (GFXfont *)&FreeSansBold24pt7b;  // Sans Bold 24
  fontPtr[2]= (GFXfont *)&FreeMonoBold18pt7b;  // Mono Bold 18
  fontPtr[3]= (GFXfont *)&FreeMonoBold24pt7b;  // Mono Bold 24
  updateStatus();
  lineaMain[0]= "";
  lineaMain[1]= "";
  NimBLEDevice::init("");
  pClient= nullptr;
//  NimBLEAddress localAddress = NimBLEDevice::getAddress();  // ESP32 BLE MAC
//  Serial.print("My MAC: ");
//  Serial.println(localAddress.toString().c_str());
  retryWifi= 0;
  milliLoop= millis();
  if (milliLoop == 0) milliLoop++;
  lastTmTry= milliLoop - EPOCHRETRY;  // substract so it starts on first loop
  debounce= milliLoop; // button unchanged
}

void loop() {
  milliLoop= millis();  // we use the same elapsed time in the whole loop
  if (milliLoop == 0) milliLoop++; // this can happen every 49 days (are we thorough?). We skip 0 to be able to compare time "0" as undefined
// read button
  lectura= digitalRead(GPIObtn);  // take a port reading
  debounceTime= milliLoop - debounce;
  // process button
  if (lectura != oldInp) {  // button state changed
    oldInp= lectura;        // this is the new "old state"
    debounce= milliLoop;    // debounce time start
  }
  else {
    if (debounceTime >= DEBTIME) { // button pressed longer than DEBTIME (debounced)
      if (lectura == boton) {  // if the port is equal to the current button state
        if ((debounceTime > HOLDTIME) && (boton == BTNPRESSED) && (!btnHeld)) {  // let's see if the button was held
          // button held, change WiFi mode
          btnHeld= true;
          switch (wifiState) {
            case WIFIOFF:  // was OFF -> change to WiFi ON
              nuevoModoWiFi(WIFION);
              break;
            case WIFION:  // ON but not connected -> change to AP mode
              nuevoModoWiFi(WIFIAPM);
              break;
            case WIFICON:  // ON and connected -> change to AP mode
              nuevoModoWiFi(WIFIAPM);
              break;
            default: // AP mode (or disconnected) -> turn WiFi OFF
              nuevoModoWiFi(WIFIOFF);
              break;
          }
        }
      }
      else { // the port is different from current state and DEBOUNCE time has elapsed
        boton= lectura;
        if (boton == BTNRELEASED) {  // button released
            if (!btnHeld) { // it was not "held"
              // button pressed!  PRINT!
              if (timeIsValid) {  // (if time is valid)
                if ((!prnIsOnline) || (!pClient->isConnected())) prnIsOnline= connect2prn();
                if (prnIsOnline) {
                  blinkStateG= LEDON;
                  digitalWrite(GPIOgreen, blinkStateG);
                  myStatus= STATPRINTING;
                  formatDT(myDT, myPrn);
                  printBuffer();
                  myStatus= STATONLINE;
                }
                else {
                  myStatus= STATDISCON;
                }
                blinkStateG= LEDOFF;
                digitalWrite(GPIOgreen, blinkStateG);
              }
            }
            else btnHeld= false; // button released after "held"
          }
        }
      }
    }
  // according to WiFi state serve the WEB page
  switch (wifiState) {
    case WIFION:                              // trying to connect to a Router
      if (WiFi.status() == WL_CONNECTED) {    // connected!
        retryWifi= 0;
        wifiState= WIFICON;                   // change to "connected" mode
        // we are now connected, start NTP
        configTime(myDT.gmtOffset, myDT.daylight, myDT.ntpServer);
        setenv("TZ", myDT.timeZone, 0);
        tzset();
        iniciaPagina();
        updateBlueLED(LEDWIFION);
        server.handleClient();
      }
      else if ((savedCfg.retryWifi != 0) && (retryWifi != 0)) { // infinite timeout?
        if ((milliLoop - retryWifi) >= savedCfg.retryWifi) nuevoModoWiFi(WIFIAPM);  // no, we timed out, get back to AP mode
      }
      break;
    case WIFICON:
      if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();  // if we are still connected to the router answer WEB requests
        if ((!timeIsValid) && ((milliLoop - lastTmTry) > EPOCHRETRY)) {  // if time is invalid, retry NTP
          if (!yelIsBlinking) {
            yelIsBlinking= true;
            digitalWrite(GPIOyellow, LEDON);
            blinkerYellow.attach_ms(BLINKINGTIME , blinkeaY);
          }
          lastTmTry= milliLoop;
          now= time(nullptr);
          if (now > NTP_MIN_VALID_EPOCH) {
            timeIsValid= true;
            if (myStatus == STATINVALTIME) updateStatus();
            blinkerYellow.detach();
            digitalWrite(GPIOyellow, LEDON);
            yelIsBlinking= false;
          }
          if (!mDNS) {
            mDNS= MDNS.begin("chronolabeler"); // setup mdns to be able to find us by "http://chronolabeler.local"
//          if (mDNS) MDNS.addService("http","tcp",80);  // optional, add mDNS as a service
          }
        }
      }
      else {
        nuevoModoWiFi(WIFION);  // the router disconnected!, new mode -> retry connection
        timeIsValid= false;
        blinkerYellow.detach();
        digitalWrite(GPIOyellow, LEDOFF);
        yelIsBlinking= false;
      }
      break;
    case WIFIAPM:
      server.handleClient();    // we are in AP mode, answer page requests
      break;
  }
}