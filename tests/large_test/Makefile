override CFLAGS += -std=gnu89 -Wall -pedantic -pthread
CXXFLAGS = -Wall -pedantic
#CFLAGS = -std=gnu89 -w -pedantic
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
LD_FLAGS = -L.
CC_OBJ = $(patsubst %.cc,%.o,$(wildcard *.cc))
TARGET = testapp
INCLUDES = -I../../kii -I../../kii-core -I$(GTEST_PATH)/include -I../../kii_json/include -I../../kii_json/libs/jsmn -I../../Linux

KIISDKNAME = libkiisdk.so
KIISDK = ../../kii/$(KIISDKNAME)
KIICLIENTNAME = libkiisocketclient.so
KIICLIENT = ../../Linux/$(KIICLIENTNAME)

TEST = test

LIBGTEST=libgtest.a

all: clean $(TARGET) $(DOCTARGET)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc
	ar -rv $(LIBGTEST) gtest-all.o gtest_main.o

$(KIICLIENT):
	$(MAKE) -C ../../Linux lib
	cp $(KIICLIENT) ./

$(KIISDK):
	$(MAKE) -C ../../kii $(KIISDKPARAM) "CFLAGS+=-DKII_JSON_FIXED_TOKEN_NUM=256"
	cp $(KIISDK) ./

$(TARGET): $(KIISDK) $(KIICLIENT) $(LIBGTEST) $(CC_OBJ)
	g++ -o $(TARGET) $(CC_OBJ) $(LD_FLAGS) $(LIBS) $(LIBGTEST)

.c.o:
	gcc $(CFLAGS) $(INCLUDES) -c $<

.cc.o:
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -rf $(DOCTARGET)
	rm -f $(KIISDK)
	rm -f $(KIISDKNAME)
	rm -f $(KIICLIENT)
	rm -f $(KIICLIENTNAME)
	rm -f $(LIBGTEST)

.PHONY: all clean copy

