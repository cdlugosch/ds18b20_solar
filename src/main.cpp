#include <WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <OneWire.h>
 
const char* ssid        = "Renoir";
const char* password    = "oyn4Feb9Om3yOb";
 
const char* mqttServer  = "10.120.40.25";
const int mqttPort      = 1883;
const char* mqttClientName  = "ESP32Client_Garden_Temperature_ds18b20";

int max_network_iteration = 2;
int network_iteration_index = 0;
int increase_sleep_at_low_voltage_factor = 1;

float BatteryVoltage  = 0.0;
float batteryLevel    = 0.0;


WiFiClient espClient;
PubSubClient mqttClient(espClient);

# define ONE_WIRE_BUS 21
# define sensor_pin 5
# define analog_voltage_pin 35


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
uint64_t Time_to_sleep = 300;   /* Time ESP32 will go to sleep (in seconds) */


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature owSensors(&oneWire);

/* ############################ */
void connectNetwork(){

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to Wifi network, SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
 
  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, password);
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
   
        if (mqttClient.connect(mqttClientName, "", "" )) {
   
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
/* ############################ */
float get_ds18b20_Temp()
{
  owSensors.begin(); 
  int CountSensors = owSensors.getDeviceCount();
  Serial.print("Number of Sensors attached to ESP device:");
  Serial.println(CountSensors);

  float temperature = 0.0;
  float t = 0.0;

  unsigned long starttime = millis();
  int index = 0;

  if(CountSensors>0){
    owSensors.requestTemperatures(); // Send the command to get temperatures
    //delay(500);

    while((index<5) && ((starttime +10000)>millis())){
      temperature = owSensors.getTempCByIndex(0);
      /* sometimes the returned value is approx 85 or -127 - maybe the sensor is not fast enough */
      if(temperature>-20 && temperature<70){
        t += temperature;
        index+=1;
      }
    }
    if(index>0){
      temperature = t/index;
    }
  }

  return temperature;
}

float getVoltage(int pin)
{
  /* Read voltage a few times and return mean value, as a single read not accurate */
  float batteryLevel = 0.0;
  int count = 0;
  boolean done = false;
  while(count<=100){
    batteryLevel += analogRead(pin);
    count +=1;
  }
 return batteryLevel/count; 
}
  
/* ############################ */

void setup()
{  
  Serial.begin(115200);

  /* Initialze Pins */
  pinMode(sensor_pin, OUTPUT);
  pinMode(analog_voltage_pin, INPUT);
  digitalWrite(sensor_pin, HIGH);

  connectNetwork();

  if(WiFi.status() == WL_CONNECTED && mqttClient.connected()){

    /* Read Temperature from ds18b20 */
    char ds18b20_tempString[8];
    dtostrf(get_ds18b20_Temp(), 1, 2, ds18b20_tempString);
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(ds18b20_tempString);  
    mqttClient.publish("esp/sensor/ds18b20_0", ds18b20_tempString);

    Serial.println("de-activate sensor to save battery ");
    digitalWrite(sensor_pin, LOW);

    /* Voltage Monitor */
    batteryLevel = getVoltage(analog_voltage_pin);
    Serial.print("Voltage : ");
    Serial.println(batteryLevel);

    BatteryVoltage = map(batteryLevel,0,710,0,100);
    BatteryVoltage = BatteryVoltage * 4.2/100*0.905;  // try to get the correct voltage, as voltage divider is a mess + multiply by 0.78

    /*increase sleep time in case battery voltage is low */
    if(BatteryVoltage < 3.9)
      increase_sleep_at_low_voltage_factor = 2;
      
    if(BatteryVoltage < 3.7)
      increase_sleep_at_low_voltage_factor = 3;
      
    if(BatteryVoltage < 3.5)
      increase_sleep_at_low_voltage_factor = 4;       


    char tempString[8];
    dtostrf(BatteryVoltage, 1, 2, tempString);
    mqttClient.publish("esp/sensor/ds18b20_0_voltage", tempString);

    /* Wait a little bit to make sure publish is finished*/
    delay(3000);
    Serial.println("Done - activating deepsleep mode");
    Time_to_sleep = Time_to_sleep * increase_sleep_at_low_voltage_factor * uS_TO_S_FACTOR;
    esp_sleep_enable_timer_wakeup(Time_to_sleep);
    Serial.flush(); 
    esp_deep_sleep_start();
  }
  
}


 
void loop()
{
}
