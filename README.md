Room Sensor
===========
This is a sensor array suitable for monitoring a room and reporting to KNX. It's 5x4cm which fits ordinary in-wall/ceiling pods. Equipped with the following sensors:

 - CCS811 - CO2 and volatile organic compounds
 - BME280 - Temperature, humidity & pressure
 - MAX44009 - Lux
 - 2x PIR for motion

Uses ~5mA idle and ~25 when the CCS811 chip is heating up, well within the capabilities of the TPUART2's 50mA limit.

Standing on the shoulders of giants
-----------------------------------
This project makes use the following libraries:
 - https://github.com/KONNEKTING/KonnektingDeviceLibrary
 - https://github.com/RobTillaart/Arduino
 - https://github.com/finitespace/BME280
 - https://github.com/sparkfun/CCS811_Air_Quality_Breakout

License Information
-------------------
Please review the LICENSE.md file for license information.
Distributed as-is; no warranty is given.
