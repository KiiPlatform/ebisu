CXXFLAGS = -Wall -std=c++11
CXXFLAGS += -g -DDEBUG

ROOT_DIR=../../..
KII_DIR=$(ROOT_DIR)/kii
TEST_ROOT_DIR=../..
LTEST_ROOT_DIR=$(TEST_ROOT_DIR)/large_test

LIBS = -lssl -lcrypto -lpthread -lkiisdk
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib

SRCS = $(wildcard *.cpp)
SRCS += $(wildcard $(LTEST_ROOT_DIR)/*.cpp)

TARGET = ./testapp
INCLUDES = -I$(LTEST_ROOT_DIR) -I$(TEST_ROOT_DIR) -I$(KII_DIR) -I$(ROOT_DIR)/khc/include -I$(ROOT_DIR)/kii_json/include -I$(ROOT_DIR)/kii_json/libs/jsmn

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
	$(MAKE) -C $(KII_DIR)
	cp $(KII_DIR)/$(KIISDK) ./

$(TARGET): $(KIISDK) $(SRCS)
	g++ $(CXXFLAGS) $(SRCS) $(LD_FLAGS) $(INCLUDES) $(LIBS) -o $(TARGET)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):. \
	$(TESTCMD)

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -f $(KIISDK)

.PHONY: all clean copy

