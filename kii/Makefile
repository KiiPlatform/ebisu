ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

ifdef KII_MQTT_USE_PORT_TCP
	override CFLAGS += -DKII_MQTT_USE_PORT_TCP=$(KII_MQTT_USE_PORT_TCP)
endif

override CFLAGS += -std=c99 -Wall -pthread -fPIC -shared

LIBS = -lssl -lcrypto -lpthread
LIBS_DIR = -L/usr/local/opt/openssl/lib
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../khc/src/*.c)
SOURCES += $(wildcard ../kii_json/src/*.c)
SOURCES += $(wildcard ../kii_json/libs/jsmn/*.c)
INCLUDES = -I../khc/include -I../kii_json/include -I../kii_json/libs/jsmn

TARGET = libkiisdk.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(LIBS_DIR) $(LIBS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm -rf $(TARGET)

doc:
	doxygen Doxyfile

.PHONY: clean
