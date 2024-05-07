#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define pin constants and other configuration parameters
#define P1 5
#define P2 6
#define C1 2
#define C2 3
#define ledPin 11
#define qtiPin 8
#define period 2
#define numVals 2000
#define throwawayCycles 1500

// Parameters for the OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16

// Initialize the OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Arrays to store PPG readings
double red[numVals];
double ir[numVals];

// Variables to manage LED state
int isRed = 1;
int count = 0;
int throwawayCount = 0;
int brightness;
String fitzType;

// Network credentials and server URL
const char* ssid = "DukeOpen";
String serverName = "http://152.3.76.100:5000";

// Struct to hold health data received from the server
struct HealthData {
  float oxygenation;
  float heartRate;
};

void setup() {
  // Configure input and output pins for sensors and LEDs
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(P1, OUTPUT);
  pinMode(P2, OUTPUT);
  pinMode(C1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Initialize LEDs to be off
  digitalWrite(P1, LOW);
  digitalWrite(P2, LOW);
  digitalWrite(C1, LOW);
  digitalWrite(C2, LOW);

  // Read initial QTI sensor value and modulate LED brightness accordingly
  float rcTime = qtiReading(20);
  modulateBrightness(rcTime);
  analogWrite(ledPin, brightness);

  // Initialize the OLED display
  while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C));
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(fitzType);
  display.display();

  // Set initial LED states
  digitalWrite(P1, HIGH);
  digitalWrite(P2, !digitalRead(P1));
  digitalWrite(C1, HIGH);
  digitalWrite(C2, !digitalRead(C1));
}

void loop() {
  // Discard initial samples to allow sensor to stabilize
  if (throwawayCount < throwawayCycles) {
    digitalWrite(P1, !digitalRead(P1));
    digitalWrite(P2, !digitalRead(P2));
    digitalWrite(C1, !digitalRead(C1));
    digitalWrite(C2, !digitalRead(C2));
    isRed = !isRed;
    throwawayCount++;
    delay(period);
  } else {
    // Alternate between Red and IR LED sensor readings
    if (isRed) {
      int sensorValue = analogRead(A0);
      float voltage = sensorValue * (3.3 / 4095.0);
      red[count] = (double) voltage;
    } else {
      int sensorValue = analogRead(A1);
      float voltage = sensorValue * (3.3 / 4095.0);
      ir[count] = (double) voltage;
      count++;
    }

    // If enough data has been collected, process and send it
    if (count >= numVals) {
      count = 0;
      throwawayCount = 0;
      String redString = convertData(red);
      String irString = convertData(ir);
      sendData(redString, 0);
      sendData(irString, 1);

      // Retrieve processed health data from the server
      HealthData oxyHR = retrieveData();

      // Display the health data on the OLED screen
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 20);
      display.println(fitzType);
      int oxygenation = (int) oxyHR.oxygenation;
      int heartRate = (int) oxyHR.heartRate;
      display.print(oxygenation);
      display.print("% ");
      display.print(heartRate);
      display.println(" PR");
      display.display();
    }

    // Switch the LED being used for measurement
    digitalWrite(P1, !digitalRead(P1));
    digitalWrite(P2, !digitalRead(P2));
    digitalWrite(C1, !digitalRead(C1));
    digitalWrite(C2, !digitalRead(C2));
    isRed = !isRed;

    delay(period);
  }
}

float qtiReading(int numReadings) {
  long totalTime = 0;
  float numReadingsFloat = (float) numReadings;
  for (int i = 0; i < numReadings; i++) {
    // Charge and discharge the QTI sensor to measure resistance
    pinMode(qtiPin, OUTPUT);
    digitalWrite(qtiPin, HIGH);
    delay(1);
    pinMode(qtiPin, INPUT);
    digitalWrite(qtiPin, LOW);
    long time = micros();
    while (digitalRead(qtiPin));
    time = micros() - time;
    totalTime += time;
    delay(100);
  }
  float totalTimeFloat = (float) totalTime;
  float avgTime = totalTimeFloat / numReadingsFloat;
  return avgTime;
}

void modulateBrightness(float rcTime) {
  // Adjust LED brightness and determine Fitzpatrick type based on QTI sensor reading
  if (rcTime < 350.0) {
    brightness = 50;
    fitzType = "Type 1";
  } else if (rcTime < 400.0) {
    brightness = 50;
    fitzType = "Type 2";
  } else if (rcTime < 560.0) {
    brightness = 100;
    fitzType = "Type 3";
  } else if (rcTime < 640.0) {
    brightness = 135;
    fitzType = "Type 4";
  } else if (rcTime < 680.0) {
    brightness = 200;
    fitzType = "Type 5";
  } else {
    brightness = 255;
    fitzType = "Type 6";
  }
}

String convertData(double* arr) {
  // Convert an array of double values to a comma-separated string
  String dataString = "";
  for (int i = 0; i < numVals; i++) {
    dataString += String(arr[i], 5);
    if (i < numVals - 1) dataString += ",";
  }
  return dataString;
}

void sendData(String data, int indicator) {
  // Connect to WiFi and send data to the server
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1);
  }
  String ind = indicator ? "ir" : "red";
  HTTPClient http;
  String sendServer = serverName + "/send_data";
  http.begin(sendServer);
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"indicator\":\"" + ind + "\", \"dataString\":\"" + data + "\"}";
  int httpResponseCode = http.POST(payload);
  http.end();
  WiFi.disconnect();
}

HealthData retrieveData() {
  HealthData data;
  // Retrieve processed health data from the server
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1);
  }
  HTTPClient http;
  String retrieveServer = serverName + "/retrieve_data";
  http.begin(retrieveServer);
  int httpResponseCode = http.GET();

  // If the request is successful, extract data from the JSON response
  if (httpResponseCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    data.oxygenation = doc["spo2"];
    data.heartRate = doc["hr"];
  }

  http.end();
  WiFi.disconnect();
  return data;
}
