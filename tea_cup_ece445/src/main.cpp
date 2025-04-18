#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

extern "C" {
   void app_main(void);
}

static const int oneWireBus = 17;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  Serial.print("Number of sensors = ");
  Serial.println(sensors.getDeviceCount());
  Serial.print("Parasite mode = ");
  Serial.println(sensors.isParasitePowerMode());
  // sensors.isParasitePowerMode(true);
}

void loop() {
  delay(1000);
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);
  Serial.print(temperatureC);
  Serial.println(" C");
  Serial.print(temperatureF);
  Serial.println(" F");
  // delay(5000);
}

void app_main(void) {
  initArduino();
  setup();
  while (true) {
    loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}