CFLAGS = -std=gnu89 -Wall -pedantic -pthread
CXXFLAGS = -Wall -pedantic
#CFLAGS = -std=gnu89 -w -pedantic
ifdef DEBUG
CFLAGS += -g -DDEBUG
CXXFLAGS += -g -DDEBUG
endif

KIISDKPARAM =
ifdef DEBUG
KIISDKPARAM += DEBUG=1
endif

LIBS = -lssl -lcrypto -lpthread -lkiisdk -lgtest_main
LD_FLAGS = -L../kii -L../gtest-1.7.0/lib
C_OBJ = $(patsubst %.c,%.o,$(wildcard *.c))
CC_OBJ = $(patsubst %.cc,%.o,$(wildcard *.cc))
TARGET = testapp
INCLUDES = -I../kii -I../kii-core -I../gtest-1.7.0/include

KIISDK = ../kii/libkiisdk.so

all: clean $(TARGET) $(DOCTARGET)

$(KIISDK):
	$(MAKE) -C ../kii $(KIISDKPARAM)
	cp $(KIISDK) ./


$(TARGET): $(KIISDK) $(C_OBJ) $(CC_OBJ)
	g++ -o $(TARGET) $(C_OBJ) $(CC_OBJ) $(LD_FLAGS) $(LIBS)

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

.PHONY: all clean copy

