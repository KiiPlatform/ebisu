build:
	make -C kii

doc:
	make doc -C kii

small-test:
	make test -C tests/small_test/khc
	make test -C tests/small_test/tio
	make test -C tests/small_test/kii_json

large-test:
	make test -C tests/large_test/kii
	make test -C tests/large_test/khc

test: small-test large-test

.PHONY: build doc small-test large-test
