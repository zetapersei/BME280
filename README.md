[![BME280](BME280_I2CS.png)](https://www.controleverything.com/content/Humidity?sku=BME280_I2CS)
# BME280
BME280 Digital Humidity, Pressure and Temperature Sensor

The BME280 is a combined humidity, pressure and temperature sensor.

This Device is available from ControlEverything.com [SKU: BME280_I2CS]

https://www.controleverything.com/content/Humidity?sku=BME280_I2CS

This Sample code can be used with Raspberry Pi, Arduino, Particle Photon, Beaglebone Black and Onion Omega.




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
###### 

Adding the database code for store and read the values using Mysql.


#####The code output is the relative humidity in %RH, pressure in hPa and temperature reading in degree celsius and fahrenheit.
