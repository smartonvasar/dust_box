#include "config.h"

#include <IoTGuru.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#endif

#ifdef ESP32
  #include <WiFi.h>
#endif

const char* ssid      = WIFI_SSID;
const char* password  = WIFI_PASSWORD;

String userShortId    = IOTGURU_USER_SHORT_ID;
String deviceShortId  = IOTGURU_DEVICE_SHORT_ID;
String deviceKey      = IOTGURU_DEVICE_KEY;

String sdsNodeShortId    = IOTGURU_SDS_NODE_SHORT_ID;
String bmeNodeShortId    = IOTGURU_BME_NODE_SHORT_ID;
String statusNodeShortId = IOTGURU_STATUS_NODE_SHORT_ID;

const char* ota_version = IOTGURU_OTA_VERSION;

IoTGuru iotGuru = IoTGuru(userShortId, deviceShortId, deviceKey);
WiFiClient client;
HTTPClient sensorCommunityClient;

volatile int PIN_SDA     = 4; // wemos D2
volatile int PIN_SCL     = 5; // wemos D1
volatile int PIN_SDS_RX  = 0; // wemos D3
volatile int PIN_SDS_TX  = 2; // wemos D4

#include "SdsDustSensor.h"
SdsDustSensor sds(PIN_SDS_RX, PIN_SDS_TX);

union unionForm {
  byte inBytes[2];
  unsigned int inInt;
} sdsDeviceId;

#include "Adafruit_BME280.h"
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

volatile double temp;
volatile double humidity;
volatile double pressure;
volatile double altitude;

void setup() {
    Serial.begin(115200);
    delay(10);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.println("wifi?");
    }
    Serial.println("wifi OK.");
    Serial.print("ChipId: ");
    Serial.println(String(ESP.getChipId()));

    /**
     * Set the callback function.
     */
    iotGuru.setCallback(&callback);
    /**
     * Set check in duration.
     */
    iotGuru.setCheckDuration(60000);
    /**
     * Set the debug printer (optional).
     */
    iotGuru.setDebugPrinter(&Serial);
    /**
     * Set the network client.
     */
    iotGuru.setNetworkClient(&client);
    /**
     * Check new firmware and try to update during the clean boot.
     */
    iotGuru.firmwareUpdate(ota_version);

    sds.begin();
    delay(10);
    sds.wakeup();
    delay(10);
    Serial.println(sds.queryFirmwareVersion().toString());
    Serial.println(sds.setQueryReportingMode().toString());

    if (! bme.begin(0x76))
    {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
    }
    sensorCommunityClient.setReuse(true);
}

volatile unsigned long nextWakeup = 0;
volatile unsigned long nextSend = 0;
volatile unsigned long readAttempt = 12;

void loop() {
    if (iotGuru.check(ota_version)) {
        Serial.println("Check ota version");
        ESP.restart();
    }

    iotGuru.loop();

    if (nextWakeup < millis()) {
        Serial.println("Wake up");

        temp = bme.readTemperature() / 1.0;
        humidity = bme.readHumidity() / 1.0;
        pressure = bme.readPressure() / 100.0;
        altitude = bme.readAltitude(SEALEVELPRESSURE_HPA) / 1000.0;

        Serial.print("Environmental measurement temperature = ");
        Serial.print(temp);
        Serial.print(", humidity = ");
        Serial.print(humidity);
        Serial.print(", pressure = ");
        Serial.print(pressure);
        Serial.print(", altitude = ");
        Serial.println(altitude);

        iotGuru.sendMqttValue(bmeNodeShortId, "temperature", temp);
        iotGuru.sendMqttValue(bmeNodeShortId, "humidity", humidity);
        iotGuru.sendMqttValue(bmeNodeShortId, "pressure", pressure);
        iotGuru.sendMqttValue(bmeNodeShortId, "altitude", altitude);

        reportBMEToSensorCommunity(String(temp), String(humidity), String(pressure));
        
        Serial.println("Turn on the SDS sensor");
        sds.wakeup();
        nextWakeup = millis() + 120000;
        nextSend = millis() + 30000;
        readAttempt = 12;
    }

    if (nextSend < millis()) {
        PmResult pm = sds.queryPm();
        if (readAttempt == 0) {
            nextSend = nextWakeup + 30000;
            readAttempt = 12;
            Serial.println("Turn off the SDS sensor");
            iotGuru.sendMqttValue(statusNodeShortId, "error", PMStatusToString(pm.statusToString()));
            sds.sleep();
            return;
        }

        nextSend = millis() + 2500;
        readAttempt--;

        if (pm.isOk()) {

            sdsDeviceId.inBytes[0] = pm.deviceId()[0];
            sdsDeviceId.inBytes[1] = pm.deviceId()[1];
            Serial.println("DeviceId: " + String(sdsDeviceId.inInt) + "(" + String(pm.deviceId()[0]) + "/" + String(pm.deviceId()[1]) + ")");

            Serial.print("Raw measurement PM2.5 = ");
            Serial.print(pm.pm25);
            Serial.print(", PM10 = ");
            Serial.println(pm.pm10);

            Serial.println("Compensate the dust measurement with the humidity");
            pm.pm25 = pm.pm25 / (1.0 + 0.48756 * pow((humidity / 100.0), 8.60068));
            pm.pm10 = pm.pm10 / (1.0 + 0.81559 * pow((humidity / 100.0), 5.83411));

            Serial.print("Compensated measurement PM2.5 = ");
            Serial.print(pm.pm25);
            Serial.print(", PM10 = ");
            Serial.println(pm.pm10);
         
            iotGuru.sendMqttValue(sdsNodeShortId, "pm25", pm.pm25);
            iotGuru.sendMqttValue(sdsNodeShortId, "pm10", pm.pm10);

            reportSDSToSensorCommunity(String(pm.pm10), String(pm.pm25));

            readAttempt = 0;
        } else {
            Serial.print("Could not read values from SDS sensor, reason: ");
            Serial.println(pm.statusToString());
        }
    }
}

