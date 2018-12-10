CXXFLAGS = -Wall -std=c++11
CXXFLAGS += -g -DDEBUG
CMAKE_BUILD_TYPE = DEBUG

ROOT_DIR=../../..

KII_BUILD_DIR=build-kii
KII_SDK=$(KII_BUILD_DIR)/libs/libkii.so

TEST_ROOT_DIR=../..
LTEST_ROOT_DIR=$(TEST_ROOT_DIR)/large_test

LIBS = -lssl -lcrypto -lpthread -lkii
LD_FLAGS = -L. -L/usr/local/opt/openssl/lib -L$(KII_BUILD_DIR)/usr/local/lib

SRCS = $(wildcard *.cpp)
SRCS += $(wildcard $(LTEST_ROOT_DIR)/*.cpp)

TARGET = ./testapp
INCLUDES = -I$(LTEST_ROOT_DIR) -I$(TEST_ROOT_DIR) -I$(KII_BUILD_DIR)/usr/local/include -I$(ROOT_DIR)/khc/include -I$(ROOT_DIR)/jkii/include -I$(ROOT_DIR)/jkii/libs/jsmn


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

kii:
	mkdir -p $(KII_BUILD_DIR)
	cd build-kii && cmake ../../../../kii && make && make DESTDIR=. install

$(TARGET): kii $(SRCS)
	g++ $(CXXFLAGS) $(SRCS) $(LD_FLAGS) $(INCLUDES) $(LIBS) -o $(TARGET)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(KII_BUILD_DIR)/usr/local/lib \
	$(TESTCMD)

clean:
	touch $(TARGET)
	rm $(TARGET)
	rm -f *.o
	rm -rf $(KII_BUILD_DIR)

.PHONY: all clean copy kii

