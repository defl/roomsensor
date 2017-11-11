//
// Room Sensor for KNX
//
// MIT Licensed, (c) 2017 Dennis Fleurbaaij <mail@dennisfleurbaaij.com>
// Distributed as-is; no warranty is given.
//

#include <math.h>
#include <float.h>
#include <limits.h>

#include <avr/power.h>
#include <Wire.h>

#include <Max44009.h>  // https://github.com/RobTillaart/Arduino/tree/master/libraries/Max44009
#include <BME280I2C.h>  // https://github.com/finitespace/BME280/blob/master/src/
#include <SparkFunCCS811.h>  // https://github.com/sparkfun/CCS811_Air_Quality_Breakout/tree/master/Libraries/Arduino/src
#include <KonnektingDevice.h>  // https://github.com/KONNEKTING/KonnektingDeviceLibrary

#include "kdevice_RoomSensor.h"  // Auto generated by "KONNEKTING-CodeCreator.bat pro33_sensor_v2.kdevice.xml" 


// See KiCad schematic
static const byte PIN_LED = A0;  // PC0, output
static const byte PIN_CCS811_INTERRUPT = A1;  // PC1,PCINT9 input
static const byte PIN_CCS811_WAKE = A2;  // PC2, output
static const byte PIN_MAX44009_INTERRUPT = A3;  // PC3,PCINT11 input
static const byte PIN_SW1 = 2;  // PD2,PCINT18 input; Konnecting KNX programming button, must be on "external input" (PD2, PD3)
static const byte PIN_AS312_1_MOTION = 5;  // PD5,PCINT21 input
static const byte PIN_AS312_2_MOTION = 6;  // PD6,PCINT22 input

// I2C addresses, some depend on physical connections
static const byte I2C_CCS811 = 0x5A;
static const byte I2C_BMP280 = 0x76;
static const byte I2C_MAX44009 = 0x4A;

// Device limits according to data sheets
static const uint16_t CCS811_ECO2_MIN = 400;  // ppm
static const uint16_t CCS811_ECO2_MAX = 8192;
static const uint16_t CCS811_TVOC_MIN = 0;  // ppb
static const uint16_t CCS811_TVOC_MAX = 1187;
static const float    BME280_TEMP_MIN = -40;  // c
static const float    BME280_TEMP_MAX = 85;
static const float    BME280_HUM_MIN = 0;  // %
static const float    BME280_HUM_MAX = 100;
static const float    BME280_PRESS_MIN = 300;  // hPa
static const float    BME280_PRESS_MAX = 1100;
static const float    MAX44009_LUX_MIN = 0;  // Lux
static const float    MAX44009_LUX_MAX = 188000;

// Special constants used in the kdevice
static const uint8_t TRIGGERED_VALUE_DONT_SEND = 255;

// Error consts
static const uint8_t ERROR_CCS811 = 2;
static const uint8_t ERROR_BME280 = 3;
static const uint8_t ERROR_MAX44009 = 4;

// Globals
BME280I2C bme280;
CCS811 ccs811(I2C_CCS811);
Max44009 max44009(I2C_MAX44009);

struct Config {

  uint8_t  ledMode;
  uint32_t cyclicResendIntervalMs;
  
  uint8_t  as312TriggerMode;

  uint8_t  ccs811Mode;
  uint16_t ccs811Eco2DiffReportingThreshold;
  uint16_t ccs811Eco2MaxLimit;
  uint8_t  ccs811Eco2MaxTriggeredValue;
  uint16_t ccs811TvocDiffReportingThreshold;
  uint16_t ccs811TvocMaxLimit;
  uint8_t  ccs811TvocMaxTriggeredValue;
  
  uint32_t bme280PollingIntervalMs;
  float    bme280TempDiffReportingThreshold;
  float    bme280TempMinLimit;
  uint8_t  bme280TempMinTriggeredValue;
  float    bme280TempMaxLimit;
  uint8_t  bme280TempMaxTriggeredValue;
  float    bme280RhDiffReportingThreshold;
  float    bme280RhMinLimit;
  uint8_t  bme280RhMinTriggeredValue;
  float    bme280RhMaxLimit;
  uint8_t  bme280RhMaxTriggeredValue;
  float    bme280PressDiffReportingThreshold;

