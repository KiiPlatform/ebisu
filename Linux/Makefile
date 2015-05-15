CFLAGS = -std=gnu89 -Wall -pedantic -pthread
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L../kii/
SOURCES = $(wildcard *.c)
TARGET = exampleapp
INCLUDES = -I../kii -I../kii-core

KIISDK = ../kii/libkiisdk.so

all: clean $(TARGET) $(DOCTARGET)

$(KIISDK):
	$(MAKE) -C ../kii


$(TARGET): $(KIISDK)
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -rf $(DOCTARGET)

.PHONY: all clean copy

