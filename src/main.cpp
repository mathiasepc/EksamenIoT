/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com
*********/

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

// DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// Libraries to get time from NTP Server
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

float lastTemperature = -999.0;

// Define deep sleep options
uint64_t uS_TO_S_FACTOR = 1000000; // Conversion factor for microseconds to seconds
// Sleep for 2 minutes = 120 seconds
uint64_t TIME_TO_SLEEP = 120;

// Replace with your network credentials
const char *ssid = "Hidden";
const char *password = "Hidden";

// Define variables to keep track of time
unsigned long previousMillis = 0;                 // Store the previous millis
const unsigned long interval = 300000;             // 5 minutes in milliseconds
const unsigned long intervalTemperatures = 60000; // 20 seconds interval

// Temperature Sensor variables
float temperature;
unsigned long lastTemperatureMillis = 0;

// Define CS pin for the SD card module
#define SD_CS 5

// Define the GPIO pin to which the button is connected
#define buttonPin 14
bool buttonPressed = false;

// Prototype
void writeFile(fs::FS &fs, const char *path, const char *message);
void getReadings();
void getTimeStamp();
void logSDCard();
void appendFile(fs::FS &fs, const char *path, const char *message);
void buttonInterrupt();

// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

String dataMessage;

// Data wire is connected to ESP32 GPIO 21
#define ONE_WIRE_BUS 4
// Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// Function to handle WebSocket data from the client
void handleWebSocketData(AsyncWebSocketClient *client, uint8_t *data, size_t len)
{
  String command = String((char *)data);

  if (command == "get_temperature")
  {
    // float temperatureC = sensors.getTempCByIndex(0);
    String temperatureString = String(temperature);
    client->text(temperatureString);
  }
}

// This function handles WebSocket events
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    // Call a separate function to handle WebSocket data
    handleWebSocketData(client, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  // Start serial communication for debugging purposes
  Serial.begin(115200);

  pinMode(buttonPin, INPUT_PULLUP);                         // Configure the button pin as input with a pull-up resistor
  esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonPin, LOW); // Configure the button pin as an external wake-up source

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Initialize an NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200);

  // Initialize SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  initWebSocket();

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Initialize SD card
  SD.begin(SD_CS);
  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS))
  {
    Serial.println("ERROR - SD card initialization failed!");
    return; // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if (!file)
  {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading ID, Date, Hour, Temperature \r\n");
  }
  else
  {
    Serial.println("File already exists");
  }
  file.close();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html"); });

  // Set up the web server
  server.on("/download_csv", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    File file = SD.open("/data.txt", FILE_READ);
    if (file) {
      request->send(file, "data.csv", "text/csv");
      file.close();
    } else {
      request->send(404, "text/plain", "File not found");
    } });

  server.on("/clear_csv", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (SD.remove("/data.txt")) {
      File file = SD.open("/data.txt", FILE_WRITE);
      if (file) {
        file.close();
        request->send(200, "text/plain", "CSV file cleared");
      } else {
        request->send(500, "text/plain", "Failed to create a new CSV file");
      }
    } else {
      request->send(500, "text/plain", "Failed to delete CSV file");
    } });

  server.begin();

  // Enable Timer wake_up
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Start the DallasTemperature library
  sensors.begin();
}

void loop()
{
  // Get the current time
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    // Save the current time as the new "previous" time
    previousMillis = currentMillis;

    // Run your code
    Serial.println("DONE! Going to sleep now.");
    esp_deep_sleep_start();
  }

  // Check for temperature change
  if (currentMillis - lastTemperatureMillis >= intervalTemperatures)
  {
    ws.cleanupClients();

    // Get temperature
    sensors.requestTemperatures();
    temperature = sensors.getTempCByIndex(0);

    getReadings();
    getTimeStamp();
    logSDCard();

    // Update the timer for temperature readings
    lastTemperatureMillis = currentMillis;
  }
}

// This function is called when the button is pressed
void buttonInterrupt()
{
  // Set the buttonPressed flag to true
  buttonPressed = true;
  Serial.println("Button pressed! Waking up from deep sleep.");
}

// Function to get temperature
void getReadings()
{
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0); // Temperature in Celsius
  String temperatureString = String(temperature);
  ws.textAll(temperatureString);
  Serial.print("Temperature: ");
  Serial.println(temperature);
}

// Function to get date and time from NTPClient
void getTimeStamp()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);
}

// Write the sensor readings on the SD card
void logSDCard()
{
  readingID++;
  dataMessage = String(readingID) + "," + String(dayStamp) + "," + String(timeStamp) + "," +
                String(temperature) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
  // Increment readingID on every new reading
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}
