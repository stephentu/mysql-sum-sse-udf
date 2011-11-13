CC=gcc
OPT=-O3
CFLAGS=-Wall
MYSQL_INC=/usr/include/mysql
MYSQL_PLUGIN_DIR=/usr/lib/mysql/plugin

all: libsumsse.so 

sumsse.o: sumsse.c
	$(CC) $(CFLAGS) $(OPT) -I$(MYSQL_INC) -fPIC -c sumsse.c -o sumsse.o

libsumsse.so: sumsse.o
	ld -shared -o libsumsse.so sumsse.o

install: install_udf

.PHONY: install_udf
install_udf: libsumsse.so
	install -m 644 -g mysql -o mysql libsumsse.so $(MYSQL_PLUGIN_DIR)

.PHONY: clean
clean:
	rm -rf *.so *.o
