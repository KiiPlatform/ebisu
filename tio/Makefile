DOC_OUTPUT_DIR=html

all: clean doc linux-app

doc:
	doxygen

linux-app:
	make -C linux-sample

clean:
	make -C linux-sample clean
	rm -rf $(DOC_OUTPUT_DIR)

.PHONY: all linux-app
