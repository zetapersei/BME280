// This code is designed to work with the BME280_I2CS I2C Mini Module available from ControlEverything.com.
// https://www.controleverything.com/content/Humidity?sku=BME280_I2CS#tabs-0-product_tabset-2

/* 
*************************************************************************
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT 
   HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
   OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF THE COPYRIGHT HOLDERS OR 
   CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <float.h>
#include <math.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mysql.h>
#include "Kalman.h"

#define TEMP_DIFF 10
time_t t;
int error = 0;
struct tm *local;

// Mysql variables
	
MYSQL *connection, mysql;

static const char LANG_DB_TEMP_DIFF[] = "\nWARNING: Temperature difference out of bonds (%f to %f). Data will NOT be saved!\n";
static const char LANG_DB_HUMID_DIFF[] = "\nWARNING: Humidity value out of bonds (%i %%). Data will NOT be saved!\n";

Kalman myFilterTemp(0.125,32,1023,0); //suggested initial values for high noise filtering
Kalman myFilterHum(0.125,32,1023,0);
Kalman myFilterPress(0.125,32,1023,0);


float ConvertFormat(float formData)
    {
	char tempdata[5] = "";
	sprintf(tempdata,"%.1f", formData);
	float finalTemp = atof(tempdata);
	return finalTemp;
     }



void saveTemperature(float temperature)
{
	static signed int t_old_min = -1;
	static float old_value = -FLT_MAX;

	t = time(NULL);
	local = localtime(&t);

	/* Store if value has changed or not from same minute */
	if (t_old_min != local->tm_hour || old_value != temperature) {

		/* Check for invalid values */
		float difference = old_value - temperature;
		if ((difference < -TEMP_DIFF || difference > TEMP_DIFF)
		    && old_value != -FLT_MAX) {
			printf(LANG_DB_TEMP_DIFF, old_value, temperature);
			return;
		}

		char query_1[255] = "";

        	sprintf( query_1, "INSERT INTO wr_temperature (sensor_id, value) " "VALUES(5, %.1f)", temperature );

        	int state = mysql_query(connection, query_1);

        	if (state != 0) {
                	printf("%s", mysql_error(connection));
                	return 1;
                }
			
		
	}

	t_old_min = local->tm_hour;
	old_value = temperature;
}


void saveHumidity(unsigned int humidity)
{
	static signed int h_old_min = -1;
	static int old_value = -INT_MAX;

	t = time(NULL);
	local = localtime(&t);

	/* Store if value has changed or not from same minute */
	if (h_old_min != local->tm_hour || old_value != humidity) {

		/* Check for invalid values */
		if (humidity <= 0 || humidity > 100) {
			fprintf(stderr, LANG_DB_HUMID_DIFF, humidity);
			return;
		}

		char query[255] = "";

        sprintf( query, "INSERT INTO wr_humidity (sensor_id, value) " "VALUES(5, %u)", humidity);

        int state = mysql_query(connection, query);

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
                }
	}

	h_old_min = local->tm_hour;
	old_value = humidity;
}

void savePressure(int pressure)
{
	static signed int t_old_min = -1;
	static int old_value = -INT_MAX;

	t = time(NULL);
	local = localtime(&t);

	/* Store if value has changed or not from same minute */
	if (t_old_min != local->tm_hour || old_value != pressure) {

		/* Check for invalid values */
		int difference = old_value - pressure;
		if ((difference < -TEMP_DIFF || difference > TEMP_DIFF)
		    && old_value != -INT_MAX) {
			printf(LANG_DB_TEMP_DIFF, old_value, pressure);
			return;
		}

		char query_1[255] = "";

        	sprintf( query_1, "INSERT INTO wr_barometer (sensor_id, value) " "VALUES(5, %u)", pressure );

        	int state = mysql_query(connection, query_1);

        	if (state != 0) {
                	printf("%s", mysql_error(connection));
                	return 1;
                }
			
		
	}

	t_old_min = local->tm_hour;
	old_value = pressure;
}



// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	pid_t pid, sid;

    // 1 - Fork
    pid = fork(); 

    if (pid < 0) 
    {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }

    //2 - Umask
    umask(0);

    //3 - Logs
    openlog(argv[0],LOG_NOWAIT|LOG_PID,LOG_USER); 
    syslog(LOG_NOTICE, "Successfully started daemon\n"); 

    //4 - Session Id
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Could not create process group\n");
        exit(EXIT_FAILURE);
    }

    //5 - WD
    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "Could not change working directory to /\n");
        exit(EXIT_FAILURE);
    }

    //6 - FD
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

	
	
	// Create I2C bus
	int file;
	char *bus = "/dev/i2c-1";
	if((file = open(bus, O_RDWR)) < 0) 
	{
		printf("Failed to open the bus. \n");
		exit(1);
	}
	// Get I2C device, BME280 I2C address is 0x76(136)
	ioctl(file, I2C_SLAVE, 0x76);

	// Read 24 bytes of data from register(0x88)
	char reg[1] = {0x88};
	write(file, reg, 1);
	char b1[24] = {0};
	if(read(file, b1, 24) != 24)
	{
		printf("Error : Input/Output error \n");
		exit(1);
	}

	// Convert the data
	// temp coefficents
	int dig_T1 = (b1[0] + b1[1] * 256);
	int dig_T2 = (b1[2] + b1[3] * 256);
	if(dig_T2 > 32767)
	{
		dig_T2 -= 65536;
	}
	int dig_T3 = (b1[4] + b1[5] * 256);
	if(dig_T3 > 32767)
	{
		dig_T3 -= 65536;
	}

	// pressure coefficents
	int dig_P1 = (b1[6] + b1[7] * 256);
	int dig_P2 = (b1[8] + b1[9] * 256);
	if(dig_P2 > 32767)
	{
		dig_P2 -= 65536;
	}
	int dig_P3 = (b1[10] + b1[11] * 256);
	if(dig_P3 > 32767)
	{
		dig_P3 -= 65536;
	}
	int dig_P4 = (b1[12] + b1[13] * 256);
	if(dig_P4 > 32767)
	{
		dig_P4 -= 65536;
	}
	int dig_P5 = (b1[14] + b1[15] * 256);
	if(dig_P5 > 32767)
	{
	dig_P5 -= 65536;
	}
	int dig_P6 = (b1[16] + b1[17] * 256);
	if(dig_P6 > 32767)
	{
		dig_P6 -= 65536;
	}
	int dig_P7 = (b1[18] + b1[19] * 256);
	if(dig_P7 > 32767)
	{
		dig_P7 -= 65536;
	}
	int dig_P8 = (b1[20] + b1[21] * 256);
	if(dig_P8 > 32767)
	{
	dig_P8 -= 65536;
	}
	int dig_P9 = (b1[22] + b1[23] * 256);
	if(dig_P9 > 32767)
	{
		dig_P9 -= 65536;
	}

	// Read 1 byte of data from register(0xA1)
	reg[0] = 0xA1;
	write(file, reg, 1);
	char data[8] = {0};
	read(file, data, 1);
	int dig_H1 = data[0];

	// Read 7 bytes of data from register(0xE1)
	reg[0] = 0xE1;
	write(file, reg, 1);
	read(file, b1, 7);

	// Convert the data
	// humidity coefficents
	int dig_H2 = (b1[0] + b1[1] * 256);
	if(dig_H2 > 32767)
	{
		dig_H2 -= 65536;
	}
	int dig_H3 = b1[2] & 0xFF ;
	int dig_H4 = (b1[3] * 16 + (b1[4] & 0xF));
	if(dig_H4 > 32767)
	{
		dig_H4 -= 65536;
	}
	int dig_H5 = (b1[4] / 16) + (b1[5] * 16);
	if(dig_H5 > 32767)
	{
		dig_H5 -= 65536;
	}
	int dig_H6 = b1[6];
	if(dig_H6 > 127)
	{
		dig_H6 -= 256;
	}

	// Select control humidity register(0xF2)
	// Humidity over sampling rate = 1(0x01)
	char config[2] = {0};
	config[0] = 0xF2;
	config[1] = 0x01;
	write(file, config, 2);
	// Select control measurement register(0xF4)
	// Normal mode, temp and pressure over sampling rate = 1(0x27)
	config[0] = 0xF4;
	config[1] = 0x27;
	write(file, config, 2);
	// Select config register(0xF5)
	// Stand_by time = 1000 ms(0xA0)
	config[0] = 0xF5;
	config[1] = 0xA0;
	write(file, config, 2);
	
	// Initialization Mysql
	mysql_init(&mysql);
        connection = mysql_real_connect(&mysql,"localhost", "user", "password", 
                                    "weather", 0, 0, 0);

        if (connection == NULL) {
                printf("%s", mysql_error(&mysql));
                return 1;
		}
	
