#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT_U.h>
#include <ArduinoJson.h>

#define RXp1 16
#define TXp1 17


float Humedad;
// WiFi
const char *ssid = "..."; // Enter your Wi-Fi name
const char *password = "...";  // Enter Wi-Fi password

// MQTT Broker
const char *mqtt_broker = "broker.hivemq.com";
const char *topic = "Incubadoras/Incubadora1";
const char *clientID = "7ubLLYZWdcg8uX";
//const char *mqtt_username = "aguscosta";
//const char *mqtt_password = "1234";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    // Set software serial baud to 115200;
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXp1, TXp1);
    
    // Connecting to a WiFi network
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the Wi-Fi network");
    Serial.println(WiFi.localIP());
    //connecting to a mqtt broker
    client.setServer(mqtt_broker, mqtt_port);
    //client.setCallback(callback);
    while (!client.connected()) {
        String client_id = clientID;
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Public EMQX MQTT broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }
    // Publish and subscribe
    client.publish(topic, "Hi, I'm ESP32 ^^");
    client.subscribe(topic);
}

/*
void callback(char *topic, byte *data, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) data[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}
*/
void loop() {
  // Leer la cadena JSON recibida desde el Arduino Mega
  String jsonString = Serial2.readStringUntil('\n');

  // Publicar la cadena JSON en un topic MQTT
  if (jsonString!=""){
    client.publish("Incubadoras/Incubadora1", jsonString.c_str());
    Serial.println(jsonString);
  }
  Serial.println("Publicado");
  client.loop();
}
