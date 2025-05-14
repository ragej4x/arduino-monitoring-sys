#include <WiFiS3.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int waterPumpPin = 13;
const int solutionAPin = 12;
const int solutionBPin = 8;
const int solutionCPin = 7;
const int solutionDPin = 4;

const int floaterSwitchPin = A3;
const int phSensorPin = A0;
const int tdsSensorPin = A1;
const int temperatureSensorPin = 2;

OneWire oneWire(temperatureSensorPin);
DallasTemperature sensors(&oneWire);

#define VREF 5.0
#define SCOUNT 30
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float temperature = 25;

float calibration_value = 21.34;
int phval = 0; 
unsigned long int avgval; 
int buffer_arr[10], temp;

unsigned long lastSensorReadTime = 0;
const unsigned long sensorReadInterval = 5000;

#define WIFI_SSID "PLDTHOMEFIBRb8900"
#define WIFI_PASSWORD "#Fk9lratv123456789"

#define FIREBASE_HOST "iot-monitoringsys-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "Bfblx5ubfwbbPw6vbM13Scw3zGdDw6iZPOgV3RaW"

WiFiSSLClient client;

int ph_limit = 14;
int ppm_limit = 0;
int ph_min = -3;
int ppm_min = 10;
int scan_interval = 1;
int fail_safe = 0;

unsigned long pumpAStartTime = 0;
unsigned long pumpBStartTime = 0;
unsigned long pumpCStartTime = 0;
unsigned long pumpDStartTime = 0;
bool pumpARunning = false;
bool pumpBRunning = false;
bool pumpCRunning = false;
bool pumpDRunning = false;

float readTemperature() {
  sensors.requestTemperatures();
  float celsius = sensors.getTempCByIndex(0);
  
  if (celsius == -127.00) {
    return 25.0;
  }
  
  return celsius;
}

float readPh() {
  for (int i = 0; i < 10; i++) { 
    buffer_arr[i] = analogRead(phSensorPin);
    delay(10);
  }

  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }

  float volt = (float)avgval * 5.0 / 1024 / 6;
  float ph_act = -5.70 * volt + calibration_value;
  
  return ph_act;
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
    
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  
  return bTemp;
}

int readPpm() {
  static unsigned long analogSampleTimepoint = millis();
  
  if(millis() - analogSampleTimepoint > 40) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(tdsSensorPin);
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT) {
      analogBufferIndex = 0;
    }
  }
  
  for(copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
    analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
  }
  
  float averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 1024.0;
  
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensationVoltage = averageVoltage / compensationCoefficient;
  
  int ppmValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 
                 255.86 * compensationVoltage * compensationVoltage + 
                 857.39 * compensationVoltage) * 0.5;
                 
  return ppmValue;
}

bool readFloatSwitch() {
  return digitalRead(floaterSwitchPin) == HIGH;
}

void sendLogToFirebase(String message) {
  if (client.connect(FIREBASE_HOST, 443)) {
    unsigned long currentTime = millis();
    
    StaticJsonDocument<256> doc;
    doc["message"] = message;
    doc["timestamp"] = currentTime;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    String path = "/logs/" + String(currentTime) + ".json?auth=" + String(FIREBASE_AUTH);
    
    client.println("PUT " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonStr.length());
    client.println();
    client.println(jsonStr);
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 10000) {
        client.stop();
        return;
      }
    }
    
    while (client.available()) {
      client.read();
    }
  }
  client.stop();
}

void updatePumpStatusInFirebase() {
  if (client.connect(FIREBASE_HOST, 443)) {
    StaticJsonDocument<256> doc;
    bool waterLevelHigh = readFloatSwitch();
    
    doc["water_pump"] = digitalRead(waterPumpPin) == HIGH;
    doc["water_pump_manual_control"] = false;
    
    doc["pump_a"] = digitalRead(solutionAPin) == HIGH;
    doc["pump_b"] = digitalRead(solutionBPin) == HIGH;
    doc["pump_c"] = digitalRead(solutionCPin) == HIGH;
    doc["pump_d"] = digitalRead(solutionDPin) == HIGH;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    String path = "/pumps.json?auth=" + String(FIREBASE_AUTH);
    
    client.println("PUT " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonStr.length());
    client.println();
    client.println(jsonStr);
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }
    
    while (client.available()) {
      client.read();
    }
  }
  client.stop();
}