while(-1)
{	// Read 8 bytes of data from register(0xF7)
	// pressure msb1, pressure msb, pressure lsb, temp msb1, temp msb, temp lsb, humidity lsb, humidity msb
	reg[0] = 0xF7;
	write(file, reg, 1);
	read(file, data, 8);

	// Convert pressure and temperature data to 19-bits
	long adc_p = ((long)(data[0] * 65536 + ((long)(data[1] * 256) + (long)(data[2] & 0xF0)))) / 16;
	long adc_t = ((long)(data[3] * 65536 + ((long)(data[4] * 256) + (long)(data[5] & 0xF0)))) / 16;
	// Convert the humidity data
	long adc_h = (data[6] * 256 + data[7]);

	// Temperature offset calculations
	float var1 = (((float)adc_t) / 16384.0 - ((float)dig_T1) / 1024.0) * ((float)dig_T2);
	float var2 = ((((float)adc_t) / 131072.0 - ((float)dig_T1) / 8192.0) *
					(((float)adc_t)/131072.0 - ((float)dig_T1)/8192.0)) * ((float)dig_T3);
	float t_fine = (long)(var1 + var2);
	float cTemp = (var1 + var2) / 5120.0;
	float fTemp = cTemp * 1.8 + 32;

	// Pressure offset calculations
	var1 = ((float)t_fine / 2.0) - 64000.0;
	var2 = var1 * var1 * ((float)dig_P6) / 32768.0;
	var2 = var2 + var1 * ((float)dig_P5) * 2.0;
	var2 = (var2 / 4.0) + (((float)dig_P4) * 65536.0);
	var1 = (((float) dig_P3) * var1 * var1 / 524288.0 + ((float) dig_P2) * var1) / 524288.0;
	var1 = (1.0 + var1 / 32768.0) * ((float)dig_P1);
	float p = 1048576.0 - (float)adc_p;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((float) dig_P9) * p * p / 2147483648.0;
	var2 = p * ((float) dig_P8) / 32768.0;
	float pressure = (p + (var1 + var2 + ((float)dig_P7)) / 16.0) / 100;

	// Humidity offset calculations
	float var_H = (((float)t_fine) - 76800.0);
	var_H = (adc_h - (dig_H4 * 64.0 + dig_H5 / 16384.0 * var_H)) * (dig_H2 / 65536.0 * (1.0 + dig_H6 / 67108864.0 * var_H * (1.0 + dig_H3 / 67108864.0 * var_H)));
	float humidity = var_H * (1.0 -  dig_H1 * var_H / 524288.0);
	if(humidity > 100.0)
	{
		humidity = 100.0;
	}else
	if(humidity < 0.0) 
	{
		humidity = 0.0;
	}

	// Output data to screen
	// printf("Temperature in Celsius : %.2f C \n", cTemp);
	// printf("Temperature in Fahrenheit : %.2f F \n", fTemp);
	// printf("Pressure : %.2f hPa \n", pressure);
	// printf("Relative Humidity : %.2f RH \n", humidity);
	
	
	
	float kTemp = myFilterTemp.getFilteredValue(cTemp);
	saveTemperature(ConvertFormat(kTemp));
	
	float kHum = myFilterHum.getFilteredValue(humidity);
	saveHumidity((int)kHum);
	
	float kPress = myFilterPress.getFilteredValue(pressure);
	savePressure((int)kPress);
	
	sleep(1);
   }
	
	mysql_close(connection);
	
	closelog();
	
}
