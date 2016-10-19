override CFLAGS += -Wall -pthread
CXXFLAGS = -Wall
ifdef DEBUG
CFLAGS += -g -DDEBUG
CXXFLAGS += -g -DDEBUG
endif

GTEST_PATH=../../kii-core/gtest-1.7.0

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk -lkiisocketclient
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib
CC_OBJ = $(patsubst %.cc,%.o,$(wildcard *.cc))
TARGET = testapp
INCLUDES = -I../../kii -I../../kii-core -I$(GTEST_PATH)/include -I../../kii_json/include -I../../kii_json/libs/jsmn -I../../Linux

KIISDK = libkiisdk.so
KIICLIENT = libkiisocketclient.so

LIBGTEST=libgtest.a

all: clean $(TARGET)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc
	ar -rv $(LIBGTEST) gtest-all.o gtest_main.o

$(KIICLIENT):
	$(MAKE) -C ../../Linux lib
	cp ../../Linux/$(KIICLIENT) ./

$(KIISDK):
	$(MAKE) -C ../../kii $(KIISDKPARAM) "CFLAGS+=-DKII_JSON_FIXED_TOKEN_NUM=256"
	cp ../../kii/$(KIISDK) ./

$(TARGET): $(KIISDK) $(KIICLIENT) $(LIBGTEST) $(CC_OBJ)
	g++ -pthread -o $(TARGET) $(CC_OBJ) $(LD_FLAGS) $(LIBS) $(LIBGTEST)

test: $(TARGET)
	./$(TARGET)

.c.o:
	gcc $(CFLAGS) $(INCLUDES) -c $<

.cc.o:
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -f $(KIISDK)
	rm -f $(KIICLIENT)
	rm -f $(LIBGTEST)

.PHONY: all clean copy

