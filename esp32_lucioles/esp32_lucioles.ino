/*
 * Auteur : G.Menez
 */

#undef TLS_USE
#undef MQTT_CRED

// SPIFFS
#include <SPIFFS.h>
// OTA
#include <ArduinoOTA.h>
#include "ota.h"
// Capteurs
#include "OneWire.h"
#include "DallasTemperature.h"
// Wifi (TLS) https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure
#include <WiFi.h>
#include <WiFiClientSecure.h>
//JSON
#include <ArduinoJson.h>

#include "classic_setup.h"
// MQTT https://pubsubclient.knolleary.net/
#include <PubSubClient.h>


/*===== ESP GPIO configuration ==============*/
/* ---- LED         ----*/
const int led01 = 19; // LED will use GPIO pin 19
const int led02 = 23; // LED will use GPIO pin 23
/* ---- Light       ----*/
const int LightPin = A5; // Read analog input on ADC1_CHANNEL_5 (GPIO 33)
/* ---- Temperature ----*/
OneWire oneWire(23); // Pour utiliser une entite oneWire sur le port 23
DallasTemperature TempSensor(&oneWire) ; // Cette entite est utilisee par le capteur de temperature
/* ---- Localisation ----*/
const float lat = 22.396;
const float lgn = 114.10;
/* ---- Identification ----*/
const String IDENTIFIER = "esp_Wen_21800822";

char data[256];

String whoami; // Identification de CET ESP au sein de la flotte

/*===== JSON =================================*/
//StaticJsonBuffer<200> jsonBuffer;

/*===== WIFI =================================*/

#ifdef TLS_USE
WiFiClientSecure secureClient;     // Avec TLS !!!
#else
WiFiClient espClient;                // Use Wifi as link layer
#endif

/*===== MQTT broker/server and TOPICS ========*/
//String MQTT_SERVER = "192.168.1.101";
String MQTT_SERVER = "test.mosquitto.org";

#ifdef TLS_USE
int MQTT_PORT =  8883; // for TLS cf https://test.mosquitto.org/
#else
int MQTT_PORT = 1883;
#endif

//==== MQTT Credentials =========
#ifdef MQTT_CRED
char *mqtt_id     = "deathstar";
char *mqtt_login  = "darkvador";
char *mqtt_passwd = "6poD2R2";
#else
char *mqtt_id     = "wen";
char *mqtt_login  = NULL;
char *mqtt_passwd = NULL;
#endif

//==== MQTT TOPICS ==============
#define TOPIC_WLN "sensors/WLN"

#ifdef TLS_USE
PubSubClient client(secureClient); // MQTT client
#else
PubSubClient client(espClient);    // The ESP is a MQTT Client
#endif

void mqtt_pubcallback(char* topic, byte* message, unsigned int length) {
  /* 
   *  MQTT Callback ... if a message is published on this topic.
   */
  
  // Byte list to String ... plus facile a traiter ensuite !
  // Mais sans doute pas optimal en performance => heap ?
  String messageTemp ;
  for(int i = 0 ; i < length ; i++) {
    messageTemp += (char) message[i];
  }
  
  Serial.print("Message : ");
  Serial.println(messageTemp);
  Serial.print("arrived on topic : ");
  Serial.println(topic) ;
 
  // Analyse du message et Action 
  if(String (topic) == TOPIC_LED) {
     // Par exemple : Changes the LED output state according to the message   
    Serial.print("Action : Changing output to ");
    if(messageTemp == "on") {
      Serial.println("on");
      set_pin(led01,HIGH);
      set_pin(led02,LOW);
    } else if (messageTemp == "off") {
      Serial.println("off");
      set_pin(led01,LOW);
      set_pin(led02,HIGH);
    }
  }
}

void setup_mqtt_client() {
  /*
    Setup the MQTT client
  */

  // set server
  client.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
  // set callback when publishes arrive for the subscribed topic
  client.setCallback(mqtt_pubcallback);
}

