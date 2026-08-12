SDK_BUILD_PATH=./build
INSTALL_PATH=$(SDK_BUILD_PATH)

CMAKE_BUILD_TYPE = RELEASE
ifdef DEBUG
CFLAGS += -g -DDEBUG
CMAKE_BUILD_TYPE = DEBUG
endif

CFLAGS += -Wall -pthread

LIBS = -lssl -lcrypto -lpthread -ltio
LD_FLAGS = -L$(INSTALL_PATH)/lib
# On Mac using homebrew.
LD_FLAGS += -L/usr/local/opt/openssl/lib
SOURCES = $(wildcard *.c)
SOURCES += $(wildcard linux-env/*.c)
TARGET = exampleapp
INCLUDES = -I$(INSTALL_PATH)/include
# On Mac using homebrew.
INCLUDES += -I/usr/local/opt/openssl/include/
RPATH = -Wl,-rpath,$(INSTALL_PATH)/lib

all: clean $(TARGET)

sdk:
	rm -rf $(SDK_BUILD_PATH)
	cmake -S ../.. -B $(SDK_BUILD_PATH) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(CURDIR)/$(SDK_BUILD_PATH) -DCMAKE_INSTALL_LIBDIR=lib
	cmake --build $(SDK_BUILD_PATH) --target install

$(TARGET): sdk
	gcc $(CFLAGS) $(SOURCES) $(LIBS) $(LD_FLAGS) $(INCLUDES) $(RPATH) -o $@

clean:
	touch $(TARGET)
	rm $(TARGET)
	touch $(SDK_BUILD_PATH)
	rm -rf $(SDK_BUILD_PATH)

.PHONY: all clean sdk
