#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>
#include <HTTPClient.h> 
#include <time.h> 
#include <groq_stt.h> // Integrated Groq Speech-to-Text Engine
#include <math.h>
#include <qrcode.h> // Built directly into the ESP32 Arduino core

// Default Fallback Wi-Fi Credentials (used if no credentials saved in NVS yet)
const char* DEFAULT_SSID     = "ssid";
const char* DEFAULT_PASSWORD = "password";

// Access Point (AP) Mode Credentials for Wi-Fi Setup
const char* AP_SSID     = "Elecsy-Setup";
const char* AP_PASSWORD = ""; // Leave empty ("") for open Wi-Fi, or min 8 chars e.g. "12345678"

// Active STA Credentials loaded from Preferences
String savedSSID     = "";
String savedPassword = "";

// Captive Portal & Storage Objects
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// API Configurations
const char* apiKey       = "Gemini-api-key"; 
const char* GROQ_API_KEY = "groq-api-key";

// Hardware Pins for ESP32-C3 Super Mini
#define TOUCH_PIN       3   // GPIO 3 for TTP223 Touch Sensor
#define I2C_SDA         8   // Default SDA pin on ESP32-C3 Super Mini
#define I2C_SCL         9   // Default SCL pin on ESP32-C3 Super Mini

// INMP441 Microphone I2S Pin Mapping
const int PIN_MIC_SCK = 7;     
const int PIN_MIC_WS  = 5;     
const int PIN_MIC_SD  = 6;     

#define SCREEN_WIDTH  128 
#define SCREEN_HEIGHT  64 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 


const char* UPI_URI = "upi://pay?pa=<upi-id>&pn=<upi-name>&cu=INR";

// Open-Meteo Free API for New Delhi (Lat: 28.6139, Lon: 77.2090) - No API Key Needed
const char* WEATHER_URL = "http://api.open-meteo.com/v1/forecast?latitude=28.6139&longitude=77.2090&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=Asia%2FKolkata";

// Weather Data Storage
float temperature  = 0.0;
int humidity       = 0;
float windSpeed    = 0.0;
int weatherCode    = 0;
bool hasValidData  = false;

unsigned long lastUpdateMs = 0;
const unsigned long UPDATE_INTERVAL_MS = 600000; // Refresh every 10 minutes

// Driver Instantiations
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
GroqSTT stt(PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_SD, TOUCH_PIN);

// Robot Face Layout Coordinates
const int faceX = 14, faceY = 2, faceW = 100, faceH = 60, faceRadius = 12;
const int leftEyeX = 38, rightEyeX = 90, eyeY = 26, eyeRadius = 12, pupilRadius = 4;  
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 3000; 

// Animation variables
int thinkingOffset = 0;
bool thinkingDirection = true;
int wifiAnimFrame = 0;

// Double Tap Sensor variables
bool lastTouchState = LOW;
unsigned long lastTouchTime = 0;
int touchCount = 0;
const unsigned long doubleTapTimeout = 400;

// NTP Time Configuration for IST
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST is UTC +5:30
const int   daylightOffset_sec = 0; 

// Timer Variables
int setMinutes = 1;
unsigned long totalDurationMs = 0;
unsigned long remainingMs     = 0;
unsigned long lastTickMs      = 0;
bool times_up = true;

enum TimerState { READY, RUNNING, PAUSED, FINISHED };
TimerState state = READY;

// Touch Button Handling for Timer
bool lastTouchState1        = false;
unsigned long touchStartMs = 0;
bool longPressHandled      = false;

