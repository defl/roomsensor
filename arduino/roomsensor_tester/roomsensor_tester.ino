//
// Room Sensor board tester
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


// Globals
BME280I2C bme280;
CCS811 ccs811(I2C_CCS811);
Max44009 max44009(I2C_MAX44009);

// Interrupt variables
volatile bool readCcs881 = false;
volatile bool readMax44009 = false;

static unsigned long BME280_POLLING_INTERVAL_MS = 10000;


//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Interrupt handler for A0-A5 (Only handle: A1=PIN_CCS811_INTERRUPT, A3=PIN_MAX44009_INTERRUPT)
ISR (PCINT1_vect)
{
  if(digitalRead(PIN_CCS811_INTERRUPT) == LOW)
  {
    readCcs881 = true;
    Serial.println("! PIN_CCS811_INTERRUPT triggered");
  }

  if(digitalRead(PIN_MAX44009_INTERRUPT) == LOW)
  {
    readMax44009 = true;
    Serial.println("! PIN_MAX44009_INTERRUPT triggered");
  }
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
  Serial.begin(19200);
  Serial.println("Initialized serial");
  
  // Power
  power_adc_disable(); // Analog to Digital Converter
  power_spi_disable(); // Serial Peripheral Interface
  power_timer1_disable();
  power_timer2_disable();
  Serial.println("Power done");
  
  // Pins
  // We assume default OUTPUT is low, for PIN_CCS811_WAKE that means CCS811 cpu is active and will respond to I2C bus
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_CCS811_INTERRUPT, INPUT_PULLUP);  // CCS811 will drive this low if data is ready
  pinMode(PIN_CCS811_WAKE, OUTPUT);
  pinMode(PIN_SW1, INPUT);
  pinMode(PIN_MAX44009_INTERRUPT, INPUT_PULLUP);  // MAX44009 will drive this low if data is ready
  pinMode(PIN_AS312_1_MOTION, INPUT);
  pinMode(PIN_AS312_2_MOTION, INPUT);
  Serial.println("Pinmode done");

  // I2C bus
  Wire.setClock(400000);
  Serial.println("I2C done");

  // CSS811
  while(ccs811.begin() != CCS811Core::SENSOR_SUCCESS)
  {
    Serial.println("ccs811.begin() failed");
    delay(1000);
  }
  ccs811.setDriveMode(2); // Mode 2 = 10 seconds pulse
  ccs811.enableInterrupts();
  ccs811Sleep();
  Serial.println("CCS811 done");

  // BME280
  while(!bme280.begin())
  {
    Serial.println("bme280.begin() failed");
    delay(1000);
  }
  Serial.println("BME280 done");

  // MAX44009
  while(max44009.getError() != 0)
  {
    Serial.println("max44009.getError() failed");
    delay(1000);
  }
  const float lux = max44009.getLux();
  max44009.setHighThreshold(lux+1);
  max44009.setLowThreshold(lux-1);
  max44009.enableInterrupt();
  Serial.print("MAX44009 initial lux: ");
  Serial.print(lux);
  Serial.print(", interrupt enabled: ");
  Serial.print(max44009.interruptEnabled());
  Serial.print(", interrupt status: ");
  Serial.print(max44009.getInterruptStatus());
  Serial.print(", low threshold: ");
  Serial.print(max44009.getLowThreshold());
  Serial.print(", high threshold: ");
  Serial.print(max44009.getHighThreshold());
  Serial.print(", threshold timer: ");
  Serial.println(max44009.getThresholdTimer());
  
  Serial.println("MAX44009 done");

  // Pin Chance Interrupts (http://gammon.com.au/interrupts)
  // Note that PIN_SW1, PD2 is not in PCI interrupt list, it's handled by KONNECTING
  PCMSK1 |= bit(PCINT9);  // PIN_CCS811_INTERRUPT, A1
  PCMSK1 |= bit(PCINT11);  // PIN_MAX44009_INTERRUPT, A3
  PCIFR  |= bit(PCIF1);  // Clear outstanding
  PCICR  |= bit(PCIE1);  // Enable pin change interrupts
  Serial.println("PCI interrupt setup done");

  Serial.println("setup() done");
}


