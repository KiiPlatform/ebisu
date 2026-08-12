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

# Everything is built out of tree now, so there is one directory to remove
# instead of a per-test-directory list of build trees, binaries and .dSYM
# bundles that had to be kept in step by hand.
clean:
	rm -rf $(BUILD_DIR)


.PHONY: configure build doc test stest-khc stest-kii stest-tio stest-jkii stest ltest-khc ltest-kii ltest clean
