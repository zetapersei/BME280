#include <stdio.h>
#include <stdlib.h>
// #include <mysql.h>
#include <string.h>
#include <time.h>
#include <limits.h>			// INT_MIN / INT_MAX
#include <float.h>
#include <math.h>

typedef enum {
	UNDEFINED   = 0,
	TEMPERATURE = 1,
	HUMIDITY    = 2,
	WINDSPEED   = 4,
	WINDGUST    = 8,
	WINDDIR  	= 16,
	RAINTOTAL   = 32,
	SWITCH      = 64,
	BAROMETER   = 128,
	DISTANCE    = 256,
	LEVEL       = 512,
	TEST        = 1024
} SensorType;


typedef struct {		//							Temp	Humid	Pellet	Baro
	double  r;          // Sensor noise variance	 0.2	 2.0	 10		 2
	double  pn;         // Process noise variance	 0.01	 0.05	 0.05	 0.25
	double  p;          // Predicted error
	double  x;          // Predicted value
	
	time_t  save_t;     // Next save time
	int     db_row;     // Database row
	int     save_i;     // Saved int value
	float   save_f;     // Saved float value
	int     type;       // Type of data
} DataStore;

typedef struct {
	float   save_f;     // Saved float value
	time_t  save_t;     // Next save time
	int     db_row;     // Database row
} DataFloat;

typedef struct {
	int     save_i;     // Saved float value
	time_t  save_t;     // Next save time
	int     db_row;     // Database row
} DataInt;

typedef struct {
	char  run;
	char  daemon;
	char  sensorAutoAdd;
	char  *sensor_i2c;
	
	int   is_client;
	char  server[16];
	int   port;
	int   is_server;
	
	char  mysql;
	char  mysqlServer[MAX_TAG_SIZE];
	char  mysqlUser[MAX_TAG_SIZE];
	char  mysqlPass[MAX_TAG_SIZE];
	int   mysqlPort;
	char  mysqlDatabase[MAX_TAG_SIZE];
	
	int   saveTemperatureTime;
	int   saveHumidityTime;
	int   saveRainTime;
	int   saveDistanceTime;
	int   sampleWindTime;
	
} ConfigSettings;

ConfigSettings configFile;



DataStore *sensorDataInit( int type, double last_value, double m_noise, double p_noise ) {
	DataStore *d = (DataStore *) malloc( sizeof( DataStore ) );
	if ( !d ) {
		fprintf( stderr, "ERROR in %s: Allocating memory\n", __func__ );
		return NULL;
	}
	d->r      = m_noise;
	d->pn     = p_noise;
	d->p      = 1.0;
	d->x      = last_value;
	d->save_t = INT_MIN;
	d->save_i = INT_MIN;
	d->save_f = FLT_MIN;
	d->db_row = -1;
	d->type   = type;
	return d;
}

DataInt *sensorDataInt() {
	DataInt *d = (DataInt *) malloc( sizeof( DataInt ) );
	if ( !d ) {
		fprintf( stderr, "ERROR in %s: Allocating memory\n", __func__ );
		return NULL;
	}
	d->save_i = INT_MIN;
	d->save_t = INT_MIN;
	d->db_row = 0;
	return d;
}

DataFloat *sensorDataFloat() {
	DataFloat *d = (DataFloat *) malloc( sizeof( DataFloat ) );
	if ( !d ) {
		fprintf( stderr, "ERROR in %s: Allocating memory\n", __func__ );
		return NULL;
	}
	d->save_f = FLT_MIN;
	d->save_t = INT_MIN;
	d->db_row = 0;
	return d;
}

typedef struct {
	float              x;			// Wind vector X
	float              y;			// Wind vector Y
	time_t             save_time;	// Next save time
	time_t             next_tx;		// Time to new transmission
	unsigned int       rowid;		// Database row
	struct DataSample *head;		// Fisrst stored value
	struct DataSample *tail;		// Last stored value
} DataWind;