  float    max44009LuxDiffReportingThresholdPct;
} config;

// Interrupt variables
volatile bool readCcs881 = false;
volatile bool readMax44009 = false;

// Measurement state
uint16_t ccs881eCo2;
uint16_t ccs881TVoc;
float bme280Temp;
float bme280Rh;
float bme280Press;
float max44009Lux;
bool motion;

bool ccs881eCo2MeasurementValid = false;
bool ccs881TVocMeasurementValid = false;
bool bme280TempMeasurementValid = false;
bool bme280RhMeasurementValid = false;
bool bme280PressMeasurementValid = false;
bool max44009LuxMeasurementValid = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Blink led n times at human speed (blocking)
// Does not care about the current state of the led, might be confusing to the user. Too bad.
void blinkLed(short blinks, unsigned long delayMs=100)
{
  while(blinks>0)
  {
    digitalWrite(PIN_LED, HIGH);
    delay(delayMs);
    digitalWrite(PIN_LED, LOW);
    delay(delayMs);
    
    --blinks;
  }
}


// Max limit check and reporting
template<typename T> void checkMaxLimit(T value, T limit, int triggeredValue, uint8_t commObject)
{
  if(triggeredValue == TRIGGERED_VALUE_DONT_SEND)
    return;
  
  if (value >= limit)
    Knx.write(commObject, triggeredValue);
}

// Min limit check and reporting
template<typename T> void checkMinLimit(T value, T limit, int triggeredValue, uint8_t commObject)
{
  if(triggeredValue == TRIGGERED_VALUE_DONT_SEND)
    return;
  
  if (value <= limit)
    Knx.write(commObject, triggeredValue);
}


// Write if value differs from previousValue by at least reportingThreshold
// Sets previous value if threshold exeeded.
template<typename T> void writeIfDiff(T value, T* previousValue, T reportingThreshold, uint8_t commObject)
{
  if(abs(value - *previousValue) < reportingThreshold)
    return;
  
  Knx.write(commObject, value);
  *previousValue = value;
}


// KNX callback function, unused
void knxEvents(byte index)
{
}


// Interrupt handler for A0-A5 (Only handle: A1=PIN_CCS811_INTERRUPT, A3=PIN_MAX44009_INTERRUPT)
ISR (PCINT1_vect)
{
  if(digitalRead(PIN_CCS811_INTERRUPT) == LOW)
    readCcs881 = true;

  if(digitalRead(PIN_MAX44009_INTERRUPT) == LOW)
    readMax44009 = true;
}  


// Wake CCS881 chip
void ccs811Wake()
{
  digitalWrite(PIN_CCS811_WAKE, LOW); 
  delayMicroseconds(55);  // Need to wait at least 50 us before using
}


