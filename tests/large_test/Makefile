override CFLAGS += -std=gnu89 -Wall -pedantic -pthread
CXXFLAGS = -Wall -pedantic
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
CXXFLAGS += -g -DDEBUG
endif

GTEST_PATH=../../gtest-1.7.0

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk -lkiiclient
LD_FLAGS = -L.
CC_OBJ = $(patsubst %.cc,%.o,$(wildcard *.cc))
TARGET = testapp
INCLUDES = -I../../kii -I../../kii-core -I$(GTEST_PATH)/include -I../../kii_json/include -I../../kii_json/libs/jsmn -I../../Linux

KIISDK = ../../kii/libkiisdk.so
KIICLIENT = ../../Linux/libkiiclient.so
TEST = test

LIBGTEST=libgtest.a

all: clean $(TARGET) $(DOCTARGET)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc
	ar -rv $(LIBGTEST) gtest-all.o gtest_main.o

client:
	$(MAKE) -C ../../Linux lib
	cp $(KIICLIENT) ./

kiisdk:
	$(MAKE) -C ../../kii $(KIISDKPARAM) "CFLAGS+=-DKII_JSON_FIXED_TOKEN_NUM=256"
	cp $(KIISDK) ./

$(TARGET): client kiisdk $(LIBGTEST) $(CC_OBJ)
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
	rm -f libkiisdk.so
	rm -f $(LIBGTEST)
	rm -f libkiiclient.so

.PHONY: all clean copy

