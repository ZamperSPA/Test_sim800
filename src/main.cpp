#include <Arduino.h>

#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define TINY_GSM_DEBUG SerialMon
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define TINY_GSM_USE_GPRS true
// #define DUMP_AT_COMMANDS
#define LED_PIN 13

#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <StreamDebugger.h>

StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);

TinyGsmClient client(modem);
PubSubClient  mqtt(client);


// Your GPRS credentials, if any
const char apn[]      = "m2m.movistar.cl";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
const char* broker = "broker.hivemq.com";

const char* topicLed       = "zamper/led";
const char* topicInit      = "zamper/init";
const char* topicLedStatus = "zamper/ledStatus";


int ledStatus = LOW;

uint32_t lastReconnectAttempt = 0;

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus);
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.print(broker);

  // Connect to MQTT Broker
  boolean status = mqtt.connect("GsmClientTest");

  // Or, if you want to authenticate MQTT:
  // boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

  if (status == false) {
    SerialMon.println(" fail");
    return false;
  }
  SerialMon.println(" success");
  mqtt.publish(topicInit, "GsmClientTest started");
  mqtt.subscribe(topicLed);
  return mqtt.connected();
}


void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  // !!!!!!!!!!!
  // Set your reset, enable, power pins here
  // !!!!!!!!!!!

  SerialMon.println("Wait...");

  // Set GSM module baud rate
  //TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  SerialAT.begin(115200);

  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  //modem.restart();
  modem.init();
  modem.setBaud(9600);
  SerialAT.begin(9600);
  modem.testAT();
  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) { SerialMon.println("Network connected"); }


  // GPRS connection parameters are usually set after network registration
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) { SerialMon.println("GPRS connected"); }


  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  // Make sure we're still registered on the network
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Network disconnected");
    if (!modem.waitForNetwork(180000L, true)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    if (modem.isNetworkConnected()) {
      SerialMon.println("Network re-connected");
    }

#if TINY_GSM_USE_GPRS
    // and make sure GPRS/EPS is still connected
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS disconnected!");
      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
      }
      if (modem.isGprsConnected()) { SerialMon.println("GPRS reconnected"); }
    }
#endif
  }

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) { lastReconnectAttempt = 0; }
    }
    delay(100);
    return;
  }

  mqtt.loop();
}
