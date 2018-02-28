build:
	make -C kii

doc:
	make doc -C kii

small-test:
	make test -C tests/small_test

large-test:
	make test -C tests/large_test

.PHONY: build doc small-test large-test
