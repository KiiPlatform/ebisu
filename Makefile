BUILD_DIR?=build
BUILD_TYPE?=Debug
CMAKE_FLAGS?=
# `-j` without a number lets ctest use every core.
CTEST_FLAGS?=--output-on-failure -j

# A thin façade over CMake and CTest, so the familiar target names keep working.
# Run ctest directly for anything finer grained:
#   ctest --test-dir build -R mqtt --rerun-failed
#   ctest --test-dir build -L large --repeat until-pass:2
#   ctest --test-dir build -T memcheck

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)

# Compiles every library and every test. This used to run `make -C kii`, whose
# default target is `all: clean doc` -- so it built doxygen output and not one
# line of C, and a compile break could only be caught by the test targets.
build: configure
	cmake --build $(BUILD_DIR) --parallel

doc:
	make doc -C kii

# Self-contained suites: no network, no credentials.
stest: build
	ctest --test-dir $(BUILD_DIR) -L small $(CTEST_FLAGS)

stest-khc: build
	ctest --test-dir $(BUILD_DIR) -R small_test_khc $(CTEST_FLAGS)

stest-kii: build
	ctest --test-dir $(BUILD_DIR) -R small_test_kii $(CTEST_FLAGS)

stest-tio: build
	ctest --test-dir $(BUILD_DIR) -R small_test_tio $(CTEST_FLAGS)

stest-jkii: build
	ctest --test-dir $(BUILD_DIR) -R small_test_jkii $(CTEST_FLAGS)

# Against a live Kii application; needs APP_ID and DEFAULT_SITE in the
# environment, without which they are not configured at all.
ltest: build
	ctest --test-dir $(BUILD_DIR) -L large $(CTEST_FLAGS)

ltest-khc: build
	ctest --test-dir $(BUILD_DIR) -R large_test_khc $(CTEST_FLAGS)

ltest-kii: build
	ctest --test-dir $(BUILD_DIR) -R large_test_kii $(CTEST_FLAGS)

test: build
	ctest --test-dir $(BUILD_DIR) $(CTEST_FLAGS)

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


.PHONY: configure build doc test stest-khc stest-kii stest-tio stest-jkii stest ltest-khc ltest-kii ltest clean
