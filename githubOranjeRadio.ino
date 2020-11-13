
// Oranje radio
// Jose Baars, 2019
// public domain

#undef USESSDP
#undef USEOTA
#undef USETLS


//tft
#define TFT_CS     4
#define TFT_RST    14  // you can also connect this to the Arduino reset
#define TFT_DC     13


#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h> 

#include <SPI.h>
#include <VS1053.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "FS.h"
#include "SPIFFS.h"
#include <WiFiClient.h>
#ifdef USETLS
#include <WiFiClientSecure.h>
#endif
#include <WebServer.h>
#ifdef USEOTA
  #include <ArduinoOTA.h>
#endif
//E][WiFiUdp.cpp:219] parsePacket(): could not receive data: 9
//WIFI_STATIC_RX_BUFFER_NUM=10
//CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM 32
//CONFIG_ESP32_WIFI_RX_BA_WIN 6
//...Documents\ArduinoData\packages\esp32\hardware\esp32\1.0.3-rc1\tools\sdk\include\config\\sdkconfig.h
//https://github.com/espressif/esp-idf/issues/3646

#ifdef USESSDP
  #include <SSDPDevice.h>
#else
  #include <ESPmDNS.h>
#endif
#include <Update.h>
#include <Wire.h>
#include "paj7620.h"

#include "sk.h"
#include <wificredentials.h>

#define FORMAT_SPIFFS_IF_FAILED true

WiFiClient        *radioclient;
WiFiClient        iclient;
#ifdef USETLS
WiFiClientSecure  sclient;
#endif
int               contentsize=0;

//hangdetection
#define MAXUNAVAILABLE 50000

int   unavailablecount=0;
int   failed_connects=0;
int   disconnectcount=0;
int   topunavailable=0;

//OTA password
#define APNAME    "OranjeRadio"
#define APVERSION "V0.23"
#define APPAS     "oranjeboven"

SemaphoreHandle_t staSemaphore;
SemaphoreHandle_t volSemaphore;
SemaphoreHandle_t tftSemaphore;
SemaphoreHandle_t updateSemaphore;
SemaphoreHandle_t scrollSemaphore;
SemaphoreHandle_t chooseSemaphore;

TaskHandle_t      gestureTask;   
TaskHandle_t      pixelTask;   
TaskHandle_t      webserverTask;  
TaskHandle_t      radioTask;
TaskHandle_t      playTask;
TaskHandle_t      scrollTask;

#define WEBCORE     0
#define RADIOCORE   1
#define GESTURECORE 1
#define PLAYCORE    1


#define PIXELTASKPRIO     3
#define GESTURETASKPRIO   7
#define RADIOTASKPRIO     6
#define PLAYTASKPRIO      5
#define WEBSERVERTASKPRIO 7
#define SCROLLTASKPRIO    4

QueueHandle_t playQueue;
#define PLAYQUEUESIZE 512

// stations

typedef struct {
char *name;
int  protocol;
char *host;
char *path;
int   port;
int   status;
unsigned int   position;
}Station;


#define STATIONSSIZE 100
Station *stations; //= (Station *) ps_malloc( STATIONSSIZE * sizeof(Station *) );

static volatile int     currentStation;
static volatile int     stationCount;
int                     playingStation = -1;
int                     chosenStation = 0;
int                     scrollStation = -1;
int                     scrollDirection;


// neopixel 
#define NEOPIN     32
#define NEONUMBER  10

sk gstrip;

//gesture sensor

#define GSDAPIN 27
#define GSCLPIN 26
#define GINTPIN 25


int gmode;
//tft
#define SCROLLUP 0
#define SCROLLDOWN 1
// webserver
WebServer server(80);

//battery
float   batvolt = 0.0;
#define BATPIN     36

//vs1053
#define VS1053_CS     5
#define VS1053_DCS    15
#define VS1053_DREQ   22
#define VS1053_RST    21

// Default volume
int currentVolume=65; 

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);


bool stationChunked = false;
bool stationClose   = false;

//pixels and gestures



/*------------------------------------------------------*/

