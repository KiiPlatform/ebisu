CFLAGS = -std=gnu89 -Wall -pedantic -pthread -fPIC -shared

ifdef DEBUG
	CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard ../kii-core/*.c)
INCLUDES = -I../kii-core

TARGET = libkiisdk.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)

.PHONY: clean
