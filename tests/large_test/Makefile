override CFLAGS += -Wall -pthread
CXXFLAGS = -Wall -std=c++11
ifdef DEBUG
CFLAGS += -g -DDEBUG
CXXFLAGS += -g -DDEBUG
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib
OBJS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
TARGET = testapp
INCLUDES = -I../../kii -I../../khc/include -I../../kii_json/include -I../../kii_json/libs/jsmn

KIISDK = libkiisdk.so

all: clean $(TARGET)

$(KIISDK):
	FIXED_JSON_TOKEN_NUM=256 $(MAKE) -C ../../kii
	cp ../../kii/$(KIISDK) ./

$(TARGET): $(KIISDK) $(OBJS)
	g++ -pthread -o $(TARGET) $(OBJS) $(LD_FLAGS) $(LIBS) $(LIBGTEST)

test: $(TARGET)
	LD_LIBRARY_PATH=. ./$(TARGET)

.c.o:
	gcc $(CFLAGS) $(INCLUDES) -c $<

.cpp.o:
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -f $(KIISDK)

.PHONY: all clean copy

