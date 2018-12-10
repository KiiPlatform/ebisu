SDK_DEPS=$(wildcard src/*.c)
SDK_DEPS+=$(wildcard include/*.h)
SDK_DEPS+=CMakeLists.txt

CMAKE_BUILD_DIR=build
DOC_OUTPUT_DIR=html

all: build-sdk doc

$(CMAKE_BUILD_DIR):
	mkdir -p build

CMAKE_MAKEFILE=build/Makefile

$(CMAKE_MAKEFILE): $(CMAKE_BUILD_DIR)
	cd build && cmake ..

build-sdk: $(CMAKE_MAKEFILE) $(SDK_DEPS)
	make -C $(CMAKE_BUILD_DIR)

doc:
	doxygen

clean:
	rm -rf $(CMAKE_BUILD_DIR)
	rm -rf $(DOC_OUTPUT_DIR)

.PHONY: build-sdk doc all