void callback(const char* nodeShortId, const char* fieldName, const char* message) {
    Serial.print(nodeShortId);Serial.print(" - ");Serial.print(fieldName);Serial.print(": ");Serial.println(message);
}

int PMStatusToString(String status) {
  if (status == "Ok") {
    return 0;
  }
  if (status == "Not available") {
    return 101;
  }
  if (status == "Invalid checksum") {
    return 102;
  }
  if (status == "Invalid response id") {
    return 103;
  }
  if (status == "Invalid head") {
    return 104;
  }
  if (status == "Invalid tail") {
    return 105;
  }
  return 106;
}

void reportSDSToSensorCommunity(String pm10, String pm25) {
    client.connect("api.sensor.community", 80);
    
    Serial.print("[HTTP] SDS begin...\n");
    // configure target server and url
    sensorCommunityClient.begin(client, "http://api.sensor.community/v1/push-sensor-data/"); //HTTP
    sensorCommunityClient.addHeader("Content-Type", "application/json");
    // SDS
    sensorCommunityClient.addHeader("X-Pin", "1");
    sensorCommunityClient.addHeader("X-Sensor", "esp8266-" + String(ESP.getChipId()));
    sensorCommunityClient.setTimeout(12000);

    Serial.print("[HTTP] POST...\n");
    // start connection and send HTTP header and body
    String postData = "{\"sensor\":\"" + String(SENSORCOMMUNITY_SDS_SENSOR_ID) + "\",\"software_version\":\"" + String(IOTGURU_OTA_VERSION) + "\",\"sensordatavalues\":[{\"value_type\":\"P1\",\"value\":\"" + pm10 + "\"},{\"value_type\":\"P2\",\"value\":\"" + pm25 + "\"}]}";
    Serial.print("[HTTP] POST data: " + postData + "\n");

    int httpCode = sensorCommunityClient.POST(postData);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = sensorCommunityClient.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", sensorCommunityClient.errorToString(httpCode).c_str());
    }
    sensorCommunityClient.end();
}


void reportBMEToSensorCommunity(String temperature, String humidity, String pressure) {
    client.connect("api.sensor.community", 80);
    
    Serial.print("[HTTP] BME begin...\n");
    // configure target server and url
    sensorCommunityClient.begin(client, "http://api.sensor.community/v1/push-sensor-data/"); //HTTP
    sensorCommunityClient.addHeader("Content-Type", "application/json");
    // BME
    sensorCommunityClient.addHeader("X-Pin", "11");
    sensorCommunityClient.addHeader("X-Sensor", "esp8266-" + String(ESP.getChipId()));
    sensorCommunityClient.setTimeout(12000);

    Serial.print("[HTTP] POST...\n");
    // start connection and send HTTP header and body
    String postData = "{\"sensor\":\"" + String(SENSORCOMMUNITY_BME_SENSOR_ID) + "\",\"software_version\":\"" + String(IOTGURU_OTA_VERSION) + "\",\"sensordatavalues\":[{\"value_type\":\"temperature\",\"value\":\"" + temperature + "\"},{\"value_type\":\"humidity\",\"value\":\"" + humidity + "\"},{\"value_type\":\"pressure\",\"value\":\"" + pressure + "\"}]}";
    Serial.print("[HTTP] POST data: " + postData + "\n");

    int httpCode = sensorCommunityClient.POST(postData);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = sensorCommunityClient.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", sensorCommunityClient.errorToString(httpCode).c_str());
    }
    sensorCommunityClient.end();
}
