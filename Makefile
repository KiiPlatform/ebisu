BUILD_DIR?=build
BUILD_TYPE?=Debug

# Compiles every library. This used to run `make -C kii`, whose default target
# is `all: clean doc` -- so it built doxygen output and not one line of C, and
# a compile break could only ever be caught by the test targets.
build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR)

doc:
	make doc -C kii

stest-khc:
	make test -C tests/small_test/khc

stest-kii:
	make test -C tests/small_test/kii

stest-tio:
	make test -C tests/small_test/tio

stest-jkii:
	make test -C tests/small_test/jkii

stest: stest-khc stest-kii stest-tio stest-jkii

ltest-khc:
	make test -C tests/large_test/khc

ltest-kii:
	make test -C tests/large_test/kii

ltest: ltest-khc ltest-kii

test: stest ltest

clean:
	rm -rf $(BUILD_DIR)
	# jkii - small test
	rm -rf ./tests/small_test/jkii/build-jkii
	rm -f ./tests/small_test/jkii/testapp
	# khc - small test
	rm -rf ./tests/small_test/khc/build-khc
	rm -f ./tests/small_test/khc/testapp
	rm -rf ./tests/small_test/khc/testapp.dSYM
	# kii - small test
	rm -rf ./tests/small_test/kii/build-kii
	rm -f ./tests/small_test/kii/small-test
	rm -rf ./tests/small_test/kii/small-test.dSYM
	# tio - small test
	rm -rf ./tests/small_test/tio/build-tio
	rm -f ./tests/small_test/tio/small-test
	rm -rf ./tests/small_test/tio/small-test.dSYM
	# khc - large test
	rm -rf ./tests/large_test/khc/build-khc
	rm -f ./tests/large_test/khc/large-test
	rm -rf ./tests/large_test/khc/large-test.dSYM
	# kii - large test
	rm -rf ./tests/large_test/kii/build-kii
	rm -f ./tests/large_test/kii/testapp
	rm -rf ./tests/large_test/kii/testapp.dSYM


.PHONY: build doc stest-khc stest-kii stest-tio stest-jkii stest ltest-khc ltest-kii ltest clean
