#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "WiFiDetails.h"
#include <WiFiClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// GPS setup vars
static const int RXPin = 14, TXPin = 12;
static const uint32_t GPSBaud = 9600;
static const int MAX_SATELLITES = 40;
static const int PAGE_LENGTH = 40;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

// The TinyGPS++ object
TinyGPSPlus gps;
// TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
// TinyGPSCustom messageNumber(gps, "GPGSV", 2);      // $GPGSV sentence, second element
// TinyGPSCustom satNumber[4];                        // to be initialized later
// TinyGPSCustom elevation[4];
// bool anyChanges = false;
// unsigned long linecount = 0;

//OLED setup vars
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int screenState = 0;

//SSID and password to autoconnect on start
const char *ssid = "Bellapais";
const char *password = "KoubaCat37";

//IFTTT server api posting vars
const char *IFTTTServerName = "http://maker.ifttt.com/trigger/is_raining/with/key/cy5u-CnAtPkHkXwE71ZUFR";
unsigned long lastRequest = -60000;

// Vars for network speed
String sssid;
uint8_t encryptionType;
int32_t RSSI;
uint8_t *BSSID;
int32_t channel;
bool isHidden;
uint8_t curBss;
uint8_t prevRssi;
int rssi = 0;
int networkIndex;

// Pin number for rain sesnor
const int ADC = A0;

// Pin number for button
const int button = 2;

// Weather vars
float h;
float t;
float f;
double dew;
int rainSensor = 0;
int rainPercentage = 0;

#define DHTPIN 0      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);

// Function declarations

double dewPoint(double celsius, double humidity)
{
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO))) - 1);
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1);
  RHS += log10(1013.246);

  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP / 0.61078); // temp var
  return (241.88 * T) / (17.558 - T);
}

ICACHE_RAM_ATTR void wifiSignalDisplay(){

  display.setCursor(0, 0);

  if (WiFi.status() == WL_CONNECTED)
  {

    int netnum = WiFi.scanNetworks();

    for (int i = 0; i < netnum; i++)
    {
      if (WiFi.SSID(i) == ssid)
      {
        networkIndex = i;
        rssi = WiFi.RSSI(i);
      }
    }

    // Serial.println(WiFi.status());

    // WiFi.getNetworkInfo(networkIndex, sssid, encryptionType, RSSI, BSSID, channel, isHidden);

    // rssi = RSSI;

    // rssi = WiFi.RSSI(networkIndex);

    //   int netnum = WiFi.scanNetworks();

    // for (int i = 0; i < netnum; i++)
    // {
    //   if (WiFi.SSID(i) == ssid)
    //   {
    //     rssi = WiFi.RSSI(i);
    //   }

    // }

    // WiFi.getNetworkInfo(netnum, sssid, encryptionType, RSSI, BSSID, channel, isHidden);

    int bars;
    //  int bars = map(RSSI,-80,-44,1,6); // this method doesn't refelct the Bars well
    // simple if then to set the number of bars

    if (rssi >= -66)
    {
      bars = 5;
    }
    else if ((rssi < -66) & (rssi > -86))
    {
      bars = 4;
    }
    else if ((rssi < -87) & (rssi > -92))
    {
      bars = 3;
    }
    else if ((rssi < -93) & (rssi > -101))
    {
      bars = 2;
    }
    else if ((rssi < -102) & (rssi > -110))
    {
      bars = 1;
    }
    else
    {
      bars = 0;
    }

    for (int b = 0; b <= bars; b++)
    {
      display.fillRect(0 + (b * 5), 16 - (b * 3), 3, b * 3, WHITE);
    }
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.print("X");
    display.setTextSize(1);
  }
}

ICACHE_RAM_ATTR void rainCheck(int rainPercent){
  
  if (rainPercent >= 50)
  {
    // Make sure it was at least 1 min since last request (API flood prevention)
    if (millis() - lastRequest >= 60000)
    {
      
      // Needs to have wifi client in latest versions to POST
      WiFiClient client;
      HTTPClient http;

      // Unique URL for sending POST
      http.begin(client, IFTTTServerName);
      // Add content header (JSON)
      http.addHeader("Content-Type", "application/json");
      // Data to send with POST
      String httpRequestData = "{\"value1\":\"" + String(rainPercent) + "\",\"value2\":\"" + String(t) + "\",\"value3\":\"" + String(dew) + "\"}";
      // Send HTTP POST req.
      int httpResponseCode = http.POST(httpRequestData);

      // Print result to Serial
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      // Set last request to current uptime to ensure it's not flooding the API
      lastRequest = millis();

      display.clearDisplay();
      display.setCursor(20, 0);
      display.setTextSize(2);
      display.println("Raining!");
      display.setTextSize(1);
      display.setCursor(30, (SCREEN_HEIGHT / 2) - 1);
      display.print("Sending alert");
      display.display();
      delay(500);
      display.invertDisplay(true);
      delay(500);
      display.invertDisplay(false);
      delay(500);
      display.invertDisplay(true);
      delay(500);
      display.invertDisplay(false);
      delay(500);

      if (httpResponseCode==200)
      {
        display.clearDisplay();
        display.setCursor(30, (SCREEN_HEIGHT / 2) - 1);
        display.print("Sending Complete");
        display.display();
      }else
      {
        display.clearDisplay();
        display.setCursor(30, (SCREEN_HEIGHT / 2) - 1);
        display.print("Error: ");
        display.print(httpResponseCode);
        display.display();
      }

      // Delay to show success/error code on screen
      delay(1000);

      // Free up resources
      http.end();
    }
  }
}

ICACHE_RAM_ATTR void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