void checkAndAdjustLevels(float currentPh, int currentPpm) {
  bool pumpStateChanged = false;
  bool adjustmentMade = false;
  
  if (ppm_min > 0) {
    if (currentPpm < ppm_min) {
      if (!pumpARunning) {
        digitalWrite(solutionAPin, HIGH);
        pumpAStartTime = millis();
        pumpARunning = true;
        pumpStateChanged = true;
        adjustmentMade = true;
        String message = "PPM below minimum (" + String(currentPpm) + " < " + String(ppm_min) + "). Activating Pump A.";
        Serial.println(message);
        sendLogToFirebase(message);
        
        if (pumpStateChanged) {
          updatePumpStatusInFirebase();
          return;
        }
      }
    } else if (ppm_limit > 0 && currentPpm > ppm_limit) {
      if (!pumpBRunning) {
        digitalWrite(solutionBPin, HIGH);
        pumpBStartTime = millis();
        pumpBRunning = true;
        pumpStateChanged = true;
        adjustmentMade = true;
        String message = "PPM above limit (" + String(currentPpm) + " > " + String(ppm_limit) + "). Activating Pump B.";
        Serial.println(message);
        sendLogToFirebase(message);
        
        if (pumpStateChanged) {
          updatePumpStatusInFirebase();
          return;
        }
      }
    } else {
      if (pumpARunning) {
        digitalWrite(solutionAPin, LOW);
        unsigned long duration = (millis() - pumpAStartTime) / 1000;
        pumpARunning = false;
        pumpStateChanged = true;
        String message = "PPM within range. Pump A turned off after " + String(duration) + " seconds.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
      
      if (pumpBRunning) {
        digitalWrite(solutionBPin, LOW);
        unsigned long duration = (millis() - pumpBStartTime) / 1000;
        pumpBRunning = false;
        pumpStateChanged = true;
        String message = "PPM within range. Pump B turned off after " + String(duration) + " seconds.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
    }
  }
  
  if (!adjustmentMade && ph_min > -4 && ph_limit > 0) {
    if (currentPh < ph_min) {
      if (!pumpCRunning) {
        digitalWrite(solutionCPin, HIGH);
        pumpCStartTime = millis();
        pumpCRunning = true;
        pumpStateChanged = true;
        String message = "pH below minimum (" + String(currentPh) + " < " + String(ph_min) + "). Activating Pump C (pH UP).";
        Serial.println(message);
        sendLogToFirebase(message);
      }
      
      if (pumpDRunning) {
        digitalWrite(solutionDPin, LOW);
        unsigned long duration = (millis() - pumpDStartTime) / 1000;
        pumpDRunning = false;
        pumpStateChanged = true;
        String message = "Turning off Pump D while adjusting pH upward.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
    } else if (currentPh > ph_limit) {
      if (!pumpDRunning) {
        digitalWrite(solutionDPin, HIGH);
        pumpDStartTime = millis();
        pumpDRunning = true;
        pumpStateChanged = true;
        String message = "pH above limit (" + String(currentPh) + " > " + String(ph_limit) + "). Activating Pump D (pH DOWN).";
        Serial.println(message);
        sendLogToFirebase(message);
      }
      
      if (pumpCRunning) {
        digitalWrite(solutionCPin, LOW);
        unsigned long duration = (millis() - pumpCStartTime) / 1000;
        pumpCRunning = false;
        pumpStateChanged = true;
        String message = "Turning off Pump C while adjusting pH downward.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
    } else {
      if (pumpCRunning) {
        digitalWrite(solutionCPin, LOW);
        unsigned long duration = (millis() - pumpCStartTime) / 1000;
        pumpCRunning = false;
        pumpStateChanged = true;
        String message = "pH within range. Pump C turned off after " + String(duration) + " seconds.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
      
      if (pumpDRunning) {
        digitalWrite(solutionDPin, LOW);
        unsigned long duration = (millis() - pumpDStartTime) / 1000;
        pumpDRunning = false;
        pumpStateChanged = true;
        String message = "pH within range. Pump D turned off after " + String(duration) + " seconds.";
        Serial.println(message);
        sendLogToFirebase(message);
      }
    }
  }
  
  if (pumpStateChanged) {
    updatePumpStatusInFirebase();
  }
}

void updateSensorsAndControl(bool performControl) {
  if (performControl) {
    digitalWrite(waterPumpPin, LOW);
    Serial.println("Water pump turned OFF for sensor checks");
    delay(1000);
  }
  
  Serial.println("Reading temperature sensor...");
  float tempValue = readTemperature();
  delay(500);
  
  Serial.println("Reading pH sensor...");
  float phValue = readPh();
  delay(500);
  
  Serial.println("Reading PPM/TDS sensor...");
  int ppmValue = readPpm();
  delay(500);
  
  Serial.println("Reading water level sensor...");
  bool waterLevelHigh = readFloatSwitch();
  
  temperature = tempValue;
  
  Serial.println("Sensor readings complete:");
  Serial.println("Temperature: " + String(tempValue) + "°C");
  Serial.println("pH: " + String(phValue));
  Serial.println("PPM: " + String(ppmValue));
  Serial.println("Water Level: " + String(waterLevelHigh ? "HIGH" : "LOW"));
  
  if (client.connect(FIREBASE_HOST, 443)) {
    StaticJsonDocument<200> doc;
    doc["temperature"] = tempValue;
    doc["pH"] = phValue;
    doc["PPM"] = ppmValue;
    doc["waterLevel"] = waterLevelHigh ? "HIGH" : "LOW";
    doc["timestamp"] = millis();
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    String path = "/sensors.json?auth=" + String(FIREBASE_AUTH);
    
    client.println("PUT " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonStr.length());
    client.println();
    client.println(jsonStr);
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }
    
    while (client.available()) {
      client.read();
    }
  }
  client.stop();
  
  if (performControl) {
    if (waterLevelHigh) {
      Serial.println("Water level is HIGH - Starting sequential checks and adjustments");
      checkAndAdjustLevels(phValue, ppmValue);
      
      digitalWrite(waterPumpPin, LOW);
      Serial.println("Water level HIGH - Water pump kept OFF");
    } else {
      Serial.println("Water level is LOW - Turning off all solution pumps for safety");
      
      if (pumpARunning || digitalRead(solutionAPin) == HIGH) {
        digitalWrite(solutionAPin, LOW);
        pumpARunning = false;
        Serial.println("Water level LOW - Pump A turned OFF for safety");
      }
      
      if (pumpBRunning || digitalRead(solutionBPin) == HIGH) {
        digitalWrite(solutionBPin, LOW);
        pumpBRunning = false;
        Serial.println("Water level LOW - Pump B turned OFF for safety");
      }
      
      if (pumpCRunning || digitalRead(solutionCPin) == HIGH) {
        digitalWrite(solutionCPin, LOW);
        pumpCRunning = false;
        Serial.println("Water level LOW - Pump C turned OFF for safety");
      }
      
      if (pumpDRunning || digitalRead(solutionDPin) == HIGH) {
        digitalWrite(solutionDPin, LOW);
        pumpDRunning = false;
        Serial.println("Water level LOW - Pump D turned OFF for safety");
      }
      
      digitalWrite(waterPumpPin, HIGH);
      Serial.println("Water level LOW - Water pump turned ON");
      
      updatePumpStatusInFirebase();
    }
  } else {
    if (waterLevelHigh) {
      digitalWrite(waterPumpPin, LOW);
    } else {
      digitalWrite(waterPumpPin, HIGH);
    }
  }
}

void readPumpCommandsFromFirebase() {
  if (client.connect(FIREBASE_HOST, 443)) {
    String path = "/pumps.json?auth=" + String(FIREBASE_AUTH);
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }

    String response = "";
    bool jsonStarted = false;
    String line = "";
    
    while (client.available()) {
      char c = client.read();
      
      if (c == '\r' && line.length() == 0) {
      } else if (c == '\n' && line.length() == 0) {
        jsonStarted = true;
        line = "";
      } else if (jsonStarted) {
        response += c;
      } else if (c == '\n') {
        line = "";
      } else {
        line += c;
      }
    }

    if (response.length() > 0) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        client.stop();
        return;
      }

      bool stateChanged = false;
      bool waterLevelHigh = readFloatSwitch();
      
      bool waterPumpManualControl = doc["water_pump_manual_control"].as<bool>();
      if (waterPumpManualControl) {
        bool waterPumpState = doc["water_pump"].as<bool>();
        if (digitalRead(waterPumpPin) != (waterPumpState ? HIGH : LOW)) {
          digitalWrite(waterPumpPin, waterPumpState ? HIGH : LOW);
          stateChanged = true;
          String message = "Water pump " + String(waterPumpState ? "activated" : "deactivated") + " via Firebase command (manual control)";
          Serial.println(message);
          sendLogToFirebase(message);
        }
      } else {
        if (doc["water_pump"].as<bool>() != (waterLevelHigh ? false : true)) {
          updatePumpStatusInFirebase();
        }
      }
      
      if (waterLevelHigh) {
        bool pumpAState = doc["pump_a"].as<bool>();
        if (digitalRead(solutionAPin) != (pumpAState ? HIGH : LOW) || 
            (pumpARunning && !pumpAState) || (!pumpARunning && pumpAState)) {
          digitalWrite(solutionAPin, pumpAState ? HIGH : LOW);
          stateChanged = true;
          if (pumpAState && !pumpARunning) {
            pumpARunning = true;
            pumpAStartTime = millis();
            String message = "Pump A activated via Firebase command";
            Serial.println(message);
            sendLogToFirebase(message);
          } else if (!pumpAState && pumpARunning) {
            unsigned long duration = (millis() - pumpAStartTime) / 1000;
            pumpARunning = false;
            String message = "Pump A deactivated via Firebase command after " + String(duration) + " seconds";
            Serial.println(message);
            sendLogToFirebase(message);
          }
        }
        
        bool pumpBState = doc["pump_b"].as<bool>();
        if (digitalRead(solutionBPin) != (pumpBState ? HIGH : LOW) || 
            (pumpBRunning && !pumpBState) || (!pumpBRunning && pumpBState)) {
          digitalWrite(solutionBPin, pumpBState ? HIGH : LOW);
          stateChanged = true;
          if (pumpBState && !pumpBRunning) {
            pumpBRunning = true;
            pumpBStartTime = millis();
            String message = "Pump B activated via Firebase command";
            Serial.println(message);
            sendLogToFirebase(message);
          } else if (!pumpBState && pumpBRunning) {
            unsigned long duration = (millis() - pumpBStartTime) / 1000;
            pumpBRunning = false;
            String message = "Pump B deactivated via Firebase command after " + String(duration) + " seconds";
            Serial.println(message);
            sendLogToFirebase(message);
          }
        }
        
        bool pumpCState = doc["pump_c"].as<bool>();
        if (digitalRead(solutionCPin) != (pumpCState ? HIGH : LOW) || 
            (pumpCRunning && !pumpCState) || (!pumpCRunning && pumpCState)) {
          digitalWrite(solutionCPin, pumpCState ? HIGH : LOW);
          stateChanged = true;
          if (pumpCState && !pumpCRunning) {
            pumpCRunning = true;
            pumpCStartTime = millis();
            String message = "Pump C (pH UP) activated via Firebase command";
            Serial.println(message);
            sendLogToFirebase(message);
            
            if (pumpDRunning) {
              digitalWrite(solutionDPin, LOW);
              unsigned long duration = (millis() - pumpDStartTime) / 1000;
              pumpDRunning = false;
              String message = "Pump D turned off automatically when Pump C was activated";
              Serial.println(message);
              sendLogToFirebase(message);
            }
          } else if (!pumpCState && pumpCRunning) {
            unsigned long duration = (millis() - pumpCStartTime) / 1000;
            pumpCRunning = false;
            String message = "Pump C deactivated via Firebase command after " + String(duration) + " seconds";
            Serial.println(message);
            sendLogToFirebase(message);
          }
        }
        
        bool pumpDState = doc["pump_d"].as<bool>();
        if (digitalRead(solutionDPin) != (pumpDState ? HIGH : LOW) || 
            (pumpDRunning && !pumpDState) || (!pumpDRunning && pumpDState)) {
          digitalWrite(solutionDPin, pumpDState ? HIGH : LOW);
          stateChanged = true;
          if (pumpDState && !pumpDRunning) {
            pumpDRunning = true;
            pumpDStartTime = millis();
            String message = "Pump D (pH DOWN) activated via Firebase command";
            Serial.println(message);
            sendLogToFirebase(message);
            
            if (pumpCRunning) {
              digitalWrite(solutionCPin, LOW);
              unsigned long duration = (millis() - pumpCStartTime) / 1000;
              pumpCRunning = false;
              String message = "Pump C turned off automatically when Pump D was activated";
              Serial.println(message);
              sendLogToFirebase(message);
            }
          } else if (!pumpDState && pumpDRunning) {
            unsigned long duration = (millis() - pumpDStartTime) / 1000;
            pumpDRunning = false;
            String message = "Pump D deactivated via Firebase command after " + String(duration) + " seconds";
            Serial.println(message);
            sendLogToFirebase(message);
          }
        }
      } else {
        bool anyPumpChanged = false;
        
        if (digitalRead(solutionAPin) == HIGH) {
          digitalWrite(solutionAPin, LOW);
          pumpARunning = false;
          anyPumpChanged = true;
        }
        
        if (digitalRead(solutionBPin) == HIGH) {
          digitalWrite(solutionBPin, LOW);
          pumpBRunning = false;
          anyPumpChanged = true;
        }
        
        if (digitalRead(solutionCPin) == HIGH) {
          digitalWrite(solutionCPin, LOW);
          pumpCRunning = false;
          anyPumpChanged = true;
        }
        
        if (digitalRead(solutionDPin) == HIGH) {
          digitalWrite(solutionDPin, LOW);
          pumpDRunning = false;
          anyPumpChanged = true;
        }
        
        if (anyPumpChanged) {
          String message = "Water level LOW - All solution pumps turned OFF for safety";
          Serial.println(message);
          sendLogToFirebase(message);
          stateChanged = true;
        }
      }
      
      if (stateChanged) {
        updatePumpStatusInFirebase();
      }
    }
  }
  client.stop();
}

void readConfigFromFirebase() {
  if (client.connect(FIREBASE_HOST, 443)) {
    String path = "/config.json?auth=" + String(FIREBASE_AUTH);
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }

    String response = "";
    bool jsonStarted = false;
    String line = "";
    
    while (client.available()) {
      char c = client.read();
      
      if (c == '\r' && line.length() == 0) {
      } else if (c == '\n' && line.length() == 0) {
        jsonStarted = true;
        line = "";
      } else if (jsonStarted) {
        response += c;
      } else if (c == '\n') {
        line = "";
      } else {
        line += c;
      }
    }

    if (response.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        return;
      }

      bool configChanged = false;
      
      if (doc.containsKey("ph_limit") && ph_limit != doc["ph_limit"].as<float>()) {
        ph_limit = doc["ph_limit"].as<float>();
        configChanged = true;
      }
      
      if (doc.containsKey("ppm_limit") && ppm_limit != doc["ppm_limit"].as<int>()) {
        ppm_limit = doc["ppm_limit"].as<int>();
        configChanged = true;
      }
      
      if (doc.containsKey("ph_min") && ph_min != doc["ph_min"].as<float>()) {
        ph_min = doc["ph_min"].as<float>();
        configChanged = true;
      }
      
      if (doc.containsKey("ppm_min") && ppm_min != doc["ppm_min"].as<int>()) {
        ppm_min = doc["ppm_min"].as<int>();
        configChanged = true;
      }
      
      if (doc.containsKey("scan_interval") && scan_interval != doc["scan_interval"].as<int>()) {
        scan_interval = doc["scan_interval"].as<int>();
        configChanged = true;
      }
      
      if (doc.containsKey("fail_safe") && fail_safe != doc["fail_safe"].as<int>()) {
        fail_safe = doc["fail_safe"].as<int>();
        configChanged = true;
      }
      
      if (configChanged) {
        String message = "Configuration updated: pH min=" + String(ph_min) + 
                        ", pH max=" + String(ph_limit) + 
                        ", PPM min=" + String(ppm_min) + 
                        ", PPM max=" + String(ppm_limit) + 
                        ", Scan interval=" + String(scan_interval) + " min";
        Serial.println(message);
        sendLogToFirebase(message);
      }
    }
  }
  client.stop();
}

