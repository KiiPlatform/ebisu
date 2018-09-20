GTEST_PATH=libs/gtest-1.7.0
LIBGTEST=libgtest.a
LIBKIIJSON=libkiijson.so

SOURCE = $(wildcard src/*.c)
TARGET = $(OUTDIR)/testapp
LIBSDIR = $(OUTDIR)
LIBS= -lkiijson
INCLUDES= -I$(GTEST_PATH)/include  -I../include -I../libs/jsmn
OUTDIR=target

all: clean $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $@

$(LIBKIIJSON):
	make FLEXIBLE_JSON_TOKEN=1 -C ../
	cp ../$(LIBKIIJSON) $(OUTDIR)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc -o $(OUTDIR)/gtest-all.o
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc  -o $(OUTDIR)/gtest_main.o
	ar -rv $(OUTDIR)/$(LIBGTEST) $(OUTDIR)/gtest-all.o $(OUTDIR)/gtest_main.o

$(TARGET): $(OUTDIR) $(LIBGTEST) $(LIBKIIJSON)
	g++ $(CFLAGS) $(SOURCE) $(LIBSDIR)/$(LIBGTEST) $(INCLUDES) -L $(LIBSDIR) $(LIBS) -pthread -o $@

test: $(TARGET)
	cd target && export LD_LIBRARY_PATH=. && ./testapp

clean:
	rm -rf $(OUTDIR)
