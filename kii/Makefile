ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

ifndef KEEP_ALIVE_INTERVAL
	override CFLAGS += -DKII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS=300
else
	override CFLAGS += -DKII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS=$(KEEP_ALIVE_INTERVAL)
endif

ifdef KII_MQTT_USE_PORT_TCP
	override CFLAGS += -DKII_MQTT_USE_PORT_TCP=$(KII_MQTT_USE_PORT_TCP)
endif

ifndef FIXED_JSON_TOKEN_NUM
	JSON_FLAG = -DKII_JSON_FIXED_TOKEN_NUM=128
else
	JSON_FLAG = -DKII_JSON_FIXED_TOKEN_NUM=$(FIXED_JSON_TOKEN_NUM)
endif

ifdef FLEXIBLE_JSON_TOKEN
	JSON_FLAG =
endif

override CFLAGS += -std=gnu89 -Wall -pedantic -pthread -fPIC -shared $(JSON_FLAG)

LIBS = -lssl -lcrypto -lpthread
LIBS_DIR = -L/usr/local/opt/openssl/lib
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../kii-core/*.c)
SOURCES += $(wildcard ../kii_json/src/*.c)
SOURCES += ../lib/jsmn/jsmn.c
INCLUDES = -I../kii-core -I../lib/jsmn -I../kii_json/include

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
