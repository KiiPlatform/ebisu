build:
	make -C kii

doc:
	make doc -C kii

stest-khc:
	make test -C tests/small_test/khc

stest-tio:
	make test -C tests/small_test/tio

stest-kii_json:
	make test -C tests/small_test/kii_json

stest: stest-khc stest-tio stest-kii_json

ltest-khc:
	make test -C tests/large_test/khc

ltest-kii:
	make test -C tests/large_test/kii

ltest: ltest-khc ltest-kii

test: stest ltest

.PHONY: build doc stest-khc stest-tio stest-kii_json stest ltest-khc ltest-kii ltest
