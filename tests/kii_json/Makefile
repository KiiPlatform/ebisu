GTEST_PATH=../../gtest-1.7.0
LIBGTEST=libgtest.a
LIBKII=libkiisdk.so

SOURCE = $(wildcard *.c)
TARGET = kii_json_test
LIBS= $(LIBGTEST) $(LIBKII)
INCLUDES= -I$(GTEST_PATH)/include -I../../kii -I../../kii-core

all: clean $(TARGET)

$(LIBKII):
	(pushd ../../kii; make; cp libkiisdk.so ../unit_test/kii_json; popd)

$(LIBGTEST):
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest-all.cc 
	g++ -isystem $(GTEST_PATH)/include -I$(GTEST_PATH) -pthread -c $(GTEST_PATH)/src/gtest_main.cc 
	ar -rv $(LIBGTEST) gtest-all.o gtest_main.o

$(TARGET): $(LIBGTEST) $(LIBKII)
	g++ $(SOURCE) $(INCLUDES) $(LIBS) -o $@

clean:
	rm -f $(LIBGTEST) $(LIBKII) gtest-all.o $(TARGET)
