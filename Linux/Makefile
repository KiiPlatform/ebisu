CFLAGS = -std=gnu89 -Wall -pedantic -pthread
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L../kii/
SOURCES = $(wildcard *.c)
TARGET = exampleapp
INCLUDES = -I../kii -I../kii-core -I../kii_json/include -I../kii_json/libs/jsmn

KIISDK = ../kii/libkiisdk.so

all: clean $(TARGET) $(DOCTARGET)

$(KIISDK):
	$(MAKE) -C ../kii $(KIISDKPARAM) "CFLAGS=-DKII_JSON_FIXED_TOKEN_NUM=256"
	cp $(KIISDK) ./


$(TARGET): $(KIISDK)
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -rf $(DOCTARGET)
	touch $(KIISDK)
	rm -rf $(KIISDK)

.PHONY: all clean copy

