


// Please select the corresponding model

// #define SIM800L_IP5306_VERSION_20190610
// #define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
 #define SIM800L_IP5306_VERSION_20200811

// #define TEST_RING_RI_PIN            //Note will cancel the phone call test

// #define ENABLE_SPI_SDCARD   //Uncomment will test external SD card
// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS
#define SMS_TARGET "+447973177615"
#define BATT_PIN 34
#include <ESP32AnalogRead.h>
ESP32AnalogRead adc;


// Define the serial console for debug prints, if needed
//#define TINY_GSM_DEBUG SerialMon

#include "utilities.h"

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
//Set serial for AT commands (to the module)
#define SerialAT  Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800          // Modem is SIM800
#define TINY_GSM_RX_BUFFER      1024   // Set RX buffer to 1Kb

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60*60*1        /* Time ESP32 will go to sleep (in seconds) */


// Server details
const char server[] = "demo.minus1.com";
const char resource[] = "/box/";

// Your GPRS credentials (leave empty, if missing)
const char apn[]      = ""; // Your APN
const char gprsUser[] = ""; // User
const char gprsPass[] = ""; // Password
const char simPIN[]   = ""; // SIM card PIN code, if any

// MQTT details
const char* broker = "217.155.27.30";

const char* topicStatus      = "box/battery";
//const char* topicInit      = "GsmClientTest/init";
//const char* topicLedStatus = "GsmClientTest/ledStatus";

#include <TinyGsmClient.h>
#include <PubSubClient.h>
TinyGsmClient client(modem);
PubSubClient mqtt(client);

//const int  port = 1880;

/*
This is just to demonstrate how to use SPI device externally.
Here we use SD card as a demonstration. In order to maintain versatility,
I chose three boards with free pins as SPI pins
*/
#ifdef ENABLE_SPI_SDCARD

#include "FS.h"
#include "SD.h"
#include <SPI.h>

SPIClass SPI1(HSPI);

#define MY_CS       33
#define MY_SCLK     25
#define MY_MISO     27
#define MY_MOSI     26

void setupSDCard()
{
    SPI1.begin(MY_SCLK, MY_MISO, MY_MOSI, MY_CS);
    //Assuming use of SPI SD card
    if (!SD.begin(MY_CS, SPI1)) {
        Serial.println("Card Mount Failed");
    } else {
        Serial.println("SDCard Mount PASS");
        String size = String((uint32_t)(SD.cardSize() / 1024 / 1024)) + "MB";
        Serial.println(size);
    }
}  
#else
#define setupSDCard()
#endif


void setupModem()
{
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LED_OFF);
}

