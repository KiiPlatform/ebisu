ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

override CFLAGS += -std=c99 -Wall -fPIC -shared

SOURCES = $(wildcard src/*.c)
SOURCES += libs/jsmn/jsmn.c
INCLUDES = -Ilibs/jsmn -Iinclude

TARGET = libkiijson.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(INCLUDES) -o $@

test:
	make test -C unit-test

doc:
	doxygen Doxyfile

clean:
	touch $(TARGET)
	rm -rf $(TARGET)

.PHONY: clean
