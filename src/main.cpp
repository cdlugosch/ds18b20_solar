#include <DallasTemperature.h>
#include <OneWire.h>
#include "myEspLib.h"

float BatteryVoltage  = 0.0;
int increase_sleep_at_low_voltage_factor = 1;


# define ONE_WIRE_BUS 21
# define sensor_pin 5
# define analog_voltage_pin 35

char topic[100]; /* mqtt publish topic - string concat in c is a mess */

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

  float temperature = -127.0;
  float t = 0.0;

  unsigned long starttime = millis();
  int count = 0;

  if(CountSensors>0){
    owSensors.requestTemperatures(); // Send the command to get temperatures

    while((count<50) && ((starttime +10000)>millis())){
      temperature = owSensors.getTempCByIndex(0);
      /* sometimes the returned value are not in a realistic range (for Central Europe 2023 ;) ) - simply drop them */
      if(temperature>-30 && temperature<70){
        t += temperature;
        count+=1;
      }
      delay(100);
    }
    if(index>0){
      temperature = t/count;
    }else{}
  }

  return temperature;
}

float getVoltage(int pin)
{
  /* Read voltage a few times and return mean value, as a single read not accurate */
  float adc_analogValue = 0.0;
  float Voltage = 0.0;

  int count = 0;
  while(count<=20){
    adc_analogValue += analogRead(pin);
    count +=1;
    delay(100);
  }

  if(count>0){
    char adc_mqtt_payload[10];
    dtostrf(adc_analogValue/count, 2, 0, adc_mqtt_payload);  
    mqttClient.publish("esp/sensor/ds18b20_0_adc", adc_mqtt_payload);  


    Voltage = adc_analogValue/count; 
    Voltage = Voltage/185;  /* measured multiplier based on real voltage divider */



    if(Voltage <= 3.0 || Voltage > 4.5) // Reading was a failure due to weak connectors or reading - ESP does not work anymore at that voltage
      Voltage = 0.0;    
    
  }


 return Voltage; 
}
  
/* ############################ */

void setup()
{  
  Serial.begin(115200);

  /* Initialze Pins */
  pinMode(sensor_pin, OUTPUT);
  pinMode(analog_voltage_pin, INPUT);
  digitalWrite(sensor_pin, HIGH);

  /* try a few times to connecto to WiFi */
  if(connectNetwork()){

    /* Read Temperature from ds18b20 */
    float temperature = get_ds18b20_Temp();
    if(temperature > -30){
      char ds18b20_tempString[8];

      dtostrf(get_ds18b20_Temp(), 1, 2, ds18b20_tempString);
      Serial.print("Temperature for the device 1 (index 0) is: ");
      Serial.println(ds18b20_tempString); 
      /*strncpy(topic,topic_root,sizeof(topic_root));
      strncat(topic, "ds18b20_0",sizeof("ds18b20_0"));*/
      mqttClient.publish("esp/sensor/ds18b20_0", ds18b20_tempString);
      mqttClient.publish("esp/sensor/ds18b20_0_temp_read_failure", "false");

      Serial.println("de-activate sensor to save battery ");
      digitalWrite(sensor_pin, LOW);
    }else{
      mqttClient.publish("esp/sensor/ds18b20_0_temp_read_failure", "true");
    }


    /* Voltage Monitor */
    BatteryVoltage = getVoltage(analog_voltage_pin);
    Serial.print("BatteryVoltage : ");
    Serial.println(BatteryVoltage);

    if(BatteryVoltage>0.0){
      char BatteryVoltage_mqtt_payload[10];
      dtostrf(BatteryVoltage, 1, 2, BatteryVoltage_mqtt_payload);  
      mqttClient.publish("esp/sensor/ds18b20_0_voltage", BatteryVoltage_mqtt_payload);  
      mqttClient.publish("esp/sensor/ds18b20_0_voltage_read_error", "false");  

      /*increase sleep time in case battery voltage is low */
      if(BatteryVoltage < 3.9 && BatteryVoltage > 0)
        increase_sleep_at_low_voltage_factor = 2;
        
      if(BatteryVoltage < 3.7 && BatteryVoltage > 0)
        increase_sleep_at_low_voltage_factor = 3;
        
      if(BatteryVoltage < 3.5 && BatteryVoltage > 0)
        increase_sleep_at_low_voltage_factor = 4;       

      if(BatteryVoltage < 3.4 && BatteryVoltage > 0) 
        increase_sleep_at_low_voltage_factor = 6;  



    }else{
      mqttClient.publish("esp/sensor/ds18b20_0_voltage_read_error", "true");  
    }
    /* Log/Publish DeepSleep Time */   
    char DeepSleepTime_mqtt_payload[10];
    dtostrf((Time_to_sleep * increase_sleep_at_low_voltage_factor )/60, 1, 0, DeepSleepTime_mqtt_payload);  
    Serial.print("DeepSleepMinutes : ");
    Serial.println(DeepSleepTime_mqtt_payload);             
    mqttClient.publish("esp/sensor/ds18b20_0_DeepSleepMinutes", DeepSleepTime_mqtt_payload);  

  }

    /* Wait a little bit to make sure publish is finished*/
  delay(3000);
  Serial.println("Done - activating deepsleep mode");
  Time_to_sleep = Time_to_sleep * increase_sleep_at_low_voltage_factor * uS_TO_S_FACTOR;
  esp_sleep_enable_timer_wakeup(Time_to_sleep);
  Serial.flush(); 
  esp_deep_sleep_start();
  
}


 
void loop()
{
}
