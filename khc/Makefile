SDK_DEPS=$(wildcard src/*.c)
SDK_DEPS+=$(wildcard include/*.h)
SDK_DEPS+=CMakeLists.txt

CMAKE_BUILD_DIR=build
DOC_OUTPUT_DIR=html

all: build-sdk doc

# khc is a component of the top-level project, so this configures the root and
# builds just the khc target out of it.
build-sdk: $(SDK_DEPS)
	cmake -S .. -B $(CMAKE_BUILD_DIR)
	cmake --build $(CMAKE_BUILD_DIR) --target khc

doc:
	doxygen

clean:
	rm -rf $(CMAKE_BUILD_DIR)
	rm -rf $(DOC_OUTPUT_DIR)

.PHONY: build-sdk doc all
