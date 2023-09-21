// #include <Arduino.h>
// #include <OneWire.h>
// #include <DallasTemperature.h>
// #include <WiFi.h>
// #include <ESPAsyncWebServer.h>
// #include <SPIFFS.h>

// float lastTemperature = -999.0;

// // GPIO where the DS18B20 is connected to
// const int oneWireBus = 4;

// // Setup a oneWire instance to communicate with any OneWire devices
// OneWire oneWire(oneWireBus);

// // Pass our oneWire reference to Dallas Temperature sensor
// DallasTemperature sensors(&oneWire);
// AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");

// // Replace with your network credentials
// const char *ssid = "E308";
// const char *password = "98806829";

// // Create AsyncWebServer object on port 80

// String readBME280Temperature()
// {
//   // Read temperature as Celsius (the default)
//   float t = sensors.getTempCByIndex(0);
//   // Convert temperature to Fahrenheit
//   // t = 1.8 * t + 32;
//   if (isnan(t))
//   {
//     Serial.println("Failed to read from BME280 sensor!");
//     return "";
//   }
//   else
//   {
//     Serial.println(t);
//     return String(t);
//   }
// }

// // Funktion til at håndtere WebSocket-data fra client
// void handleWebSocketData(AsyncWebSocketClient *client, uint8_t *data, size_t len)
// {
//   String command = String((char *)data);

//   if (command == "get_temperature")
//   {
//     float temperatureC = sensors.getTempCByIndex(0);
//     String temperatureString = String(temperatureC);
//     client->text(temperatureString);
//   }
// }

    // Denne handler events med websocket
// void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
//              void *arg, uint8_t *data, size_t len)
// {
//   switch (type)
//   {
//   case WS_EVT_CONNECT:
//     Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
//     break;
//   case WS_EVT_DISCONNECT:
//     Serial.printf("WebSocket client #%u disconnected\n", client->id());
//     break;
//   case WS_EVT_DATA:
//     // Kald separat funktion for at håndtere WebSocket-data
//     handleWebSocketData(client, data, len);
//     break;
//   case WS_EVT_PONG:
//   case WS_EVT_ERROR:
//     break;
//   }
// }

// void initWebSocket()
// {
//   ws.onEvent(onEvent);
//   server.addHandler(&ws);
// }

// void setup()
// {
//   // Start the Serial Monitor
//   Serial.begin(115200);
//   // Start the DS18B20 sensor
//   sensors.begin();

//   // Initialize SPIFFS
//   if (!SPIFFS.begin())
//   {
//     Serial.println("An Error has occurred while mounting SPIFFS");
//     return;
//   }

//   // Connect to Wi-Fi
//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(1000);
//     Serial.println("Connecting to WiFi..");
//   }

//   // Print ESP32 Local IP Address
//   Serial.println(WiFi.localIP());

//   initWebSocket();

//   // Route for root / web page
//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send(SPIFFS, "/index.html"); });
//   server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
//             { request->send_P(200, "text/plain", readBME280Temperature().c_str()); });

//   // Start server
//   server.begin();
// }

// void loop()
// {
//   //Henter temperatur
//   sensors.requestTemperatures();
//   float temperatureC = sensors.getTempCByIndex(0);

//   // Tjek for temperaturændring
//   if (temperatureC != lastTemperature)
//   {
//     ws.cleanupClients();

//     Serial.println(temperatureC);
    
//     //send til client
//     String temperatureString = String(temperatureC);
//     ws.textAll(temperatureString);

//     // Opdater lastTemperature
//     lastTemperature = temperatureC;
//   }

//   delay(1000);
// }