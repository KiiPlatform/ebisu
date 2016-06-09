ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

ifndef FIXED_JSON_TOKEN_NUM
	JSON_FLAG = -DKII_JSON_FIXED_TOKEN_NUM=128
else
	JSON_FLAG = -DKII_JSON_FIXED_TOKEN_NUM=$(FIXED_JSON_TOKEN_NUM)
endif

ifdef FLEXIBLE_JSON_TOKEN
	JSON_FLAG =
endif

override CFLAGS += -std=gnu89 -Wall -pedantic -fPIC -shared $(JSON_FLAG)

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