#ifdef USEOTA
void initOTA( char *apname, char *appass){
  

 // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(apname);

  // No authentication by default
  ArduinoOTA.setPassword( appass );

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;

    xSemaphoreTake( updateSemaphore, portMAX_DELAY);
    
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "Firmware";
      syslog("Installing new firmware over ArduinoOTA");
    } else { // U_SPIFFS
      type = "filesystem";
      SPIFFS.end();
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);

      tft_ShowUpload( type );

  });
  ArduinoOTA.onEnd([]() {
   xSemaphoreGive( updateSemaphore); 
   tft_uploadEnd( "success");
   Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    tft_uploadProgress( (progress / (total / 100))  );

  });
  ArduinoOTA.onError([](ota_error_t error) {
    //tft_uploadEnd("failed");

    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      tft_uploadEnd("Auth failed");
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      tft_uploadEnd("Begin failed");
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      tft_uploadEnd("Connect failed");
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      tft_uploadEnd("Receive failed");  
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      tft_uploadEnd("End failed");
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

#endif

/*-----------------------------------------------------------*/

void getWiFi( char *apname, char *appass){
  
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

   // Workaround for esp32 failing to set hostname as found here: https://github.com/espressif/arduino-esp32/issues/2537#issuecomment-508558849
   // for some reason it does not work here,although it does in th Basic example
   
   Serial.printf("1-config 0 and set hostname to %s\n", apname);
 
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname( apname );
    
    wm.setHostname( apname );// This only partially works in setting the mDNS hostname 
    
    
    wm.setAPCallback( tft_NoConnect );
    wm.setConnectTimeout(20);
    
    //reset settings - wipe credentials for testing
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( apname ),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    res = wm.autoConnect( apname, appass ); // anonymous ap

    if(!res) {
        Serial.println("Failed to connect");
        ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("Wifi CONNECTED");

         ntp_setup( true );
    }
  
  Serial.print("IP address = ");
  Serial.println(WiFi.localIP());
#ifdef USEOTA
  initOTA( apname, appass );
#endif

}

/*----------------------------------------------------------*/


void setup () {

    Serial.begin(115200);
    Serial.printf("\n%s %s  %s %s\n", APNAME, APVERSION, __DATE__, __TIME__);   
    
    // Enable WDT
   
    enableCore0WDT(); 
    enableCore1WDT();

     TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE;
     TIMERG0.wdt_feed=1;
     TIMERG0.wdt_wprotect=0;
 
     Serial.println("SPI begin...");
     SPI.begin(); 
     
    //unreset the VS1053
    pinMode( VS1053_RST , OUTPUT);

    digitalWrite( VS1053_RST , LOW);
    delay( 200);
    digitalWrite( VS1053_RST , HIGH);

     Serial.println("Creating semaphores...");
    
     staSemaphore = xSemaphoreCreateMutex();
     volSemaphore = xSemaphoreCreateMutex();
     tftSemaphore = xSemaphoreCreateMutex();
     updateSemaphore = xSemaphoreCreateMutex();
     scrollSemaphore = xSemaphoreCreateMutex();
     chooseSemaphore = xSemaphoreCreateMutex();
      
     Serial.println("Take sta semaphore...");
     xSemaphoreTake(staSemaphore, 10);
     xSemaphoreGive(staSemaphore);
     
     Serial.println("Take vol semaphore...");
     xSemaphoreTake(volSemaphore, 10);
     xSemaphoreGive(volSemaphore);

     Serial.println("Take tft semaphore...");
     xSemaphoreTake(tftSemaphore, 10);
     xSemaphoreGive(tftSemaphore);

     Serial.println("Take update semaphore...");
     xSemaphoreTake(updateSemaphore, 10);
     xSemaphoreGive(updateSemaphore);
    
     Serial.println("Create playQueue...");
     playQueue = xQueueCreate( PLAYQUEUESIZE, 32);
      
     Serial.println("Start File System...");
     setupFS();   
       
     Serial.println("Gesture init");
     
     if ( gesture_init() ) Serial.println ( "FAILED to init gesture control");
     delay(200);
     
     Serial.println("point radioclient to insecure WiFiclient");
     radioclient = &iclient;

     Serial.println("Getstations...");
     stationsInit();

    Serial.println("Start WiFi en web...");
          
     getWiFi(APNAME,APPAS);
  
        
     Serial.println("log boot");    
     syslog("Boot"); 
     
    // Wait for VS1053 and PAM8403 to power up
    // otherwise the system might not start up correctly
    //delay(3000);
    // already a delay in tft_init
    
    // This can be set in the IDE no need for ext library
    // system_update_cpu_freq(160);
    



Serial.println("player begin...");

    player.begin();

    Serial.println("Test VS1053 chip...");
    while(1){ 
      bool   isconnected = player.isChipConnected();
      Serial.printf( " Chip connected ? %s\n", isconnected?"true":"false");
      if ( isconnected ) break;
      delay(10);
    }
      
    Serial.println("not Switch to MP3...");
    //player.switchToMp3Mode();

    delay(100);
    Serial.println("Apply patches to VS1053...");
    patchVS1053();

    Serial.println("Set volume and station...");

  Serial.println("TFT init...");
 tft_init();
 
 delay(50); 
 Serial.println("Start WebServer...");
  setupWebServer();
 
 Serial.println("Start play task...");  
    play_init();
 Serial.println("Start radio task...");    
    radio_init();
 Serial.println("setup done...");    

}


void loop(void){

   vTaskDelete( NULL );
   //ArduinoOTA.handle();    
   //server.handleClient();
   //SSDPDevice.handleClient(); 
   delay(100);

}
