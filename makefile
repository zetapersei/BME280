store_bme280.o: store_bme280.c

	gcc  -Wall -pedantic -std=c99  store_bme280.c  `mysql_config --cflags --libs` -o store_bme280

clean:

rm -r store_bme280
