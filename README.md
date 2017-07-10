[![BME280](BME280_I2CS.png)](https://www.controleverything.com/content/Humidity?sku=BME280_I2CS)
# BME280
BME280 Digital Humidity, Pressure and Temperature Sensor

The BME280 is a combined humidity, pressure and temperature sensor.

This Device is available from ControlEverything.com [SKU: BME280_I2CS]

https://www.controleverything.com/content/Humidity?sku=BME280_I2CS

This Sample code can be used with Raspberry Pi, Arduino, Particle Photon, Beaglebone Black and Onion Omega.


## Python
Download and install smbus library on Raspberry pi. Steps to install smbus are provided at:

https://pypi.python.org/pypi/smbus-cffi/0.5.1

Download (or git pull) the code in pi. Run the program.

```cpp
$> python BME280.py
```

## C

Setup your BeagleBone Black according to steps provided at:

https://beagleboard.org/getting-started

Download (or git pull) the code in Beaglebone Black.

Compile the c program.
```cpp
$>gcc BME280.c -o BME280
```
Run the c program.
```cpp
$>./BME280
```


#####The code output is the relative humidity in %RH, pressure in hPa and temperature reading in degree celsius and fahrenheit.
