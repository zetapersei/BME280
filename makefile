bme280_store.o: bme280_store.c

	gcc  -Wall -pedantic -std=c99  bme280_store.c  `mysql_config --cflags --libs` -o bme280_store

clean:

	rm -r bme280_store
