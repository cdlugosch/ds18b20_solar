#include <DallasTemperature.h>
#include <OneWire.h>
#include "myEspLib.h"

float BatteryVoltage  = 0.0;
float batteryLevel    = 0.0;

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
