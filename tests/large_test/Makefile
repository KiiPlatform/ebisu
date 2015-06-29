CFLAGS = -std=gnu89 -Wall -pedantic -pthread
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

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L../../kii
C_OBJ = $(patsubst %.c,%.o,$(wildcard *.c))
CC_OBJ = $(patsubst %.cc,%.o,$(wildcard *.cc))
TARGET = testapp
INCLUDES = -I../../kii -I../../kii-core -I$(GTEST_PATH)/include

KIISDK = ../../kii/libkiisdk.so

LIBGTEST=libgtest.a

all: clean $(TARGET) $(DOCTARGET)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc
	ar -rv $(LIBGTEST) gtest-all.o gtest_main.o

$(KIISDK):
	$(MAKE) -C ../../kii $(KIISDKPARAM)
	cp $(KIISDK) ./


$(TARGET): $(KIISDK) $(LIBGTEST) $(C_OBJ) $(CC_OBJ)
	g++ -o $(TARGET) $(C_OBJ) $(CC_OBJ) $(LD_FLAGS) $(LIBS) $(LIBGTEST)

.c.o:
	gcc $(CFLAGS) $(INCLUDES) -c $<

.cc.o:
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -rf $(DOCTARGET)
	touch $(KIISDK)
	rm -rf $(KIISDK)
	rm -f libkiisdk.so
	rm -f $(LIBGTEST)

.PHONY: all clean copy