// Bitmaps (Instagram & YouTube)
const unsigned char insta_img[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3c, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x0f, 0xf0, 0x3e, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x3f, 0xfc, 0x3c, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x7f, 0xfe, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0xff, 0xff, 0x80, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0xf8, 0x1f, 0x80, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x03, 0xe0, 0x07, 0xc0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0xc0, 0x03, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0xc0, 0x03, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0x80, 0x01, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0f, 0x80, 0x01, 0xf0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0f, 0x80, 0x01, 0xf0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0f, 0x00, 0x00, 0xf0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0f, 0x80, 0x01, 0xf0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0f, 0x80, 0x01, 0xf0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0x80, 0x01, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0xc0, 0x03, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x07, 0xc0, 0x03, 0xe0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x03, 0xe0, 0x0f, 0xc0, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0xf8, 0x1f, 0x80, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0xff, 0xff, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x7f, 0xfe, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x3f, 0xfc, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x0f, 0xf0, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0x0f, 0x38, 0xe4, 0xd2, 0x7b, 0xc7, 0x22, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x61, 0x08, 0x6d, 0xb4, 0xb2, 0xc3, 0x6c, 0x92, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x61, 0x08, 0x41, 0x83, 0xba, 0xc2, 0x68, 0x1c, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0x0f, 0x40, 0xf3, 0x3e, 0xfb, 0xc9, 0xcc, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x41, 0x08, 0x65, 0x13, 0x36, 0xc2, 0x4c, 0xc8, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0xef, 0x3d, 0xf3, 0x36, 0xfa, 0x67, 0x88, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char youtube_img[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x1f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x1f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0x0f, 0x38, 0xe4, 0xd2, 0x7b, 0xc7, 0xb2, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x41, 0x08, 0x65, 0xb4, 0xb2, 0x42, 0x6c, 0x96, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0x0e, 0x41, 0xc3, 0x3a, 0x73, 0x68, 0x1c, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0x0e, 0x40, 0x73, 0x3e, 0x73, 0xc9, 0xcc, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x41, 0x08, 0x65, 0x13, 0x36, 0x42, 0x4c, 0xc8, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x79, 0xef, 0x3c, 0xe3, 0x32, 0x7a, 0x27, 0x88, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Prototypes
void startWiFiPortal();
void handlePortalRoot();
void handlePortalSave();
void drawAPSetupScreen(String ipAddr);

void drawFaceExpression(String type, int pupilOffsetX, int pupilOffsetY);
void drawThinkingFrame();
void drawListeningFrame();
void displayResponse(String text);
void displayISTTime();
void drawWifiConnectingFrame();
void drawWifiConnectedFrame();
String fetchGeminiAI(String prompt);
void handleTouch();
void updateTimer();
void drawCountdownUI();
void drawProgressRing(int cx, int cy, int rOuter, int rInner, float progress);

bool fetchDelhiWeather();
void drawWeatherScreen();
void drawWeatherIcon(int x, int y, int code);
const char* getWeatherDescription(int code);

// Callback automatically invoked by ESP32's built-in QR engine
void renderQrToOled(esp_qrcode_handle_t qrcode) {
  int size  = esp_qrcode_get_size(qrcode);
  int scale = (SCREEN_HEIGHT - 4) / size;
  if (scale < 1) scale = 1;

  int qrPixelSize = size * scale;
  int quietZone   = 3;
  int boxSize     = qrPixelSize + (quietZone * 2);
  int boxX        = 2;
  int boxY        = ((SCREEN_HEIGHT - boxSize)-8) / 2;
  int qrStartX    = boxX + quietZone;
  int qrStartY    = boxY + quietZone;

  display.clearDisplay();
  display.fillRect(boxX, boxY, boxSize, boxSize, SSD1306_WHITE);

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        display.fillRect(
          qrStartX + (x * scale),
          qrStartY + (y * scale),
          scale,
          scale,
          SSD1306_BLACK
        );
      }
    }
  }

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(70, 10); display.print("SCAN TO");
  display.setCursor(70, 22); display.print("PAY UPI");
  display.drawLine(70, 34, 124, 34, SSD1306_WHITE);
  display.setCursor(67, 40); display.print("GPay/Paytm");
  display.display();
}

void showStablePaymentQR(const char* upiPayload) {
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func       = renderQrToOled;
  cfg.max_qrcode_version = 4;
  cfg.qrcode_ecc_level   = ESP_QRCODE_ECC_LOW;
  esp_qrcode_generate(&cfg, upiPayload);
}

