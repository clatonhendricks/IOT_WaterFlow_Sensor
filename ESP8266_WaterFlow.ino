#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArducamSSD1306.h>    // Modification of Adafruit_SSD1306 for ESP8266 compatibility
#include <Adafruit_GFX.h>  

/*********** Display variable ***********************************************/

#define OLED_RESET  16  // Pin 15 -RESET digital signal

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

ArducamSSD1306 display(OLED_RESET); // FOR I2C


/************************* WiFi Access Point *********************************/

#define WLAN_SSID "*****"    // your wifi name
#define WLAN_PASS "*****"   // your wifi password

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883   // use 8883 for SSL
#define AIO_USERNAME "*****"  // your io adafruit username    
#define AIO_KEY "*****"       // your io adafruit key

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish waterflow_ml = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/WaterFlow_ml");
Adafruit_MQTT_Publish waterflow_l = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/WaterFlow_liter");
Adafruit_MQTT_Publish waterflow_Gl = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/WaterFlow_GL");
Adafruit_MQTT_Publish waterflow_avg = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/WaterFlowRate");

/*************************** Other variables ************************************/

#define LED_BUILTIN 16
#define SENSOR 2 //D1 = 5  -- D4 = 2 https://www.electronicwings.com/nodemcu/nodemcu-gpio-with-arduino-ide

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
boolean ledState = LOW;
float calibrationFactor = 4.8; //4.5; 35 overcalculate
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
unsigned int totalMilliLitres;
float flowLitres;
float totalLitres;
float totalGallons;
int x = -1;
unsigned long oldTime;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void MQTT_connect();

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT); //INPUT_PULLUP);

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;

  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  /******************** Setup wifi & MQTT access ******************************/

  // Connect to WiFi access point.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop()
{

  MQTT_connect();

  // Reset the variables if there is no water flow for 5 mins
  if ((currentMillis > 500000) && (flowRate == 0))
  {
    //currentMillis = 0;
    //previousMillis = 0;
    totalMilliLitres = 0;
    totalLitres = 0;
    totalGallons = 0;
  }

  currentMillis = millis();
  if (currentMillis - previousMillis > interval)
  {
    detachInterrupt(SENSOR);

    pulse1Sec = pulseCount;

    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();
    Serial.print("Previous Mi ");
    Serial.println(previousMillis);

    Serial.println(currentMillis);
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;
    flowLitres = (flowRate / 60);

    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
    totalLitres += flowLitres;
    totalGallons = (totalLitres / 3.78541178); // conversion to gallons
    //totalFlowRate += flowRate;
    //flowCounter++;
    //digitalWrite(LED_BUILTIN, LOW);
    // Print the flow rate for this second in litres / minute

    Serial.print("Flow rate: ");
    Serial.print(float(flowRate)); // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t"); // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);
    Serial.print("mL / ");
    Serial.print(totalLitres);
    Serial.print("L");
    Serial.print(" / ");
    Serial.print(totalGallons);
    Serial.println("G");

    // Upload to adafruit
    if ((flowRate == 0) && (totalMilliLitres != 0))
    {
      UploadtoAdafruit(totalMilliLitres, totalLitres, totalGallons);
      showDisplay(totalLitres);
    }

    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  }
}

// Method to upload to Adafruit MQTT service
void UploadtoAdafruit(unsigned int ml, float L, float G)
{
  if (x != totalMilliLitres)
  {
    Serial.print("Sending data to Adafruit");
    waterflow_ml.publish(ml);
    waterflow_l.publish(L);
    waterflow_Gl.publish(G);
    x = totalMilliLitres;
  }
}

// Method to display the liters on the screen
void showDisplay(int lt)
{
  
   display.begin();  // Switch OLED
  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(5,0);  
  display.println("Liters");
  display.display();

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(5,30);  
  display.println(lt);
  display.display();

}

// Method to connect to Adafruit MQTT service
void MQTT_connect()
{
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected())
  {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0)
  { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000); // wait 5 seconds
    retries--;
    if (retries == 0)
    {
      // basically die and wait for WDT to reset me
      while (1)
        ;
    }
  }
  Serial.println("MQTT Connected!");
}
