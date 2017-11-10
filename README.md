Room Sensor
===========

This is a sensor array suitable for monitoring a room and reporting to KNX.
It's 5x4cm which fits ordinary in-wall/ceiling pods.

It has:
 - CCS811 - CO2 and Volatile Organic Compounds (both are estimates)
 - BME280 - Temperature, humidity & pressure
 - MAX44009 - Lux
 - 2x PIR

Uses ~5ms idle and ~25 when the CCS811 chip is heating up, well within the capabilities of the TPUART2's 50ma limit.

License Information
-------------------

Please review the LICENSE.md file for license information.

Distributed as-is; no warranty is given.
