build:
	make -C kii

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
