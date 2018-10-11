build:
	make -C kii

doc:
	make doc -C kii

small-test:
	make test -C tests/small_test/khc

large-test:
	make test -C tests/large_test/kii
	make test -C tests/large_test/khc

test: small-test large-test

.PHONY: build doc small-test large-test