void setOledCameraSafeContrast(uint8_t contrast) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void showImage(const uint8_t* bitmap, int w, int h) {
  display.clearDisplay();
  int posX = (SCREEN_WIDTH - w) / 2;
  int posY = (SCREEN_HEIGHT - h) / 2;
  display.drawBitmap(posX, posY, bitmap, w, h, SSD1306_WHITE);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.print("\nBooting...");
  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(I2C_SDA, I2C_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED initialization failed"));
    for(;;); 
  }
  
  display.setRotation(2); // Rotate the display 180 degrees

  // 1. Load saved Wi-Fi credentials from NVS Flash (or use defaults)
  preferences.begin("wifi-cfg", true);
  savedSSID     = preferences.getString("ssid", DEFAULT_SSID);
  savedPassword = preferences.getString("pass", DEFAULT_PASSWORD);
  preferences.end();

  Serial.print("\nAttempting STA connection to: ");
  Serial.println(savedSSID);

  // 2. Try connecting in STA Mode for up to 15 seconds
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  unsigned long wifiStartMs = millis();
  const unsigned long wifiTimeoutMs = 15000; // 15 seconds timeout

  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartMs < wifiTimeoutMs)) {
    drawWifiConnectingFrame();
    delay(100);
  }

  // 3. If STA connection failed, switch to AP Mode + Captive Portal Web Server
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] STA Connection Failed! Entering AP Setup Mode...");
    startWiFiPortal(); // Blocks and hosts web UI until new credentials are saved & ESP reboots
  }

  // 4. Connected in STA Mode -> Continue Normal Boot
  drawWifiConnectedFrame();
  delay(1500);
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("\nWiFi Connected & NTP Clock Synced!");

  stt.setModel("whisper-large-v3-turbo");
  stt.setLanguage("en");
  stt.setGain(10);              
  stt.setHighpassHz(120);       
  stt.setSilenceThreshold(300);

  if (!stt.begin(GROQ_API_KEY)) {
    Serial.println("[STT INIT FAIL]");
    display.clearDisplay();
    display.setCursor(10, 24);
    display.print("STT Init Error");
    display.display();
    while (true) delay(1000);
  }
  Serial.println("[STT INIT OK] Audio peripheral listening...");
}

// =========================================================================
// AP MODE & CAPTIVE PORTAL WEB SERVER
// =========================================================================
void startWiFiPortal() {
  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_AP_STA); // AP_STA allows scanning networks while hosting AP

  if (strlen(AP_PASSWORD) >= 8) {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  } else {
    WiFi.softAP(AP_SSID); // Open network if password is empty
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("[AP MODE] Hotspot: "); Serial.println(AP_SSID);
  Serial.print("[AP MODE] Portal IP: "); Serial.println(apIP);

  drawAPSetupScreen(apIP.toString());

  // Redirect all DNS requests to ESP32 AP IP (triggers phone Captive Portal popup)
  dnsServer.start(DNS_PORT, "*", apIP);

  // Web Server Routes
  server.on("/", HTTP_GET, handlePortalRoot);
  server.on("/save", HTTP_POST, handlePortalSave);
  server.onNotFound(handlePortalRoot); // Catch captive portal probes (generate_204, hotspot-detect, etc.)
  server.begin();

  // Stay in portal loop until user submits new Wi-Fi credentials
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(5);
  }
}

void drawAPSetupScreen(String ipAddr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(16, 2);
  display.println("WIFI SETUP MODE");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setCursor(2, 18);
  display.print("WiFi: ");
  display.println(AP_SSID);

  display.setCursor(2, 30);
  display.print("Pass: ");
  display.println(strlen(AP_PASSWORD) >= 8 ? AP_PASSWORD : "(Open)");

  display.setCursor(2, 44);
  display.print("Open: http://");
  display.println(ipAddr);

  display.display();
}