// Main loop
void loop()
{
  const unsigned long nowMs = millis();

  // CCS881
  //
  if(readCcs881)
  { 
    Serial.println("Reading CCS811");
    
    ccs811Wake();
    ccs811.readAlgorithmResults();
    ccs811Sleep();

    const uint16_t ccs881eCo2 = ccs811.getCO2();
    const uint16_t ccs881TVoc = ccs811.getTVOC();
    Serial.print("CCS811 eCO2: ");
    Serial.print(ccs881eCo2);
    Serial.print(", tVOC: ");
    Serial.println(ccs881TVoc);

    const bool ccs881eCo2MeasurementValid = ccs881eCo2 >= CCS811_ECO2_MIN && ccs881eCo2 <= CCS811_ECO2_MAX;
    if(ccs881eCo2MeasurementValid)
      Serial.println(" - eCO2 valid");
    else
      Serial.println(" - eCO2 INVALID");
    
    const bool ccs881TVocMeasurementValid = ccs881TVoc >= CCS811_TVOC_MIN && ccs881TVoc <= CCS811_TVOC_MAX;
    if(ccs881TVocMeasurementValid)
      Serial.println(" - tVOC valid");
    else
      Serial.println(" - tVOC INVALID");

    readCcs881 = false;
  }

  // BME280
  //
  static unsigned long bme280PreviousPollingTimestampMs = 0;

  if(nowMs - bme280PreviousPollingTimestampMs >= BME280_POLLING_INTERVAL_MS)
  {
    float bme280Press, bme280Temp, bme280Rh;
    
    bme280.read(bme280Press, bme280Temp, bme280Rh, BME280::TempUnit_Celcius, BME280::PresUnit_hPa);
    Serial.print("BME280 temp: ");
    Serial.print(bme280Temp);
    Serial.print(", rh: ");
    Serial.print(bme280Rh);
    Serial.print(", press: ");
    Serial.println(bme280Press);

    const bool bme280TempMeasurementValid = bme280Temp >= BME280_TEMP_MIN && bme280Temp <= BME280_TEMP_MAX;
    if(bme280TempMeasurementValid)
      Serial.println(" - temp valid");
    else
      Serial.println(" - temp INVALID");

    const bool bme280RhMeasurementValid = bme280Rh >= BME280_HUM_MIN && bme280Rh <= BME280_HUM_MAX;
    if(bme280RhMeasurementValid)
      Serial.println(" - rh valid");
    else
      Serial.println(" - rh INVALID");
    
    const bool bme280PressMeasurementValid = bme280Press >= BME280_PRESS_MIN && bme280Press <= BME280_PRESS_MAX;
    if(bme280PressMeasurementValid)
      Serial.println(" - press valid");
    else
      Serial.println(" - press INVALID");

    // Update CCS811 if temp/hum valid and significantly differs from last uploaded
    if(bme280TempMeasurementValid && bme280RhMeasurementValid)
    {
      static float ccs811SetTemp = FLT_MAX;
      static float ccs811SetRh = FLT_MAX;
      if(abs(ccs811SetTemp - bme280Temp) > 0.5 ||
         abs(ccs811SetRh - bme280Rh) > 0.5)
      {
        Serial.println(" - Pusing temp/rh to CCS811 environment data");
        
        ccs811Wake();
        ccs811.setEnvironmentalData(bme280Rh, bme280Temp);
        ccs811Sleep();

        ccs811SetTemp = bme280Temp;
        ccs811SetRh = bme280Rh;
      }
    }

    bme280PreviousPollingTimestampMs = nowMs;
  }

  // MAX44009
  //

  if( digitalRead(PIN_MAX44009_INTERRUPT) == LOW)
    Serial.println("PIN_MAX44009_INTERRUPT IS LOW");
  
  if(readMax44009)
  {
    const float max44009Lux = max44009.getLux();
    Serial.print("MAX44009 lux: ");
    Serial.print(max44009Lux);

    Serial.print(" (interrupt enabled: ");
    Serial.print(max44009.interruptEnabled());
    Serial.print(", interrupt status: ");
    Serial.print(max44009.getInterruptStatus());
    Serial.print(", low threshold: ");
    Serial.print(max44009.getLowThreshold());
    Serial.print(", high threshold: ");
    Serial.print(max44009.getHighThreshold());
    Serial.print(", threshold timer: ");
    Serial.println(max44009.getThresholdTimer());


    const bool max44009LuxMeasurementValid = max44009Lux >= MAX44009_LUX_MIN && max44009Lux <= MAX44009_LUX_MAX;
    if(max44009LuxMeasurementValid)
    {
      Serial.println(" - lux valid");

      static float max44009LuxPrevious = FLT_MAX;
      
      const float factor = 1.1;
      const float highThreshold = max44009Lux * factor;
      const float lowThreshold = max44009Lux / factor;
      Serial.print(" - setting new thresholds (10%%) ");
      Serial.print(lowThreshold);
      Serial.print("-");
      Serial.println(highThreshold);
  
      max44009.setHighThreshold(min(highThreshold, MAX44009_LUX_MAX));
      max44009.setLowThreshold(max(lowThreshold, MAX44009_LUX_MIN));
    }
    else
      Serial.println(" - lux INVALID");

    int err = max44009.getError();
    if (err != 0)
    {
      Serial.print("Error:\t");
      Serial.println(err);
    }

    readMax44009 = false;
  }

  // AS312
  //
  static bool motionPrevious = false;
  {
    static bool as312MotionPrevious1 = false;
    static bool as312MotionPrevious2 = false;
    
    const bool as312Motion1 = (digitalRead(PIN_AS312_1_MOTION) == HIGH);
    const bool as312Motion2 = (digitalRead(PIN_AS312_2_MOTION) == HIGH);
    
    if(as312Motion1 != as312MotionPrevious1)
    {
      Serial.print("As312 1 motion: ");
      Serial.println(as312Motion1);
      as312MotionPrevious1 = as312Motion1;
    }

    if(as312Motion2 != as312MotionPrevious2)
    {
      Serial.print("As312 2 motion: ");
      Serial.println(as312Motion2);
      as312MotionPrevious2 = as312Motion2;
    }
  }
}