void initializeConfigInFirebase() {
  if (client.connect(FIREBASE_HOST, 443)) {
    String path = "/config.json?auth=" + String(FIREBASE_AUTH);
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }

    String response = "";
    bool jsonStarted = false;
    String line = "";
    
    while (client.available()) {
      char c = client.read();
      
      if (c == '\r' && line.length() == 0) {
      } else if (c == '\n' && line.length() == 0) {
        jsonStarted = true;
        line = "";
      } else if (jsonStarted) {
        response += c;
      } else if (c == '\n') {
        line = "";
      } else {
        line += c;
      }
    }
    
    client.stop();
    
    if (response.length() > 0 && response != "null") {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (!error) {
        if (doc.containsKey("ph_limit")) ph_limit = doc["ph_limit"].as<float>();
        if (doc.containsKey("ppm_limit")) ppm_limit = doc["ppm_limit"].as<int>();
        if (doc.containsKey("ph_min")) ph_min = doc["ph_min"].as<float>();
        if (doc.containsKey("ppm_min")) ppm_min = doc["ppm_min"].as<int>();
        if (doc.containsKey("scan_interval")) scan_interval = doc["scan_interval"].as<int>();
        if (doc.containsKey("fail_safe")) fail_safe = doc["fail_safe"].as<int>();
        
        Serial.println("Loaded existing configuration from Firebase");
        return;
      }
    }
    
    if (client.connect(FIREBASE_HOST, 443)) {
      StaticJsonDocument<256> doc;
      doc["ph_limit"] = ph_limit;
      doc["ppm_limit"] = ppm_limit;
      doc["ph_min"] = ph_min;
      doc["ppm_min"] = ppm_min;
      doc["scan_interval"] = scan_interval;
      doc["fail_safe"] = fail_safe;
      
      String jsonStr;
      serializeJson(doc, jsonStr);
      
      client.println("PUT " + path + " HTTP/1.1");
      client.println("Host: " + String(FIREBASE_HOST));
      client.println("Connection: close");
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      client.println(jsonStr.length());
      client.println();
      client.println(jsonStr);
      
      timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          client.stop();
          return;
        }
      }
      
      while (client.available()) {
        client.read();
      }
      
      Serial.println("Configuration initialized in Firebase");
    }
  }
  client.stop();
}