// Put CCS811 chip to sleep
void ccs811Sleep()
{
  digitalWrite(PIN_CCS811_WAKE, HIGH);
  delayMicroseconds(25);  // Need to be asleep for at least 20 us
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Setup function
void setup()
{
  blinkLed(1);

  // Power
  power_adc_disable(); // Analog to Digital Converter
  power_spi_disable(); // Serial Peripheral Interface
  power_timer1_disable();
  power_timer2_disable();
  
  // Pins
  // We assume default OUTPUT is low, for PIN_CCS811_WAKE that means CCS811 cpu is active and will respond to I2C bus
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_CCS811_INTERRUPT, INPUT_PULLUP);  // CCS811 will drive this low if data is ready
  pinMode(PIN_CCS811_WAKE, OUTPUT);
  pinMode(PIN_SW1, INPUT);
  pinMode(PIN_MAX44009_INTERRUPT, INPUT_PULLUP);  // MAX44009 will drive this low if data is ready
  pinMode(PIN_AS312_1_MOTION, INPUT);
  pinMode(PIN_AS312_2_MOTION, INPUT);

  // KNX
  // If this fails it'll reboot the chip which then calls setup() again making a loop.
  Konnekting.init(Serial, PIN_SW1, PIN_LED, MANUFACTURER_ID, DEVICE_ID, REVISION);

  // If device has been parametrized with KONNEKTING Suite, overwrite params from EEPROM.
  if (!Konnekting.isFactorySetting())
  {
    config.ledMode = Konnekting.getUINT8Param(PARAM_ledMode);
    config.cyclicResendIntervalMs = Konnekting.getUINT32Param(PARAM_cyclicResendIntervalMs);
  
    config.as312TriggerMode = Konnekting.getUINT8Param(PARAM_as312TriggerMode);
    
    config.ccs811Mode = Konnekting.getUINT8Param(PARAM_ccs811Mode);
    config.ccs811Eco2DiffReportingThreshold = Konnekting.getUINT16Param(PARAM_ccs811Eco2DiffReportingThreshold);
    config.ccs811Eco2MaxLimit = Konnekting.getUINT16Param(PARAM_ccs811Eco2MaxLimit);
    config.ccs811Eco2MaxTriggeredValue = Konnekting.getUINT8Param(PARAM_ccs811Eco2MaxTriggeredValue);
    config.ccs811TvocDiffReportingThreshold = Konnekting.getUINT16Param(PARAM_ccs811TvocDiffReportingThreshold);
    config.ccs811TvocMaxLimit = Konnekting.getUINT16Param(PARAM_ccs811TvocMaxLimit);
    config.ccs811TvocMaxTriggeredValue = Konnekting.getUINT8Param(PARAM_ccs811TvocMaxTriggeredValue);
    
    config.bme280PollingIntervalMs = Konnekting.getUINT32Param(PARAM_bme280PollingIntervalMs);
    config.bme280TempDiffReportingThreshold = static_cast<float>(Konnekting.getUINT8Param(PARAM_bme280TempDiffReportingThreshold));
    config.bme280TempMinLimit = static_cast<float>(Konnekting.getINT8Param(PARAM_bme280TempMinLimit));
    config.bme280TempMinTriggeredValue = Konnekting.getUINT8Param(PARAM_bme280TempMinTriggeredValue);
    config.bme280TempMaxLimit = static_cast<float>(Konnekting.getINT8Param(PARAM_bme280TempMaxLimit));
    config.bme280TempMaxTriggeredValue = Konnekting.getUINT8Param(PARAM_bme280TempMaxTriggeredValue);
    config.bme280RhDiffReportingThreshold = static_cast<float>(Konnekting.getUINT8Param(PARAM_bme280RhDiffReportingThreshold)) * 0.1f;
    config.bme280RhMinLimit = static_cast<float>(Konnekting.getUINT8Param(PARAM_bme280RhMinLimit));
    config.bme280RhMinTriggeredValue = Konnekting.getUINT8Param(PARAM_bme280RhMinTriggeredValue);
    config.bme280RhMaxLimit = static_cast<float>(Konnekting.getUINT8Param(PARAM_bme280RhMaxLimit));
    config.bme280RhMaxTriggeredValue = Konnekting.getUINT8Param(PARAM_bme280RhMaxTriggeredValue);
    config.bme280PressDiffReportingThreshold = static_cast<float>(Konnekting.getUINT8Param(PARAM_bme280PressDiffReportingThreshold));

    config.max44009LuxDiffReportingThresholdPct = static_cast<float>(Konnekting.getUINT8Param(PARAM_max44009LuxDiffReportingThresholdPct));
  }

  // I2C bus
  Wire.setClock(400000);

  // CSS811
  while(ccs811.begin() != CCS811Core::SENSOR_SUCCESS)
  {
    blinkLed(ERROR_CCS811);
    delay(1000);
  }
  ccs811.setDriveMode(config.ccs811Mode);
  ccs811.enableInterrupts();
  ccs811Sleep();

  // BME280
  while(!bme280.begin())
  {
    blinkLed(ERROR_BME280);
    delay(1000);
  }

  // MAX44009
  while(max44009.getError() != 0)
  {
    blinkLed(ERROR_MAX44009);
    delay(1000);
  }
  
  max44009.setHighThreshold(MAX44009_LUX_MIN);
  max44009.setLowThreshold(MAX44009_LUX_MAX);
  max44009.enableInterrupt();

  // Pin Chance Interrupts (http://gammon.com.au/interrupts)
  // Note that PIN_SW1, PD2 is not in PCI interrupt list, it's handled by KONNECTING
  PCMSK1 |= bit(PCINT9);  // PIN_CCS811_INTERRUPT, A1
  PCMSK1 |= bit(PCINT11);  // PIN_MAX44009_INTERRUPT, A3
  PCIFR  |= bit(PCIF1);  // Clear outstanding
  PCICR  |= bit(PCIE1);  // Enable pin change interrupts

  // Done, 2 slow blinks
  blinkLed(2, 500);
}


