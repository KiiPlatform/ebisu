CXXFLAGS = -Wall -std=c++11
CXXFLAGS += -g -DDEBUG

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib
OBJS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
TARGET = ./testapp
INCLUDES = -I../../kii -I../../khc/include -I../../kii_json/include -I../../kii_json/libs/jsmn

KIISDK = libkiisdk.so


TEST_RESULT_DIR=test-results/junit
TEST_RESULT=$(TEST_RESULT_DIR)/test-result.xml

ifdef MEMCHECK
TESTCMD=valgrind --leak-check=yes $(TARGET)
else
TESTCMD=$(TARGET)
endif

ifdef JUNIT
TESTCMD+= -r junit -o $(TEST_RESULT)
endif

all: clean $(TARGET)

$(KIISDK):
	FIXED_JSON_TOKEN_NUM=256 $(MAKE) -C ../../kii
	cp ../../kii/$(KIISDK) ./

$(TARGET): $(KIISDK) $(OBJS)
	g++ -pthread -o $(TARGET) $(OBJS) $(LD_FLAGS) $(LIBS) $(LIBGTEST)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):. \
	$(TESTCMD)

.cpp.o:
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -f $(KIISDK)

.PHONY: all clean copy

