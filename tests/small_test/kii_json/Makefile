ROOT_DIR=../../..
TEST_ROOT_DIR=../..
KII_JSON_DIR=$(ROOT_DIR)/kii_json

LIBKIIJSON=libkiijson.so

SOURCE = $(wildcard src/*.c)
TARGET = $(OUTDIR)/testapp
LIBSDIR = $(OUTDIR)
LIBS= -lkiijson
INCLUDES=-I$(TEST_ROOT_DIR) -I$(KII_JSON_DIR)/include -I$(KII_JSON_DIR)/libs/jsmn
OUTDIR=target

all: clean $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $@

$(LIBKIIJSON):
	make -C $(KII_JSON_DIR)
	cp $(KII_JSON_DIR)/$(LIBKIIJSON) $(OUTDIR)

$(TARGET): $(OUTDIR) $(LIBKIIJSON)
	g++ $(CFLAGS) $(SOURCE) $(INCLUDES) -L $(LIBSDIR) $(LIBS) -std=c++11 -o $@

test: $(TARGET)
	cd target && export LD_LIBRARY_PATH=. && ./testapp

clean:
	rm -rf $(OUTDIR)
