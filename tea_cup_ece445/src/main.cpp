#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  void gpio_console_start(void);   // from gpio_console.c
}

static const int oneWireBus = 17;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

/*----------------------------------------------------------------------
 * Arduino‑style setup / loop
 *----------------------------------------------------------------------*/
void setup()
{
  Serial.begin(115200);

  sensors.begin();
  Serial.print("Number of sensors = ");
  Serial.println(sensors.getDeviceCount());
  Serial.print("Parasite mode = ");
  Serial.println(sensors.isParasitePowerMode());

  Serial.println("Starting CLI …");
  gpio_console_start();           // spawns CLI task, returns immediately
}

void loop()
{
  delay(1000);

  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  Serial.printf("%.2f °C\n", temperatureC);
  Serial.printf("%.2f °F\n", temperatureF);
}

/*----------------------------------------------------------------------
 * Single ESP‑IDF entry‑point
 *----------------------------------------------------------------------*/
extern "C" void app_main(void)
{
  initArduino();                  // initialise Arduino core
  setup();
  for (;;)
  {
    loop();
    vTaskDelay(pdMS_TO_TICKS(1)); // yield to other tasks
  }
}

extern "C" float get_latest_temp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}