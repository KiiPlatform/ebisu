ROOT_DIR=../../..
TEST_ROOT_DIR=../..
KHC_ROOT_DIR=$(ROOT_DIR)/khc

LIB_SRCS=$(wildcard $(KHC_ROOT_DIR)/src/*.c)
LIB_SRCS+=$(wildcard $(KHC_ROOT_DIR)/src/*.h)
LIB_SRCS+=$(wildcard $(KHC_ROOT_DIR)/include/*.h)
LIB_BUILD_DIR=build-khc

TARGET=./testapp
INCLUDES=-I. -I$(TEST_ROOT_DIR) -I$(KHC_ROOT_DIR)/src -I$(LIB_BUILD_DIR)/include
LIBS_DIR=-L$(LIB_BUILD_DIR)/lib

LIBS=-lkhc
SRCS=$(wildcard *.cpp)
SRCS+=$(wildcard ../*.cpp)
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

all: clean test

$(LIB_BUILD_DIR): $(LIB_SRCS)
	mkdir -p $(LIB_BUILD_DIR)
	cd $(LIB_BUILD_DIR) && cmake -DCMAKE_INSTALL_PREFIX=. ../$(KHC_ROOT_DIR) && make install

$(TARGET): $(SRCS) $(LIB_BUILD_DIR)
	g++ -g $(SRCS) $(LIBS_DIR) $(LIBS) $(INCLUDES) -o $(TARGET) -std=c++11

build: $(TARGET)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIB_BUILD_DIR)/lib \
	$(TESTCMD)

clean:
	rm -f $(TARGET)
	rm -rf $(TEST_RESULT_DIR)
	rm -rf $(LIB_BUILD_DIR)

.PHONY: build clean test
