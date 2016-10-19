override CFLAGS += -Wall -pthread
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib
SOURCES = $(wildcard *.c) ../kii-core/linux/kii_core_secure_socket.c
TARGET = exampleapp
LIB_SOURCES = $(wildcard kii_*.c) ../kii-core/linux/kii_core_secure_socket.c
LIB_TARGET = libkiisocketclient.so
INCLUDES = -I../kii -I../kii-core -I../kii_json/include -I../kii_json/libs/jsmn -I../kii-core/linux/ -I/usr/local/opt/openssl/include/

KIISDK = libkiisdk.so

all: clean $(TARGET)

lib: clean $(LIB_TARGET)

$(KIISDK):
	$(MAKE) -C ../kii $(KIISDKPARAM) "CFLAGS+=-DFIXED_JSON_TOKEN_NUM=256 -DKEEP_ALIVE_INTERVAL=30"
	cp ../kii/$(KIISDK) ./


$(TARGET): $(KIISDK)
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

$(LIB_TARGET): $(KIISDK)
	gcc $(CFLAGS) -fPIC -shared $(LIB_SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)
	touch $(KIISDK)
	rm -rf $(KIISDK)
	rm -rf $(LIB_TARGET)

.PHONY: all clean copy

