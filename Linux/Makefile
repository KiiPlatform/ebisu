CFLAGS = -std=gnu89 -Wall -pedantic -pthread
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread
SOURCES = $(wildcard *.c)
SOURCES += kii-core/kii_core.c kii-core/kii_libc_wrapper.c
TARGET = exampleapp
DOCTARGET = doc 

all: clean $(TARGET) $(DOCTARGET)

copy:
	cp ../kii-core/*.c ./kii-core
	cp ../kii-core/*.h ./kii-core

$(TARGET): copy
	gcc $(CFLAGS) $(SOURCES) $(LIBS) -o $@

$(DOCTARGET): kii.h
	doxygen

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -rf $(DOCTARGET)
	touch kii-core/dummy.h
	rm kii-core/*

.PHONY: all clean copy

