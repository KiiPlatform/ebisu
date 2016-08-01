override CFLAGS += -std=gnu89 -Wall -pedantic -pthread
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L.
SOURCES = $(wildcard *.c) ../kii-core/linux/kii_core_secure_socket.c
TARGET = exampleapp
LIB_SOURCES = $(wildcard kii_*.c) ../kii-core/linux/kii_core_secure_socket.c
LIB_TARGET = libkiisocketclient.so
INCLUDES = -I../kii -I../kii-core -I../kii_json/include -I../kii_json/libs/jsmn -I../kii-core/linux/ -I/usr/local/opt/openssl/include/

KIISDKNAME = libkiisdk.so
KIISDK = ../kii/$(KIISDKNAME)

all: clean $(TARGET) $(DOCTARGET)

lib: clean $(LIB_TARGET)

$(KIISDK):
	$(MAKE) -C ../kii $(KIISDKPARAM) "CFLAGS+=-DFIXED_JSON_TOKEN_NUM=256 -DKEEP_ALIVE_INTERVAL=30"
	cp $(KIISDK) ./


$(TARGET): $(KIISDK)
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

$(LIB_TARGET): $(KIISDK)
	gcc $(CFLAGS) -shared $(LIB_SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -rf $(DOCTARGET)
	touch $(KIISDK)
	rm -rf $(KIISDK)
	rm -rf $(KIISDKNAME)
	rm -rf $(LIB_TARGET)

.PHONY: all clean copy