void mqtt_connect() {
  /*
    Connection to a MQTT broker
  */

#ifdef TLS_USE
  // For TLS
  const char* cacrt = readFileFromSPIFFS("/ca.crt").c_str();
  secureClient.setCACert(cacrt);
  const char* clcrt = readFileFromSPIFFS("/client.crt").c_str();
  secureClient.setCertificate(clcrt);
  const char* clkey = readFileFromSPIFFS("/client.key").c_str();
  secureClient.setPrivateKey(clkey);
#endif
  
  while (!client.connected()) { // Loop until we're reconnected
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect => https://pubsubclient.knolleary.net/api
    if (client.connect(mqtt_id,      /* Client Id when connecting to the server */
                       mqtt_login,   /* With credential */
                       mqtt_passwd)) {
      Serial.println("connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());

      Serial.println(" try again in 5 seconds");
      delay(5000); // Wait 5 seconds before retrying
    }
  }
}

void mqtt_subscribe(char *topic) {
  /*
    Subscribe to a topic
  */
  if (!client.connected())
    mqtt_connect();
  
  client.subscribe(topic);
}


/*============= ACCESSEURS ====================*/

float get_temperature() {
  float temperature;
  TempSensor.requestTemperaturesByIndex(0);
  delay (750);
  temperature = TempSensor.getTempCByIndex(0);
  return temperature;
}

float get_light(){
  return analogRead(LightPin);
}

void set_pin(int pin, int val){
 digitalWrite(pin, val) ;
}

int get_pin(int pin){
  return digitalRead(pin);
}

/*=============== SETUP =====================*/

void setup () {

  Serial.begin(9600);
  while (!Serial); // wait for a serial connection. Needed for native USB port only

  // Connexion Wifi
  connect_wifi();
  print_network_status();

  //Choix d'une identification pour cet ESP
  whoami =  String(WiFi.macAddress());

  // Initialize the LED
  setup_led(led01, OUTPUT, LOW);
  setup_led(led02, OUTPUT, HIGH);

  // Init temperature sensor
  TempSensor.begin();

  // Initialize SPIFFS
  SPIFFS.begin(true);

  // MQTT broker connection
  setup_mqtt_client();
  mqtt_connect();

  /*------ Subscribe to TOPIC_LED  ---------*/
  mqtt_subscribe((char *)(TOPIC_LED));
}

//JSON

char* getJSONString_fromstatus(){
  /*
   * put all relevant data from esp in a "json formatted" String
   */
  StaticJsonDocument<1000> jsondoc;
  jsondoc["status"]["temperature"] = get_temperature();
  jsondoc["status"]["light"] = get_light();

  jsondoc["info"]["loc"] = getJSONString_fromlocation(lat, lgn);
  jsondoc["info"]["user"] = IDENTIFIER;
  jsondoc["info"]["ident"] = String(WiFi.macAddress());
  
  serializeJson(jsondoc, data);
  return data;
}

char* getJSONString_fromlocation(float lat, float lgn){
  StaticJsonDocument<1000> jsondoc;
  jsondoc["lat"] = lat;
  jsondoc["lgn"] = lgn;
  //char* data = "";
  //char data[256];
  serializeJson(jsondoc, data);
  return data;
}

/*================= LOOP ======================*/

void loop () {
  static uint32_t tick = 0;
  char data[256];
  String payload; // Payload : "JSON ready" 
  int32_t period = 6 * 1000l; // Publication period

  if ( millis() - tick < period)
  {
    goto END;
  }

  Serial.println("End of stand by period");
  tick = millis();

  /*------ Publish Temperature periodically ---*/
  payload = "{\"who\": \"";
  payload += whoami;   
  payload += "\", \"value\": " ;
  payload += get_temperature(); 
  payload += "}";
  payload.toCharArray(data, (payload.length() + 1)); // Convert String payload to a char array

  Serial.println(getJSONString_fromstatus());
  client.publish(TOPIC_WLN, getJSONString_fromstatus());  // publish it
  
  //Serial.println(getJSONString_fromstatus());
  //client.publish(TOPIC_TEMP, getJSONString_fromstatus());  // publish it 

  /*------ Publish Light periodically ---------*/
  //payload = "{\"who\": \"" + whoami + "\", \"value\": " + get_light() + "}";
  //payload.toCharArray(data, (payload.length() + 1));

  //Serial.println(data);
  //client.publish(TOPIC_LIGHT, data); // publish it 
  
END :  
  // Process MQTT ... obligatoire une fois par loop()
  client.loop(); 
}

/*========================================================*/