void initializePumpControlsInFirebase() {
  if (client.connect(FIREBASE_HOST, 443)) {
    StaticJsonDocument<256> doc;
    doc["water_pump"] = false;
    doc["water_pump_manual_control"] = false;
    doc["pump_a"] = false;
    doc["pump_b"] = false;
    doc["pump_c"] = false;
    doc["pump_d"] = false;
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    String path = "/pumps.json?auth=" + String(FIREBASE_AUTH);
    
    client.println("PUT " + path + " HTTP/1.1");
    client.println("Host: " + String(FIREBASE_HOST));
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonStr.length());
    client.println();
    client.println(jsonStr);
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        client.stop();
        return;
      }
    }
    
    while (client.available()) {
      client.read();
    }
    
    Serial.println("Pump controls initialized in Firebase");
  }
  client.stop();
}

void setup() {
  Serial.begin(9600);
  
  pinMode(waterPumpPin, OUTPUT);
  pinMode(solutionAPin, OUTPUT);
  pinMode(solutionBPin, OUTPUT);
  pinMode(solutionCPin, OUTPUT);
  pinMode(solutionDPin, OUTPUT);
  
  pinMode(floaterSwitchPin, INPUT_PULLUP);
  pinMode(phSensorPin, INPUT);
  pinMode(tdsSensorPin, INPUT);
  
  sensors.begin();
  
  digitalWrite(waterPumpPin, LOW);
  digitalWrite(solutionAPin, LOW);
  digitalWrite(solutionBPin, LOW);
  digitalWrite(solutionCPin, LOW);
  digitalWrite(solutionDPin, LOW);
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  
  initializePumpControlsInFirebase();
  
  initializeConfigInFirebase();
  
  sendLogToFirebase("System started with default configuration: pH min=" + String(ph_min) + 
                   ", pH max=" + String(ph_limit) + ", PPM min=" + String(ppm_min) + 
                   ", PPM max=" + String(ppm_limit) + ", Scan interval=" + String(scan_interval) + " min");
  
  updateSensorsAndControl(true);
}