void handlePortalRoot() {
  // Scan nearby Wi-Fi networks so the user can pick from a dropdown
  int n = WiFi.scanNetworks();
  String optionsHtml = "";
  for (int i = 0; i < n; i++) {
    String netName = WiFi.SSID(i);
    if (netName.length() > 0) {
      optionsHtml += "<option value='" + netName + "'>" + netName + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Elecsy Wi-Fi Setup</title>
  <style>
    body { font-family: Arial, sans-serif; background: #111827; color: #f9fafb; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
    .card { background: #1f2937; padding: 24px; border-radius: 12px; width: 90%; max-width: 360px; box-shadow: 0 10px 25px rgba(0,0,0,0.4); }
    h2 { margin-top: 0; text-align: center; color: #38bdf8; }
    label { display: block; margin: 12px 0 6px; font-size: 14px; color: #d1d5db; }
    select, input { width: 100%; padding: 10px; border-radius: 6px; border: 1px solid #374151; background: #111827; color: #fff; box-sizing: border-box; font-size: 15px; }
    button { width: 100%; padding: 12px; margin-top: 20px; border: none; border-radius: 6px; background: #0284c7; color: white; font-size: 16px; font-weight: bold; cursor: pointer; }
    button:hover { background: #0369a1; }
    .note { font-size: 12px; color: #9ca3af; margin-top: 10px; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Elecsy Wi-Fi Setup</h2>
    <form action="/save" method="POST">
      <label for="ssid_select">Select Scanned Network:</label>
      <select id="ssid_select" onchange="document.getElementById('ssid').value=this.value">
        <option value="">-- Choose Wi-Fi --</option>
        )rawliteral" + optionsHtml + R"rawliteral(
      </select>

      <label for="ssid">Or Enter Wi-Fi SSID:</label>
      <input type="text" id="ssid" name="ssid" placeholder="Wi-Fi Name (2.4 GHz)" required>

      <label for="pass">Wi-Fi Password:</label>
      <input type="password" id="pass" name="pass" placeholder="Password">

      <button type="submit">Save &amp; Connect</button>
    </form>
    <p class="note">Device will save settings and reboot into Station mode.</p>
  </div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handlePortalSave() {
  if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
    server.send(400, "text/plain", "SSID cannot be empty.");
    return;
  }

  String newSSID = server.arg("ssid");
  String newPass = server.arg("pass");

  // Store permanently in ESP32 NVS Flash
  preferences.begin("wifi-cfg", false);
  preferences.putString("ssid", newSSID);
  preferences.putString("pass", newPass);
  preferences.end();

  String responseHtml = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body style="font-family:Arial;background:#111827;color:#fff;text-align:center;padding-top:60px;">
  <h2 style="color:#4ade80;">Credentials Saved!</h2>
  <p>Connecting to <b>)rawliteral" + newSSID + R"rawliteral(</b>...</p>
  <p>Restarting device now.</p>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", responseHtml);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Wi-Fi Saved!");
  display.setCursor(10, 36);
  display.println("Rebooting...");
  display.display();

  delay(2000);
  ESP.restart(); // Reboot cleanly into STA mode with the new credentials
}

// =========================================================================
// MAIN LOOP & FEATURES
// =========================================================================
void loop() {
  unsigned long currentTime = millis();
  bool currentTouchState = digitalRead(TOUCH_PIN);

  // Detect Touch State Changes (Edge Triggering)
  if (currentTouchState == HIGH && lastTouchState == LOW) {
    unsigned long timeSinceLastTouch = currentTime - lastTouchTime;
    lastTouchTime = currentTime;

    if (timeSinceLastTouch < doubleTapTimeout) {
      touchCount++;
    } else {
      touchCount = 1;
    }

    // --- Action A: DOUBLE TAP DETECTED ---
    if (touchCount == 2) {
      touchCount = 0;
      displayISTTime();
      lastBlinkTime = millis();
    }
  }

  // Handle single tap timeout processing
  if (touchCount == 1 && (currentTime - lastTouchTime > doubleTapTimeout)) {
    touchCount = 0;
    
    // --- Action B: SINGLE TAP DETECTED (SPEECH TO AI INTERACTION) ---
    drawListeningFrame();
    Serial.println("[SYSTEM] Recording voice stream for 3s...");
    
    String spokenTranscript = stt.listen(3000);
    
    if (spokenTranscript.length() > 0) {
      String cleanTranscript = spokenTranscript;
      cleanTranscript.trim();
      if (cleanTranscript.endsWith(".") || cleanTranscript.endsWith("?")) {
        cleanTranscript.remove(cleanTranscript.length() - 1);
      }

      cleanTranscript.toLowerCase();

      if (cleanTranscript.equalsIgnoreCase("your name")) {
        displayResponse("My name is Elecsy! Nice to meet you.");
        delay(3000);
      } 
      else if (cleanTranscript.equalsIgnoreCase("What is the time") || cleanTranscript.equalsIgnoreCase("What's the time")) { 
        touchCount = 0;
        displayISTTime();
        lastBlinkTime = millis();
      }
      else if (cleanTranscript.equalsIgnoreCase("Instagram ID") || cleanTranscript.equalsIgnoreCase("Insta ID")) { 
        showImage(insta_img, SCREEN_WIDTH, SCREEN_HEIGHT);
        delay(3000);
      }
      else if (cleanTranscript.equalsIgnoreCase("Youtube") || cleanTranscript.equalsIgnoreCase("You tube")) { 
        showImage(youtube_img, SCREEN_WIDTH, SCREEN_HEIGHT);
        delay(3000);
      }
      else if (cleanTranscript.equalsIgnoreCase("QR Code") || cleanTranscript.equalsIgnoreCase("Payment Details") || cleanTranscript.equalsIgnoreCase("UPI ID")) { 
        displayResponse("QR Code");
        setOledCameraSafeContrast(0x50); // Prevents camera scanline flicker
        showStablePaymentQR(UPI_URI);
        delay(10000);
      }
      else if (cleanTranscript.indexOf("set a timer") != -1) {
        Serial.println("Matched: set a timer");
        if ((cleanTranscript.indexOf("1 minute") != -1) || (cleanTranscript.indexOf("one minute") != -1)) {
          setMinutes = 1;
        } 
        else if ((cleanTranscript.indexOf("2 minute") != -1) || (cleanTranscript.indexOf("two minute") != -1)) {
          setMinutes = 2;
        } 
        else if ((cleanTranscript.indexOf("5 minute") != -1) || (cleanTranscript.indexOf("five minute") != -1)) {
          setMinutes = 5;
        } 
        else if ((cleanTranscript.indexOf("10 minute") != -1) || (cleanTranscript.indexOf("ten minute") != -1)) {
          setMinutes = 10;
        } 

        totalDurationMs  = (unsigned long)setMinutes * 60000UL;
        remainingMs      = totalDurationMs;
        lastTickMs       = millis();
        state            = RUNNING; // Reset state so repeated runs always start cleanly
        lastTouchState1  = false;
        longPressHandled = false;

        times_up = true;
        while (times_up) {
          handleTouch();
          updateTimer();
          drawCountdownUI();
          delay(20);
        }
      }
      else if (cleanTranscript.indexOf("weather") != -1) { 
        if (fetchDelhiWeather()) {
          drawWeatherScreen();
        }
        lastUpdateMs = millis();
      }
      else if (cleanTranscript.indexOf("wifi setup") != -1 || cleanTranscript.indexOf("reset wifi") != -1) {
        // Voice shortcut to re-open the Wi-Fi AP setup portal anytime
        startWiFiPortal();
      }
      else {
        displayResponse("I didn't get what you said!");
         // Pass the transcribed speech output payload straight into Gemini Core
        String geminiResponse = fetchGeminiAI(spokenTranscript);
        
        // Render the returned content output using the layout window
        displayResponse(geminiResponse);
        delay(3000);
      }

      Serial.print("[STT RESULT] "); Serial.println(spokenTranscript);
    } else {
      Serial.println("[STT WARNING] Empty audio matrix transcript caught.");
      displayResponse("Sorry, I didn't hear anything.");
    }
    
    delay(2000);
    lastBlinkTime = millis();
  }

  lastTouchState = currentTouchState;

  // Normal Operating Mode (Look, Blink, and Idle Face Expressions)
  if (touchCount == 0 && (currentTime - lastBlinkTime > blinkInterval)) {
    drawFaceExpression("blink", 0, 0);
    lastBlinkTime = currentTime;
    blinkInterval = random(2000, 6000); 
  } else if (touchCount == 0) {
    int moveOffset = (currentTime / 2000) % 4; 
    switch(moveOffset) {
      case 0: drawFaceExpression("idle", 0, 0);   break; 
      case 1: drawFaceExpression("idle", 3, 0);   break; 
      case 2: drawFaceExpression("idle", 0, 0);   break; 
      case 3: drawFaceExpression("idle", -3, 0);  break; 
    }
  }
  delay(30); 
}

String fetchGeminiAI(String prompt) {
  if (WiFi.status() != WL_CONNECTED) return "WiFi Error";

  for(int i = 0; i < 20; i++) {
     drawThinkingFrame();
     delay(20); 
  }

  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;
  
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.6-flash:generateContent";
  if (!http.begin(client, url)) return "Conn Init Error";
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-goog-api-key", apiKey); 
  
  String jsonPayload = "{"
                         "\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}],"
                         "\"generationConfig\":{"
                           "\"thinkingConfig\":{\"thinkingLevel\":\"minimal\"}"
                         "}"
                       "}";
  
  int httpResponseCode = http.POST(jsonPayload); 
  String response = "Error parsing response";

  if (httpResponseCode == HTTP_CODE_OK || httpResponseCode > 0) {
    String payload = http.getString();
    String searchKey = "\"text\": \"";
    int startIdx = payload.indexOf(searchKey);
    
    if (startIdx != -1) {
      int textStart = startIdx + searchKey.length();
      int textEnd = payload.indexOf("\"", textStart); 
      
      if (textEnd != -1) {
        response = payload.substring(textStart, textEnd);
        response.trim();
        response.replace("\\n", " ");
        response.replace("\\\"", "\"");
      } else {
        response = "Format Error";
      }
    } else {
      response = "Text Key Missing";
    }
  } else {
    response = "API Error: " + String(httpResponseCode);
  }

  http.end(); 
  return response;
}

void displayISTTime() {
  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)){
    display.clearDisplay();
    display.setCursor(10, 24);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.println("Time Sync Error");
    display.display();
    delay(3000);
    return;
  }

  drawFaceExpression("happy", 0, 0);
  
  char timeBuf[12];   
  char dateBuf[16];   
  
  strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo); 
  strftime(dateBuf, sizeof(dateBuf), "%d-%m-%Y", &timeinfo);

  display.fillRoundRect(20, 12, 88, 40, 4, SSD1306_BLACK);
  display.drawRoundRect(20, 12, 88, 40, 4, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  
  display.setTextSize(2);
  display.setCursor(35, 17); 
  display.print(timeBuf);

  display.setTextSize(1);
  display.setCursor(34, 39);
  display.print(dateBuf);
  
  display.display();
  delay(3000); 
}

void drawBaseFaceStructure() {
  display.clearDisplay();
  display.drawRoundRect(faceX - 4, faceY + 18, 6, 24, 2, SSD1306_WHITE);
  display.drawRoundRect(faceX + faceW - 2, faceY + 18, 6, 24, 2, SSD1306_WHITE);
  display.drawRoundRect(faceX, faceY, faceW, faceH, faceRadius, SSD1306_WHITE);
}

void drawFaceExpression(String type, int pupilOffsetX, int pupilOffsetY) {
  drawBaseFaceStructure();

  if (type == "blink") {
    display.drawLine(leftEyeX - eyeRadius, eyeY, leftEyeX + eyeRadius, eyeY, SSD1306_WHITE);
    display.drawLine(rightEyeX - eyeRadius, eyeY, rightEyeX + eyeRadius, eyeY, SSD1306_WHITE);
    display.drawLine(54, 46, 74, 46, SSD1306_WHITE);
    display.display();
    delay(150);
  } 
  else if (type == "idle") {
    display.drawCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(leftEyeX + pupilOffsetX, eyeY + pupilOffsetY, pupilRadius, SSD1306_WHITE);
    display.drawCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(rightEyeX + pupilOffsetX, eyeY + pupilOffsetY, pupilRadius, SSD1306_WHITE);
    display.drawLine(54, 46, 74, 46, SSD1306_WHITE);
    display.drawLine(54, 46, 52, 44, SSD1306_WHITE);
    display.drawLine(74, 46, 76, 44, SSD1306_WHITE);
  }
  else if (type == "happy") {
    display.drawCircle(leftEyeX, eyeY + 4, eyeRadius - 2, SSD1306_WHITE);
    display.fillRect(leftEyeX - eyeRadius, eyeY + 4, (eyeRadius * 2) + 2, eyeRadius + 2, SSD1306_BLACK);
    display.drawCircle(rightEyeX, eyeY + 4, eyeRadius - 2, SSD1306_WHITE);
    display.fillRect(rightEyeX - eyeRadius, eyeY + 4, (eyeRadius * 2) + 2, eyeRadius + 2, SSD1306_BLACK);
    display.fillRoundRect(52, 44, 24, 8, 3, SSD1306_WHITE);
  }
  display.display();
}

void drawListeningFrame() {
  drawBaseFaceStructure();
  display.drawCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
  display.drawCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
  display.drawCircle(64, 46, 6, SSD1306_WHITE);
  display.fillRoundRect(24, 54, 80, 9, 2, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(29, 55);
  display.print("LISTENING (3s)");
  display.display();
}

void drawThinkingFrame() {
  drawBaseFaceStructure();
  if (thinkingDirection) {
    thinkingOffset += 2;
    if (thinkingOffset >= 12) thinkingDirection = false;
  } else {
    thinkingOffset -= 2;
    if (thinkingOffset <= -12) thinkingDirection = true;
  }
  display.drawRoundRect(leftEyeX - 16, eyeY - 3, 32, 6, 3, SSD1306_WHITE);
  display.drawRoundRect(rightEyeX - 16, eyeY - 3, 32, 6, 3, SSD1306_WHITE);
  display.fillCircle(leftEyeX + thinkingOffset, eyeY, 2, SSD1306_WHITE);
  display.fillCircle(rightEyeX + thinkingOffset, eyeY, 2, SSD1306_WHITE);
  display.drawLine(56, 46, 72, 46, SSD1306_WHITE);
  display.display();
}

void drawWifiConnectingFrame() {
  drawBaseFaceStructure();
  display.drawCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
  display.drawCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
  float angle = (wifiAnimFrame * 45) * DEG_TO_RAD;
  int px = cos(angle) * (eyeRadius - 4);
  int py = sin(angle) * (eyeRadius - 4);
  display.fillCircle(leftEyeX + px, eyeY + py, 2, SSD1306_WHITE);
  display.fillCircle(rightEyeX + px, eyeY + py, 2, SSD1306_WHITE);
  int mouthRadius = 4 + (wifiAnimFrame % 3) * 4;
  display.drawCircle(64, 52, mouthRadius, SSD1306_WHITE);
  display.fillRect(40, 53, 48, 12, SSD1306_BLACK);
  display.fillRoundRect(33, 54, 62, 9, 2, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(34, 55);
  display.print("CONNECTING...");
  display.display();
  wifiAnimFrame = (wifiAnimFrame + 1) % 8;
}

void drawWifiConnectedFrame() {
  drawFaceExpression("happy", 0, 0);
  display.fillRoundRect(42, 54, 44, 9, 2, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(45, 55);
  display.print("ONLINE");
  display.display();
}

void displayResponse(String text) {
  drawFaceExpression("happy", 0, 0);
  delay(800);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Gemini AI Says:");
  display.println("---------------------");
  if (text.length() > 42) {
    display.println(text.substring(0, 40) + "...");
  } else {
    display.println(text);
  }
  display.display();
}

void handleTouch() {
  bool currentTouch = (digitalRead(TOUCH_PIN) == HIGH);
  unsigned long now = millis();

  if (currentTouch && !lastTouchState1) {
    touchStartMs     = now;
    longPressHandled = false;
  }

  if (currentTouch && !longPressHandled && (now - touchStartMs >= 800)) {
    longPressHandled = true;
    if (state != RUNNING) {
      setMinutes++;
      if (setMinutes > 60) setMinutes = 1;
      totalDurationMs = (unsigned long)setMinutes * 60000UL;
      remainingMs     = totalDurationMs;
      state           = READY;
      display.invertDisplay(false);
    }
  }

  if (!currentTouch && lastTouchState1 && !longPressHandled) {
    if (now - touchStartMs > 40) {
      if (state == READY || state == PAUSED) {
        state      = RUNNING;
        lastTickMs = now;
      } else if (state == RUNNING) {
        state = PAUSED;
      } else if (state == FINISHED) {
        display.invertDisplay(false);
        totalDurationMs = (unsigned long)setMinutes * 60000UL;
        remainingMs     = totalDurationMs;
        state           = READY;
      }
    }
  }

  lastTouchState1 = currentTouch;
}

void updateTimer() {
  unsigned long now = millis();

  if (state == RUNNING) {
    unsigned long elapsed = now - lastTickMs;
    lastTickMs = now;

    if (elapsed >= remainingMs) {
      remainingMs = 0;
      state       = FINISHED;
    } else {
      remainingMs -= elapsed;
    }
  } else {
    lastTickMs = now;
  }
}

void drawCountdownUI() {
  display.clearDisplay();

  unsigned long totalSec = (remainingMs + 999) / 1000;
  int mins = totalSec / 60;
  int secs = totalSec % 60;
  float progress = (totalDurationMs > 0) ? ((float)remainingMs / (float)totalDurationMs) : 0.0f;

  int ringX = 28;
  int ringY = 32;
  drawProgressRing(ringX, ringY, 25, 19, progress);

  int pct = (int)(progress * 100.0f);
  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(pctBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(ringX - (w / 2), ringY - (h / 2));
  display.print(pctBuf);

  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

  display.setTextSize(2);
  display.setCursor(62, 14);
  display.print(timeBuf);

  display.setTextSize(1);
  display.setCursor(62, 36);
  if (state == READY) {
    display.print("TAP: START");
  } else if (state == PAUSED) {
    display.print("PAUSED");
  } else if (state == RUNNING) {
    int dots = (millis() / 300) % 4;
    display.print("RUNNING");
    for (int i = 0; i < dots; i++) display.print(".");
  } else if (state == FINISHED) {
    display.print("TIME'S UP!");
    display.drawRect(62, 50, 60, 6, SSD1306_WHITE);
    display.display();
    delay(5000);
    state    = READY;
    times_up = false;
    return;
  }

  int barX = 62;
  int barY = 50;
  int barW = 60;
  int barH = 6;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int fillW = (int)((barW - 2) * progress);
  if (fillW > 0) {
    display.fillRect(barX + 1, barY + 1, fillW, barH - 2, SSD1306_WHITE);
  }

  display.display();
}

void drawProgressRing(int cx, int cy, int rOuter, int rInner, float progress) {
  display.drawCircle(cx, cy, rOuter, SSD1306_WHITE);
  display.drawCircle(cx, cy, rInner, SSD1306_WHITE);

  int totalSegments = 60;
  int activeSegments = (int)(totalSegments * progress);

  for (int i = 0; i < activeSegments; i++) {
    float angleDeg = -90.0f + (i * 360.0f / totalSegments);
    float rad = angleDeg * PI / 180.0f;

    int x0 = cx + cos(rad) * (rInner + 1);
    int y0 = cy + sin(rad) * (rInner + 1);
    int x1 = cx + cos(rad) * (rOuter - 1);
    int y1 = cy + sin(rad) * (rOuter - 1);

    display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
  }
}

bool fetchDelhiWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(WEATHER_URL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject current = doc["current"];
      temperature  = current["temperature_2m"] | 0.0;
      humidity     = current["relative_humidity_2m"] | 0;
      windSpeed    = current["wind_speed_10m"] | 0.0;
      weatherCode  = current["weather_code"] | 0;
      hasValidData = true;
      http.end();
      return true;
    } else {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("HTTP GET failed, error: %d\n", httpCode);
  }

  http.end();
  return false;
}

void drawWeatherScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(2, 1);
  display.print("NEW DELHI, IN");
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  drawWeatherIcon(18, 30, weatherCode);

  display.setTextSize(2);
  display.setCursor(44, 16);
  display.print(temperature, 1);
  display.setTextSize(1);
  display.print(" C");

  display.setCursor(44, 36);
  display.print(getWeatherDescription(weatherCode));

  display.drawLine(0, 51, 127, 51, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.printf("Hum:%d%%  Wind:%.0fkm/h", humidity, windSpeed);

  display.display();
}

const char* getWeatherDescription(int code) {
  if (code == 0) return "Clear Sky";
  if (code == 1 || code == 2) return "Partly Cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog / Haze";
  if (code >= 51 && code <= 67) return "Rain / Drizzle";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Rain Showers";
  if (code >= 95) return "Thunderstorm";
  return "Unknown";
}

void drawWeatherIcon(int x, int y, int code) {
  if (code == 0) {
    display.fillCircle(x, y, 7, SSD1306_WHITE);
    for (int a = 0; a < 360; a += 45) {
      float rad = a * PI / 180.0;
      display.drawLine(x + cos(rad) * 10, y + sin(rad) * 10,
                       x + cos(rad) * 13, y + sin(rad) * 13, SSD1306_WHITE);
    }
  } else if (code == 45 || code == 48) {
    display.drawLine(x - 11, y - 5, x + 11, y - 5, SSD1306_WHITE);
    display.drawLine(x - 8,  y,     x + 12, y,     SSD1306_WHITE);
    display.drawLine(x - 12, y + 5, x + 9,  y + 5, SSD1306_WHITE);
  } else {
    display.fillCircle(x - 5, y + 2, 5, SSD1306_WHITE);
    display.fillCircle(x + 2, y - 1, 6, SSD1306_WHITE);
    display.fillCircle(x + 8, y + 3, 4, SSD1306_WHITE);
    display.fillRect(x - 5, y + 2, 14, 6, SSD1306_WHITE);

    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 99)) {
      display.drawLine(x - 4, y + 10, x - 6, y + 14, SSD1306_WHITE);
      display.drawLine(x + 2, y + 10, x,     y + 14, SSD1306_WHITE);
      display.drawLine(x + 8, y + 10, x + 6, y + 14, SSD1306_WHITE);
    }
  }
}
