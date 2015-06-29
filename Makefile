override CFLAGS += -std=gnu89 -Wall -pedantic -fPIC -shared

ifdef DEBUG
	CFLAGS += -g -DDEBUG
endif

SOURCES = $(wildcard src/*.c)
SOURCES += libs/jsmn/jsmn.c
INCLUDES = -Ilibs/jsmn -Iinclude

TARGET = libkiijson.so

all: clean $(TARGET)

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(INCLUDES) -o $@

test:
	make -C unit-test
	(pushd unit-test/target; ./testapp; popd)

doc:
	doxygen Doxyfile

clean:
	touch $(TARGET)
	rm -rf $(TARGET) html latex

.PHONY: clean
