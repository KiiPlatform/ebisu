CFLAGS = -std=gnu89 -Wall -pedantic -pthread -fPIC -shared

ifdef DEBUG
	CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread -ljsmn
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../kii-core/*.c)
INCLUDES = -I../kii-core -I../lib/jsmn
LIB_PATH = -L../lib/jsmn

TARGET = libkiisdk.so

all: clean libjsmn $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(LIB_PATH) $(LIBS) $(INCLUDES) -o $@

libjsmn:
	(cd ../lib/jsmn; make)

clean:
	touch $(TARGET)
	rm $(TARGET)

.PHONY: clean
