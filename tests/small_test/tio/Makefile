ROOT_DIR=../../..
TEST_ROOT_DIR=../../
TIO_DIR=$(ROOT_DIR)/tio

LIB_BUILD_DIR=build-tio
LIBS_DIR=-L$(LIB_BUILD_DIR)/lib
LIB_DEPS=$(wildcard $(TIO_DIR)/*.c)
LIB_DEPS+=$(wildcard $(TIO_DIR)/*.h)
LIB_DEPS+=$(wildcard $(TIO_DIR)/include*.h)

TARGET=./small-test
TARGET_SRCS=$(wildcard *.cpp)

TEST_RESULT_DIR=test-results/junit
TEST_RESULT=$(TEST_RESULT_DIR)/test-result.xml

INCLUDES=-I. -I$(TEST_ROOT_DIR) -I$(LIB_BUILD_DIR)/include
# tio private headers.
INCLUDES+=-I$(TIO_DIR)
LIBS_DIR=-L$(LIB_BUILD_DIR)/lib
LIBS=-ltio

ifdef MEMCHECK
TESTCMD=valgrind --leak-check=yes $(TARGET)
else
TESTCMD=$(TARGET)
endif

ifdef JUNIT
TESTCMD+= -r junit -o $(TEST_RESULT)
endif

all: test

$(LIB_BUILD_DIR): $(LIB_DEPS)
	mkdir -p $(LIB_BUILD_DIR)
	cd 	$(LIB_BUILD_DIR) && cmake -DCMAKE_INSTALL_PREFIX=. ../$(TIO_DIR) && make install

$(TARGET): $(TARGET_SRCS) $(LIB_BUILD_DIR)
	g++ -g $(TARGET_SRCS) $(LIBS_DIR) $(LIBS) $(INCLUDES) -o $(TARGET) -std=c++11

build: $(TARGET)

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIB_BUILD_DIR)/lib \
	$(TESTCMD)

clean:
	rm -f $(TARGET)
	rm -rf $(TEST_RESULT_DIR)
	rm -rf $(LIB_BUILD_DIR)

.PHONY: build test clean
