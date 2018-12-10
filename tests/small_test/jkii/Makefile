ROOT_DIR=../../..
TEST_ROOT_DIR=../..

JKII_BUILD_DIR=build-jkii

SOURCE = $(wildcard src/*.cpp)

LIBDIRS = -L$(JKII_BUILD_DIR)/usr/local/lib
LIBS= -ljkii
INCLUDES=-I$(TEST_ROOT_DIR) -I$(JKII_BUILD_DIR)/usr/local/include

TARGET=testapp

TEST_RESULT_DIR=test-results/junit
TEST_RESULT=$(TEST_RESULT_DIR)/test-result.xml

ifdef MEMCHECK
TESTCMD=valgrind --leak-check=yes ./$(TARGET)
else
TESTCMD=./$(TARGET)
endif

ifdef JUNIT
TESTCMD+= -r junit -o $(TEST_RESULT)
endif

all: clean $(TARGET)

jkii:
	mkdir -p $(JKII_BUILD_DIR)
	cd $(JKII_BUILD_DIR) && cmake $(ROOT_DIR)/../jkii && make &&  make DESTDIR=. install

$(TARGET): jkii
	g++ $(CFLAGS) $(SOURCE) $(INCLUDES) $(LIBDIRS) $(LIBS) -std=c++11 -o $@

test: $(TARGET)
	mkdir -p $(TEST_RESULT_DIR)
	LD_LIBRARY_PATH=$(JKII_BUILD_DIR)/usr/local/lib $(TESTCMD)

clean:
	rm -f $(TARGET)
	rm -rf $(JKII_BUILD_DIR)
	rm -rf $(TEST_RESULT_DIR)

.PHONY: all jkii test clean
