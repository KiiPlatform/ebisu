TARGET_DIR=freertos_ebisu_demo
TIO_DIR=$(TARGET_DIR)/tio

all:
	mkdir -p $(TIO_DIR)
	cp ../../jkii/src/*.c $(TIO_DIR)
	cp ../../jkii/include/*.h $(TIO_DIR)
	cp ../../jkii/libs/jsmn/*.c $(TIO_DIR)
	cp ../../jkii/libs/jsmn/*.h $(TIO_DIR)
	cp ../../khc/src/*.c $(TIO_DIR)
	cp ../../khc/src/*.h $(TIO_DIR)
	cp ../../khc/include/*.h $(TIO_DIR)
	cp ../../kii/*.c $(TIO_DIR)
	cp ../../kii/*.h $(TIO_DIR)
	cp ../../kii/include/*.h $(TIO_DIR)
	cp ../*.c $(TIO_DIR)
	cp ../*.h $(TIO_DIR)
	cp ../include/*.h $(TIO_DIR)
	cp ./*.c $(TARGET_DIR)
	cp ./*.h $(TARGET_DIR)
	cp -r ccs $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)

.PHONY: all
