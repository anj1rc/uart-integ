#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

const char* ssid = "realme7";       
const char* password = "ricorico";  

const String firebaseURL = "https://uartsensor-fa097-default-rtdb.asia-southeast1.firebasedatabase.app/";
const String databaseSecret = "ib8uGLIQnJvKXkTrlEJ90P3IJEr9IZIxgHwCP81r"; 

#define RXD2 16 
#define TXD2 17 

struct SensorData {
  float temperature;
  float humidity;
  int ldrValue;
  int waterLevel;
  String ledStatus;
  String relayStatus;
  unsigned long timestamp;
};

SensorData lastValidData;     
bool newDataAvailable = false;

WebServer server(80);

String incomingBuffer = ""; 

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 

  connectToWiFi();

  server.on("/", handleRoot);          
  server.on("/data.json", handleJSON);  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  static unsigned long lastUploadTime = 0;
  const unsigned long uploadInterval = 5000; 

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      if (incomingBuffer.length() > 0) {
        Serial.println("Raw UART data: " + incomingBuffer); 

        SensorData sensorData = parseSensorData(incomingBuffer);

        if (validateSensorData(sensorData)) {
          Serial.println("Received valid sensor data:");
          printSensorData(sensorData);

          lastValidData = sensorData;
          lastValidData.timestamp = millis();
          newDataAvailable = true;

          if (WiFi.status() == WL_CONNECTED) {
            if (uploadToFirebase(sensorData)) {
              Serial.println("Data uploaded to Firebase successfully");
            } else {
              Serial.println("Failed to upload to Firebase");
            }
          } else {
            Serial.println("WiFi disconnected - attempting to reconnect");
            connectToWiFi();
          }
        }

        incomingBuffer = ""; 
      }
    } else {
      incomingBuffer += c;
    }
  }

  server.handleClient();

  if (millis() - lastUploadTime >= uploadInterval) {
    lastUploadTime = millis();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}


void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32 Sensor Server</h1>";

  if (newDataAvailable) {
    html += "<p><strong>Temperature:</strong> " + String(lastValidData.temperature) + " &deg;C</p>";
    html += "<p><strong>Humidity:</strong> " + String(lastValidData.humidity) + " %</p>";
    html += "<p><strong>LDR Value:</strong> " + String(lastValidData.ldrValue) + "</p>";
    html += "<p><strong>Water Level:</strong> " + String(lastValidData.waterLevel) + "</p>";
    html += "<p><strong>LED Status:</strong> " + lastValidData.ledStatus + "</p>";
    html += "<p><strong>Relay Status:</strong> " + lastValidData.relayStatus + "</p>";
    html += "<p><strong>Last Updated:</strong> " + String(lastValidData.timestamp / 1000) + "s ago</p>";
    html += "<p><a href=\"/data.json\">View as JSON</a></p>";
  } else {
    html += "<p>No sensor data available yet.</p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleJSON() {
  DynamicJsonDocument doc(512);
  doc["temperature"] = lastValidData.temperature;
  doc["humidity"] = lastValidData.humidity;
  doc["light"] = lastValidData.ldrValue;
  doc["water"] = lastValidData.waterLevel;
  doc["led"] = lastValidData.ledStatus;
  doc["relay"] = lastValidData.relayStatus;
  doc["timestamp"] = lastValidData.timestamp;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

SensorData parseSensorData(String data) {
  SensorData result;

  result.temperature = NAN;
  result.humidity = NAN;
  result.ldrValue = -1;
  result.waterLevel = -1;
  result.ledStatus = "ERROR";
  result.relayStatus = "ERROR";

  if (data.indexOf("Temp:") == -1 ||
      data.indexOf(",Hum:") == -1 ||
      data.indexOf(",LDR:") == -1 ||
      data.indexOf(",Water:") == -1 ||
      data.indexOf(",LED:") == -1 ||
      data.indexOf(",Relay:") == -1) {
    return result;
  }

  int tempStart = data.indexOf("Temp:") + 5;
  int tempEnd = data.indexOf(",Hum:");
  result.temperature = data.substring(tempStart, tempEnd).toFloat();

  int humStart = data.indexOf("Hum:") + 4;
  int humEnd = data.indexOf(",LDR:");
  result.humidity = data.substring(humStart, humEnd).toFloat();

  int ldrStart = data.indexOf("LDR:") + 4;
  int ldrEnd = data.indexOf(",Water:");
  result.ldrValue = data.substring(ldrStart, ldrEnd).toInt();

  int waterStart = data.indexOf("Water:") + 6;
  int waterEnd = data.indexOf(",LED:");
  result.waterLevel = data.substring(waterStart, waterEnd).toInt();

  int ledStart = data.indexOf("LED:") + 4;
  int ledEnd = data.indexOf(",Relay:");
  result.ledStatus = data.substring(ledStart, ledEnd).c_str();
  result.ledStatus.trim();             
  result.ledStatus.toUpperCase();      

  int relayStart = data.indexOf("Relay:") + 6;
  result.relayStatus = data.substring(relayStart).c_str();
  result.relayStatus.trim();          
  result.relayStatus.toUpperCase();  

  return result;
}

bool validateSensorData(SensorData data) {
  if (isnan(data.temperature) || data.temperature < -50 || data.temperature > 100) {
    Serial.println("Validation failed: Invalid temperature");
    return false;
  }
  if (isnan(data.humidity) || data.humidity < 0 || data.humidity > 100) {
    Serial.println("Validation failed: Invalid humidity");
    return false;
  }
  if (data.ldrValue < 0 || data.ldrValue > 4095) {
    Serial.println("Validation failed: Invalid LDR value");
    return false;
  }
  if (data.waterLevel < 0 || data.waterLevel > 1023) {
    Serial.println("Validation failed: Invalid water level");
    return false;
  }
  if (data.ledStatus != "ON" && data.ledStatus != "OFF") {
    Serial.println("Validation failed: Invalid LED status (" + data.ledStatus + ")");
    return false;
  }
  if (data.relayStatus != "ON" && data.relayStatus != "OFF") {
    Serial.println("Validation failed: Invalid Relay status (" + data.relayStatus + ")");
    return false;
  }

  return true;
}

void printSensorData(SensorData data) {
  Serial.print("Temperature: ");
  Serial.print(data.temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(data.humidity);
  Serial.println(" %");

  Serial.print("LDR Value: ");
  Serial.println(data.ldrValue);

  Serial.print("Water Level: ");
  Serial.println(data.waterLevel);

  Serial.print("LED Status: ");
  Serial.println(data.ledStatus);

  Serial.print("Relay Status: ");
  Serial.println(data.relayStatus);
}


bool uploadToFirebase(SensorData data) {
  HTTPClient http;

  DynamicJsonDocument doc(256);
  doc["temperature"] = data.temperature;
  doc["humidity"] = data.humidity;
  doc["light"] = data.ldrValue;
  doc["water"] = data.waterLevel;
  doc["led"] = data.ledStatus;
  doc["relay"] = data.relayStatus;
  doc["timestamp"] = millis();

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String fullPath = firebaseURL + "sensorData.json?auth=" + databaseSecret;
  http.begin(fullPath);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonPayload);
  String response = http.getString();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Firebase response: " + response);
    http.end();
    return true;
  } else {
    Serial.print("Error code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + response);
    http.end();
    return false;
  }
}