// Main loop
void loop()
{
  Knx.task();

  // Don't proceed until ready.
  if(!Konnekting.isReadyForApplication() || Konnekting.isProgState())
    return;

  const unsigned long nowMs = millis();

  // CCS881
  //
  if(readCcs881)
  { 
    ccs811Wake();
    ccs811.readAlgorithmResults();
    ccs811Sleep();

    ccs881eCo2 = ccs811.getCO2();
    ccs881eCo2MeasurementValid = ccs881eCo2 >= CCS811_ECO2_MIN && ccs881eCo2 <= CCS811_ECO2_MAX;
    if(ccs881eCo2MeasurementValid)
    {
      static uint16_t ccs881eCo2Previous = USHRT_MAX;
      
      writeIfDiff(ccs881eCo2, &ccs881eCo2Previous, config.ccs811Eco2DiffReportingThreshold, COMOBJ_eco2);
      
      checkMaxLimit(ccs881eCo2, config.ccs811Eco2MaxLimit, config.ccs811Eco2MaxTriggeredValue, COMOBJ_eco2MaxLimitReached);
    }
    
    ccs881TVoc = ccs811.getTVOC();
    ccs881TVocMeasurementValid = ccs881TVoc >= CCS811_TVOC_MIN && ccs881TVoc <= CCS811_TVOC_MAX;
    if(ccs881TVocMeasurementValid)
    {
      static uint16_t ccs881TVocPrevious = USHRT_MAX;
      
      writeIfDiff(ccs881TVoc, &ccs881TVocPrevious, config.ccs811TvocDiffReportingThreshold, COMOBJ_tvoc);
      
      checkMaxLimit(ccs881TVoc, config.ccs811TvocMaxLimit, config.ccs811TvocMaxTriggeredValue, COMOBJ_tvocMaxLimitReached);
    }

    if(!ccs881eCo2MeasurementValid || !ccs881TVocMeasurementValid)
      blinkLed(ERROR_CCS811);

    readCcs881 = false;
  }

  // BME280
  //
  static unsigned long bme280PreviousPollingTimestampMs = 0;

  if(nowMs - bme280PreviousPollingTimestampMs >= config.bme280PollingIntervalMs)
  {
    bme280.read(bme280Press, bme280Temp, bme280Rh, BME280::TempUnit_Celcius, BME280::PresUnit_hPa);

    bme280TempMeasurementValid = bme280Temp >= BME280_TEMP_MIN && bme280Temp <= BME280_TEMP_MAX;
    if(bme280TempMeasurementValid)
    {
      static float bme280TempPrevious = FLT_MAX;
         
      writeIfDiff(bme280Temp, &bme280TempPrevious, config.bme280TempDiffReportingThreshold, COMOBJ_temp);
      
      checkMinLimit(bme280Temp, config.bme280TempMinLimit, config.bme280TempMinTriggeredValue, COMOBJ_tempMinLimitReached);
      checkMaxLimit(bme280Temp, config.bme280TempMaxLimit, config.bme280TempMaxTriggeredValue, COMOBJ_tempMaxLimitReached);
    }
    
    bme280RhMeasurementValid = bme280Rh >= BME280_HUM_MIN && bme280Rh <= BME280_HUM_MAX;
    if(bme280RhMeasurementValid)
    {
      static float bme280RhPrevious = FLT_MAX;
      
      writeIfDiff(bme280Rh, &bme280RhPrevious, config.bme280RhDiffReportingThreshold, COMOBJ_rh);

      checkMinLimit(bme280Rh, config.bme280RhMinLimit, config.bme280RhMinTriggeredValue, COMOBJ_rhMinLimitReached);
      checkMaxLimit(bme280Rh, config.bme280RhMaxLimit, config.bme280RhMaxTriggeredValue, COMOBJ_rhMaxLimitReached);
    }
    
    bme280PressMeasurementValid = bme280Press >= BME280_PRESS_MIN && bme280Press <= BME280_PRESS_MAX;
    if(bme280PressMeasurementValid)
    {
      static float bme280PressPrevious = FLT_MAX;
      
      writeIfDiff(bme280Press, &bme280PressPrevious, config.bme280PressDiffReportingThreshold, COMOBJ_press);
    }
  
    // Update CCS811 if temp/hum valid and significantly differs from last uploaded
    if(bme280TempMeasurementValid && bme280RhMeasurementValid)
    {
      static float ccs811SetTemp = FLT_MAX;
      static float ccs811SetRh = FLT_MAX;
      if(abs(ccs811SetTemp - bme280Temp) > 0.5 ||
         abs(ccs811SetRh - bme280Rh) > 0.5)
      {
        ccs811Wake();
        ccs811.setEnvironmentalData(bme280Rh, bme280Temp);
        ccs811Sleep();

        ccs811SetTemp = bme280Temp;
        ccs811SetRh = bme280Rh;
      }
    }

    if(!bme280TempMeasurementValid || !bme280PressMeasurementValid || !bme280PressMeasurementValid)
      blinkLed(ERROR_BME280);

    bme280PreviousPollingTimestampMs = nowMs;
  }

  // MAX44009
  //
  if(readMax44009)
  {
    max44009Lux = max44009.getLux();

    max44009LuxMeasurementValid = max44009Lux >= MAX44009_LUX_MIN && max44009Lux <= MAX44009_LUX_MAX;
    if(max44009LuxMeasurementValid)
    {
      static float max44009LuxPrevious = FLT_MAX;
      
      const float reportingThreshold = max44009LuxPrevious * config.max44009LuxDiffReportingThresholdPct;
      writeIfDiff(max44009Lux, &max44009LuxPrevious, reportingThreshold, COMOBJ_lux);

      const float factor = 1.0 + config.max44009LuxDiffReportingThresholdPct;
      const float highThreshold = max44009Lux * factor;
      const float lowThreshold = max44009Lux / factor;
  
      max44009.setHighThreshold(min(highThreshold, MAX44009_LUX_MAX));
      max44009.setLowThreshold(max(lowThreshold, MAX44009_LUX_MIN));
    }
    else
      blinkLed(ERROR_MAX44009);

    readMax44009 = false;
  }

  // AS312
  //
  static bool motionPrevious = false;
  {
    const bool as312Motion1 = (digitalRead(PIN_AS312_1_MOTION) == HIGH);
    const bool as312Motion2 = (digitalRead(PIN_AS312_2_MOTION) == HIGH);
  
    switch(config.as312TriggerMode)
    {
      case 0: // Sensor 1 only
        motion = as312Motion1;
        break;
  
      case 1: // Sensor 2 only
        motion = as312Motion2;
        break;
  
      case 2: // Either sensor
        motion = as312Motion1 || as312Motion2;
        break;
  
      case 3: // Both sensors
        motion = as312Motion1 && as312Motion2;
        break;
  
      case 4: // Off
        motion = false;
        break;
    }
  }
  
  if(motion != motionPrevious)
  {
    if(motion)
      Knx.write(COMOBJ_motion, 1);
    else
      Knx.write(COMOBJ_motion, 0);

    if(config.ledMode == 2) // On while motion detected
      digitalWrite(PIN_LED, motion ? HIGH : LOW);

    motionPrevious = motion;
  }

  // Cyclic send
  static unsigned long previousCyclicResendTimestampMs = 0;
  if(config.cyclicResendIntervalMs > 0 && 
     nowMs - previousCyclicResendTimestampMs >= config.cyclicResendIntervalMs)
  {
    Knx.write(COMOBJ_motion, motion);

    if(ccs881eCo2MeasurementValid)
      Knx.write(COMOBJ_eco2, ccs881eCo2);
    
    if(ccs881TVocMeasurementValid)
      Knx.write(COMOBJ_tvoc, ccs881TVoc);
    
    if(bme280TempMeasurementValid)
      Knx.write(COMOBJ_temp, bme280Temp);
      
    if(bme280RhMeasurementValid)
      Knx.write(COMOBJ_rh, bme280Rh);
      
    if(bme280PressMeasurementValid)
      Knx.write(COMOBJ_press, bme280Press);
      
    if(max44009LuxMeasurementValid)
      Knx.write(COMOBJ_lux, max44009Lux);

    previousCyclicResendTimestampMs = nowMs;
  }
}

