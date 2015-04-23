CFLAGS = -std=gnu89 -Wall -pedantic -pthread
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread
SOURCES = $(wildcard *.c)
SOURCES += kii-core/kii.c kii-core/kii_libc_wrapper.c
TARGET = exampleapp

all: clean $(TARGET)

copy:
	cp ../kii-core/*.c kii-core/
	cp ../kii-core/*.h kii-core/

$(TARGET): copy
	gcc $(CFLAGS) $(SOURCES) $(LIBS) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)

.PHONY: all clean copy

