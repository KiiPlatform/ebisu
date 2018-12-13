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

.PHONY: build doc stest-khc stest-kii stest-tio stest-jkii stest ltest-khc ltest-kii ltest
