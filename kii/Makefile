ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

ifndef KEEP_ALIVE_INTERVAL
	override CFLAGS += -DKII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS=30
else
	override CFLAGS += -DKII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS=$(KEEP_ALIVE_INTERVAL)
endif

ifdef JSON_TOKEN_NUM
	override CFLAGS += -DKII_JSON_FIXED_TOKEN_NUM=$(JSON_TOKEN_NUM)
endif

override CFLAGS += -std=gnu89 -Wall -pedantic -pthread -fPIC -shared

LIBS = -lssl -lcrypto -lpthread
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../kii-core/*.c)
SOURCES += $(wildcard ../kii_json/src/*.c)
SOURCES += ../lib/jsmn/jsmn.c
INCLUDES = -I../kii-core -I../lib/jsmn -I../kii_json/include

TARGET = libkiisdk.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm -rf $(TARGET) html latex

doc:
	doxygen Doxyfile

.PHONY: clean
