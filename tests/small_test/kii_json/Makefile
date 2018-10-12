ROOT_DIR=../../..
TEST_ROOT_DIR=../..
KII_JSON_DIR=$(ROOT_DIR)/kii_json

SOURCE = $(wildcard src/*.cpp)

LIBDIRS = -L$(OUTDIR)
LIBS= -lkiijson
INCLUDES=-I$(TEST_ROOT_DIR) -I$(KII_JSON_DIR)/include -I$(KII_JSON_DIR)/libs/jsmn

OUTDIR = ./build
TARGET=testapp
TARGET_PATH = $(OUTDIR)/$(TARGET)
LIBKIIJSON=$(OUTDIR)/libkiijson.so

TEST_RESULT_DIR=$(OUTDIR)/test-results/junit
TEST_RESULT=$(TEST_RESULT_DIR)/test-result.xml

ifdef MEMCHECK
TESTCMD=valgrind --leak-check=yes ./$(TARGET)
else
TESTCMD=./$(TARGET)
endif

ifdef JUNIT
TESTCMD+= -r junit -o ../$(TEST_RESULT)
endif

all: clean $(TARGET_PATH)

$(OUTDIR):
	mkdir -p build

$(LIBKIIJSON): $(OUTDIR)
	make -C $(KII_JSON_DIR)
	cp $(KII_JSON_DIR)/libkiijson.so $(OUTDIR)

$(TARGET_PATH): $(LIBKIIJSON)
	g++ $(CFLAGS) $(SOURCE) $(INCLUDES) $(LIBDIRS) $(LIBS) -std=c++11 -o $@

test: $(TARGET_PATH)
	mkdir -p $(TEST_RESULT_DIR)
	cd $(OUTDIR) && LD_LIBRARY_PATH=. $(TESTCMD)

clean:
	rm -rf $(OUTDIR)
	rm -f $(LIBKIIJSON)
	rm -rf $(TEST_RESULT_DIR)