void loop() {
  static unsigned long lastConfigReadTime = 0;
  if (millis() - lastConfigReadTime >= 30000) {
    readConfigFromFirebase();
    lastConfigReadTime = millis();
  }
  
  static unsigned long lastSensorUpdateTime = 0;
  unsigned long sensorUpdateInterval = 5000;
  
  if (millis() - lastSensorUpdateTime >= sensorUpdateInterval) {
    updateSensorsAndControl(false);
    lastSensorUpdateTime = millis();
  }
  
  static unsigned long lastControlTime = 0;
  unsigned long currentSensorInterval = (scan_interval > 0) ? scan_interval * 60000 : sensorReadInterval;
  
  if (millis() - lastControlTime >= currentSensorInterval && 
      !pumpARunning && !pumpBRunning && !pumpCRunning && !pumpDRunning) {
    updateSensorsAndControl(true);
    lastControlTime = millis();
  }
  
  static unsigned long lastPumpCommandTime = 0;
  if (millis() - lastPumpCommandTime >= 2000) {
    readPumpCommandsFromFirebase();
    lastPumpCommandTime = millis();
  }
  
  unsigned long currentTime = millis();
  unsigned long maxPumpRunTime = 60000;
  
  if (pumpARunning && (currentTime - pumpAStartTime > maxPumpRunTime)) {
    digitalWrite(solutionAPin, LOW);
    pumpARunning = false;
    String message = "FAILSAFE: Pump A turned off after running for too long";
    Serial.println(message);
    sendLogToFirebase(message);
    updatePumpStatusInFirebase();
    delay(1000);
  }
  
  if (pumpBRunning && (currentTime - pumpBStartTime > maxPumpRunTime)) {
    digitalWrite(solutionBPin, LOW);
    pumpBRunning = false;
    String message = "FAILSAFE: Pump B turned off after running for too long";
    Serial.println(message);
    sendLogToFirebase(message);
    updatePumpStatusInFirebase();
    delay(1000);
  }
  
  if (pumpCRunning && (currentTime - pumpCStartTime > maxPumpRunTime)) {
    digitalWrite(solutionCPin, LOW);
    pumpCRunning = false;
    String message = "FAILSAFE: Pump C turned off after running for too long";
    Serial.println(message);
    sendLogToFirebase(message);
    updatePumpStatusInFirebase();
    delay(1000);
  }
  
  if (pumpDRunning && (currentTime - pumpDStartTime > maxPumpRunTime)) {
    digitalWrite(solutionDPin, LOW);
    pumpDRunning = false;
    String message = "FAILSAFE: Pump D turned off after running for too long";
    Serial.println(message);
    sendLogToFirebase(message);
    updatePumpStatusInFirebase();
    delay(1000);
  }
  
  delay(50);
  yield();
}
