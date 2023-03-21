#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

/* defined in config.h */
/*
// WiFi 
const char* ssid            = "ssid";
const char* WiFi_password   = "pass";

// MQTT 
const char* mqttServer      = "1.2.3.4";
const int mqttPort          = 1883;
const char* mqtt_user       = "";
const char* mqtt_password   = "";
const char* topic_root      = "project/";
const char* mqtt_ClientName = "ESP32Client_4_Project";
*/ 

int max_network_iteration = 2;
int network_iteration_index = 0;
int increase_sleep_at_low_voltage_factor = 1;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/* ############################ */
void connectNetwork(){

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to Wifi network, SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
 
  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, WiFi_password);
  }  

  while ((WiFi.status() != WL_CONNECTED) && (network_iteration_index < max_network_iteration)) {
    Serial.print('.');
    network_iteration_index += 1;
    delay(500);    // wait a few seconds for connection:
  }  
  /* DEBUG MESSAGES */  
  switch(WiFi.status()) {
    case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
    case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
    case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
    case WL_CONNECTED: 
      Serial.println("WL_CONNECTED - Connected to the WiFi network");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());      
      break;
    case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
    case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
    case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
    case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;

    default: Serial.println("UNKNOWN WL STATUS:" + WiFi.status()); break;
  }  

  if(WiFi.status() == WL_CONNECTED){
    mqttClient.setServer(mqttServer, mqttPort);
    
    network_iteration_index=0;

    while ((!mqttClient.connected()) && (network_iteration_index < max_network_iteration)) {
        Serial.println("Connecting to MQTT...");
   
        if (mqttClient.connect(mqtt_ClientName, mqtt_user, mqtt_password )) {
   
          Serial.println("MQTT connected");  
          network_iteration_index+=max_network_iteration;

        } else {
   
          Serial.print("MQTT failed with state ");
          Serial.print(mqttClient.state());
          Serial.print(".");
          
          delay(2000);
   
        }
        network_iteration_index+=1;
    }
  }
}