ICACHE_RAM_ATTR void drawDisplay()
{

  display.clearDisplay();
  wifiSignalDisplay();

  if (screenState == 0)
  {
    // TODO: Change this for GPS signal bar
    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text
    display.setCursor(50, 0);
    display.print(ssid);
    display.setCursor(50, 8);
    display.print("Signal: ");
    display.print(rssi);
    display.setCursor(0, 24); // Start at top-left corner of blue +1 line

    display.print(F("Humidity: "));
    display.print(h);
    display.println("%");
    display.print(F("Temperature: "));
    display.print(t);
    display.println(char(247));
    display.print("Dew Point: ");
    display.print(dew);
    display.println(char(247));
    display.println();
    display.print("Rain Level: ");
    display.print(rainPercentage);
    display.print(" %");
  }
  else if (screenState == 1)
  {
    display.setCursor(0, 24); // Start at top-left corner of blue +1 line

    if (gps.time.isValid())
    {
      display.print(gps.date.day());
      display.print("/");
      display.print(gps.date.month());
      display.print("/");
      display.println(gps.date.year());
    }
    else
    {
      display.println("");
    }

    display.print(F("Location: "));
    if (gps.location.isValid())
    {
      display.print(gps.location.lat(), 6);
      display.println(F(","));
      display.println(gps.location.lng(), 6);
    }
    else
    {
      display.println(F("Finding..."));
    }

    display.print("Altitude: ");

    if (gps.altitude.isValid())
    {
      display.print(gps.altitude.meters());
      display.println(" m");
    }
    else
    {
      display.println("Finding...");
    }

    display.print("Satellites fixed: ");

    if (gps.satellites.isValid())
    {
      display.println(gps.satellites.value());
    }
    else
    {
      display.println("N/A");
    }
  }

  display.display();
}

ICACHE_RAM_ATTR void buttonPressed(){

  if (screenState == 0)
  {
    screenState = 1;
  }
  else
  {
    screenState = 0;
  }
  //  drawDisplay();
}

void calculateHourAngle(){

  if (gps.time.isValid() && gps.date.isValid()){

  

  // double siderealTime;

  int year = gps.date.year();
  int month = gps.date.month();
  double day = gps.date.day();

  if (month == 1 || month == 2)
  {
    year = year -1;
    month = month +12;
  }

  int A = year / 100;
  int B = 2 - A + (int)(A / 4);

  double julianDay = (int)(365.25 * (year + 4716)) + (int)(30.6001 * (month + 1)) + day + B - 1524.5;
  Serial.print("Julian Day: ");
  Serial.println(julianDay);

  double julianCentuary = ((julianDay - 2451545.0) / 36525);
  Serial.print("Julian Centuary: ");
  Serial.println(julianCentuary);

  double theta0 = 280.46061837 + 360.98564736629 * (julianDay - 2451545.0) + (0.000387933 * julianCentuary * julianCentuary) - (julianCentuary * julianCentuary * julianCentuary / 38710000.0); // (12.4)
  
  //TODO: Move this function above. Write code that's in the example struct to give RA (HMS conversion)
  RightAscension(ReduceAngle(theta0));


  }
  

//τ = θ - α
// τ= hour angle
// θ= sideral time
//α= polaris RA (~ 2h32m)
}

double ReduceAngle(double d)
        {
            d = fmod(d,360);
            if (d < 0)
            {
                d += 360;
            }

            return d;
        }


double RightAscension(){

  return 1.1;
}

void setup()
{

  pinMode(ADC, INPUT);
  pinMode(button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(button), buttonPressed, FALLING);

  Serial.begin(115200);

  // GPS testing

  ss.begin(GPSBaud);

  // Initialize all the uninitialized TinyGPSCustom objects
  // for (int i = 0; i < 4; ++i)
  // {
  //   satNumber[i].begin(gps, "GPGSV", 4 + 4 * i); // offsets 4, 8, 12, 16
  //   elevation[i].begin(gps, "GPGSV", 5 + 4 * i); // offsets 5, 9, 13, 17
  // }

  // Serial.println('\n');

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner

  WiFi.begin(ssid, password); // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  display.println("Connecting to ");
  display.print(ssid);
  display.println(" ...");
  display.display();

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
    display.print(i);
    display.print(' ');
    display.display();
  }

  Serial.println('\n');
  Serial.println("Connected!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer

  display.clearDisplay();
  display.setCursor(0, 0); // Start at top-left corner

  // display.println('\n');
  display.println("Connected!");
  display.println("IP address: ");
  display.println(WiFi.localIP());
  display.display();

  if (WiFi.status() == WL_CONNECTED)
  {

    WiFi.setAutoReconnect(true);

    int netnum = WiFi.scanNetworks();

    for (int i = 0; i < netnum; i++)
    {
      if (WiFi.SSID(i) == ssid)
      {
        networkIndex = i;
        rssi = WiFi.RSSI(i);
      }
    }
  }

  dht.begin();
}

void loop()
{

// Check GPS sensor for new location
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
  }


  rainSensor = analogRead(ADC);
  rainPercentage = map((1024 + (rainSensor * -1)), 0, 1024, 0, 100);

  rainCheck(rainPercentage);

  // Read Humidity
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  dew = dewPoint(t, h);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  drawDisplay();

  calculateHourAngle();

  
}


//contrast adjustment
  //   for (int dim=150; dim>=0; dim-=10) {
  //   display.ssd1306_command(0x81);
  //   display.ssd1306_command(dim); //max 157
  //   delay(50);
  //   }

  // for (int dim2=34; dim2>=0; dim2-=17) {
  // display.ssd1306_command(0xD9);
  // display.ssd1306_command(dim2);  //max 34
  // delay(100);
  // }

  //   for (int dim=0; dim<=160; dim+=10) {
  //   display.ssd1306_command(0x81);
  //   display.ssd1306_command(dim); //max 160
  //   delay(50);
  // }