typedef struct {
	unsigned int     rowid;		// Database row
	char            *name;		// Name of sensor
	unsigned int     sensor_id;	// Sensors own id
	char    		 protocol[5];	// Protocol
	unsigned char    channel;	// Sensor channel
	unsigned char    rolling;	// Random code
	unsigned char    battery;	// Sensor battery status Full = 1
	SensorType       type;		// Type of sensor
	DataStore		*temperature;
	DataStore		*humidity;
	DataFloat		*distance;
	DataStore		*level;
	DataStore		*barometer;
	DataInt  		*sw;
	DataStore		*test;
	DataFloat		*rain;
	DataWind		*wind;
} sensor;

float sensorKalmanFilter( DataStore *k, float value) {
	// Predict
	double pc = k->p + k->pn;
	// Update
	double g = pc == 0 ? 1 : pc / ( pc + k->r );	// Kalman gain
	k->p  = ( 1 - g ) * pc;
	k->x  = g * ( value - k->x ) + k->x;
#if _DEBUG > 2
	fprintf( stderr, "%s: \tIn:%.2f Out:%.2f Gain:%.4f Err:%.4f\n", __func__, value, k->x, g, k->p );
#endif
	return k->x;
}
time_t sensorTimeSync() {
	static time_t update = 0;
	static int    correction = 0, syncTime = 3600;
	time_t        now = time( NULL );
	
	if ( now < update || !configFile.mysql )
		return (time_t) now - correction;
	
	// Sync time with database
	char query[255] = "";
	int  diff = correction;
	MYSQL_RES   *result ;
	MYSQL_ROW    row;
	
	// Fetch database time and calculate correction
	sprintf( query, "SELECT UNIX_TIMESTAMP()" );
	mysql_query( mysql, query );
	result = mysql_store_result( mysql );
	if( result && ( row = mysql_fetch_row( result ) ) ) 
		correction = now - atoi( row[0] );
	else
		correction = 0;
	
	// Adjust sync time and add 10 sec so it will not become static
	diff = correction - diff;
	if ( diff < 0 )
		diff = -diff;
	if ( diff != 0 && update != 0 )
		syncTime = syncTime / diff;
	syncTime += 10;
	update = (time_t) ( now + syncTime - correction );
#if _DEBUG > 0
	struct tm *local = localtime( &now );
	struct tm *upd   = localtime( &update );
	fprintf( stderr, "[%02i:%02i:%02i] SyncTime: Correction: %+d sec\tNext update: %02i:%02i:%02i\n", 
			local->tm_hour, local->tm_min, local->tm_sec, -correction,
			upd->tm_hour, upd->tm_min, upd->tm_sec );
#endif
	return (time_t) now - correction;
}


char sensorTemperature( sensor *s, float value ) {
#if _DEBUG > 2
	fprintf( stderr, "%s: \t%s [row:%d (%s) id:%d] = %f\n", __func__, s->name, s->rowid, s->protocol, s->sensor_id, value );
#endif
	if ( ! ( s->type & TEMPERATURE ) || value > 300.0 || value < -200.0 )
		return 0;
	else if ( s->temperature == NULL && ( s->temperature = sensorDataInit( TEMPERATURE, value, .2, .01 ) ) == NULL )
		return 1;
	
	value = roundf( sensorKalmanFilter( s->temperature, value ) * 10.0 ) / 10.0;
	time_t now = sensorTimeSync();
	if ( s->temperature->save_f == value && ( configFile.saveTemperatureTime > 0 && now < s->temperature->save_t ) )
		return 0;
	
	// Save temperature
	if ( configFile.mysql ) {
		char query[255] = "";
		sprintf( query, "INSERT INTO wr_temperature (sensor_id,value) "
						"VALUES(%d,%f)", s->rowid, value );
		if ( mysql_query( mysql, query ) ) {
			fprintf( stderr, "ERROR in %s: Inserting\n%s\n%s\n", __func__, mysql_error( mysql ), query );
			return 1;
		}
		s->temperature->db_row = mysql_insert_id( mysql );
	} else {
		printf( "Temperature change [%.2f] on sensor (%s ID:%d CH:%d ROL:%d)\n", value, s->protocol, s->sensor_id, s->channel, s->rolling );
	}
	s->temperature->save_f = value;
	s->temperature->save_t = (time_t) ( now / configFile.saveTemperatureTime + 1 ) * configFile.saveTemperatureTime;
	return 0;
}

int main(int argc, char *argv[]) {
	
	
	return 0;
}
