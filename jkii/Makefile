ifdef DEBUG
	override CFLAGS += -g -DDEBUG
endif

override CFLAGS += -std=c99 -Wall -fPIC -shared

DOCDIR=html

SOURCES = $(wildcard src/*.c)
SOURCES += libs/jsmn/jsmn.c
INCLUDES = -Ilibs/jsmn -Iinclude

TARGET = libjkii.so

all: clean $(TARGET) doc

$(TARGET):
	gcc $(CFLAGS) $(SOURCES) $(INCLUDES) -o $@

doc:
	doxygen Doxyfile

clean:
	rm -f $(TARGET)
	rm -rf $(DOCDIR)

.PHONY: all clean doc
