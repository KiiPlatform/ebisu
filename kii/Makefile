CFLAGS = -std=gnu89 -Wall -pedantic -pthread -fPIC -shared

ifdef DEBUG
	CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread -ljsmn
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../kii-core/*.c)
INCLUDES = -I../kii-core
LIB_PATH = -L../lib/jsmn

TARGET = libkiisdk.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(LIB_PATH) $(LIBS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)

.PHONY: clean
