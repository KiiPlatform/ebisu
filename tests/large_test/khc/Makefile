ROOT_DIR=../../..
TEST_ROOT_DIR=../..
LTEST_ROOT_DIR=$(TEST_ROOT_DIR)/large_test

LIB_SRCS=$(wildcard $(ROOT_DIR)/khc/src/*.c)
LIB_SRCS+=$(wildcard $(ROOT_DIR)/khc/src/*.h)
LIB_SRCS+=$(wildcard $(ROOT_DIR)/khc/include/*.h)
LIB_BUILD_DIR=build-khc

TARGET=./large-test
INCLUDES=-I. -I$(LTEST_ROOT_DIR) -I$(TEST_ROOT_DIR) -I$(ROOT_DIR)/khc/src -I$(LIB_BUILD_DIR)/include
LIBS_DIR=-L$(LIB_BUILD_DIR)/lib

LIBS=-lkhc -lssl -lcrypto
SRCS=$(wildcard *.cpp)
SRCS+=$(wildcard *.c)
SRCS+=$(wildcard $(LTEST_ROOT_DIR)/*.cpp)
TEST_RESULT_DIR=test-results/junit
TEST_RESULT=$(TEST_RESULT_DIR)/test-result.xml

CPPFLAGS+=-g -O0 -ggdb

ifdef PLAIN_HTTP
CPPFLAGS += -DPLAIN_HTTP
endif

ifdef MEMCHECK
TESTCMD=valgrind --leak-check=yes $(TARGET)
else
TESTCMD=$(TARGET)
endif

ifdef JUNIT
TESTCMD+= -r junit -o $(TEST_RESULT)
endif

CPPFLAGS += $(if $(value DEFAULT_SITE), -D_DEFAULT_SITE=$(DEFAULT_SITE), )
CPPFLAGS += $(if $(value APP_ID), -D_APP_ID=$(APP_ID), )
CPPFLAGS += $(if $(value SOCKET_LOG), -DSOCKET_LOG, )

all: clean test

$(LIB_BUILD_DIR): $(LIB_SRCS)
	mkdir -p $(LIB_BUILD_DIR)
	cd $(LIB_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=. ../$(ROOT_DIR)/khc && make install

$(TARGET): $(SRCS) $(LIB_BUILD_DIR)
	g++ $(SRCS) $(LIBS_DIR) $(LIBS) $(INCLUDES) $(CPPFLAGS) -o $(TARGET) -std=c++11

build: $(TARGET)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	DYLD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIB_BUILD_DIR)/lib \
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIB_BUILD_DIR)/lib \
	$(TESTCMD)

clean:
	rm -f $(TARGET)
	rm -rf $(TEST_RESULT_DIR)
	rm -rf $(LIB_BUILD_DIR)

.PHONY: build clean test