void turnOffNetlight()
{
    SerialMon.println("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    SerialMon.println("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();
//
//    // Only proceed if incoming message's topic matches
//    if (String(topic) == topicLed) {
//        ledStatus = !ledStatus;
//        digitalWrite(LED_GPIO, ledStatus);
//        mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
//    }
}

boolean mqttConnect(float lipoV, float batteryV)
{
    SerialMon.print("Connecting to ");
    SerialMon.println(broker);

    // Connect to MQTT Broker
    //boolean status = mqtt.connect("BoxMQTT");

    // Or, if you want to authenticate MQTT:
    mqtt.setServer("217.155.27.30", 1883);
    boolean status = mqtt.connect("BoxMQTT", "mike", "ea23zz55");

    if (status == false) {
        SerialMon.println("MQTT connect fail");
        return false;
    }
    SerialMon.println(" success");
    char payload[80];
    sprintf(payload, "lipoV: %f\nbatteryV: %f", lipoV, batteryV);
    
    mqtt.publish(topicStatus, payload);
   // mqtt.subscribe(topicLed);
    return mqtt.connected();
}



void setup()
{
    // Set console baud rate
    SerialMon.begin(115200);

    delay(10);

    // Start power management
    if (setupPMU() == false) {
        Serial.println("Setting power error");
    }

    setupSDCard();

    // Some start operations
    setupModem();

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    adc.attach(34);


}

void loop()
{
    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();

    // Turn off network status lights to reduce current consumption
    turnOffNetlight();

    // The status light cannot be turned off, only physically removed
    //turnOffStatuslight();

    // Or, use modem.init() if you don't need the complete restart
    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem: ");
    SerialMon.println(modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
        modem.simUnlock(simPIN);
    }

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork(240000L)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" OK");

    // When the network connection is successful, turn on the indicator
    digitalWrite(LED_GPIO, LED_ON);

    if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
    }

    SerialMon.print(F("Connecting to APN: "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" OK");

//    SerialMon.print("Connecting to ");
//    SerialMon.println(server);
//    if (!client.connect(server, port)) {
//        SerialMon.println(" CCC fail");
//        delay(10000);
//        return;
//    }
//    SerialMon.println(" OK");

            uint8_t  chargeState = -99;
  int8_t   percent     = -99;
  uint16_t milliVolts  = -9999;

  modem.getBattStats(chargeState, percent, milliVolts);
  DBG("Battery charge state:", chargeState);
  DBG("Battery charge 'percent':", percent);
  DBG("Battery voltage:", milliVolts / 1000.0F);

    float lipoV = milliVolts / 1000.0F;

    Serial.print("BATT: ");
    Serial.println(adc.readVoltage());

        // Set 11db attenuation on BATT Pin
   analogSetPinAttenuation(BATT_PIN, ADC_11db);

  float batteryV =adc.readVoltage();



    // Make a HTTP GET request:
//    SerialMon.println("Performing HTTP GET request...");
//    client.print(String("GET ") + resource + String(lipoV) + String("/") + String(temp) +  " HTTP/1.1\r\n");
//    client.print(String("Host: ") + server + "\r\n");
//    client.print("Connection: close\r\n\r\n");
//    client.println();
//
//    unsigned long timeout = millis();
//    while (client.connected() && millis() - timeout < 2000L) {
//        // Print available data
//        while (client.available()) {
//            char c = client.read();
//            SerialMon.print(c);
//            timeout = millis();
//        }
//    }

    mqttConnect(lipoV, batteryV);
    SerialMon.println();


  bool res;

  // bool  res = modem.sendSMS(SMS_TARGET, String("LipoV: \n" + String(milliVolts/1000.0F) + String(" LBattV: " ) +String(temp)));
//    DBG("SMS:", res ? "OK" : "fail");


    // Shutdown
    client.stop();
    SerialMon.println(F("Server disconnected"));

    modem.gprsDisconnect();
    SerialMon.println(F("GPRS disconnected"));


    // DTR is used to wake up the sleeping Modem
    // DTR is used to wake up the sleeping Modem
    // DTR is used to wake up the sleeping Modem
#ifdef MODEM_DTR

    modem.sleepEnable();

    delay(100);

    // test modem response , res == 0 , modem is sleep
    res = modem.testAT();
    Serial.print("SIM800 Test AT result -> ");
    Serial.println(res);

    delay(1000);

    Serial.println("Use DTR Pin Wakeup");
    pinMode(MODEM_DTR, OUTPUT);
    //Set DTR Pin low , wakeup modem .
    digitalWrite(MODEM_DTR, LOW);


    // test modem response , res == 1 , modem is wakeup
    res = modem.testAT();
    Serial.print("SIM800 Test AT result -> ");
    Serial.println(res);

#endif


#ifdef TEST_RING_RI_PIN
#ifdef MODEM_RI
    // Swap the audio channels
    SerialAT.print("AT+CHFA=1\r\n");
    delay(2);

    //Set ringer sound level
    SerialAT.print("AT+CRSL=100\r\n");
    delay(2);

    //Set loud speaker volume level
    SerialAT.print("AT+CLVL=100\r\n");
    delay(2);

    // Calling line identification presentation
    SerialAT.print("AT+CLIP=1\r\n");
    delay(2);

    //Set RI Pin input
    pinMode(MODEM_RI, INPUT);

    Serial.println("Wait for call in");
    //When is no calling ,RI pin is high level
    while (digitalRead(MODEM_RI)) {
        Serial.print('.');
        delay(500);
    }
    Serial.println("call in ");

    //Wait 10 seconds for the bell to ring
    delay(10000);

    //Accept call
    SerialAT.println("ATA");


    delay(10000);

    // Wait ten seconds, then hang up the call
    SerialAT.println("ATH");
#endif  //MODEM_RI
#endif  //TEST_RING_RI_PIN

    // Make the LED blink three times before going to sleep
    int i = 3;
    while (i--) {
        digitalWrite(LED_GPIO, LED_ON);
        modem.sendAT("+SPWM=0,1000,80");
        delay(500);
        digitalWrite(LED_GPIO, LED_OFF);
        modem.sendAT("+SPWM=0,1000,0");
        delay(500);
    }

    //After all off
    modem.poweroff();

    SerialMon.println(F("Poweroff"));

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

    esp_deep_sleep_start();

    /*
    The sleep current using AXP192 power management is about 500uA,
    and the IP5306 consumes about 1mA
    */
}
