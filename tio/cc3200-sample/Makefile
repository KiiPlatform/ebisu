TARGET_DIR=freertos_ebisu_demo
JKII_DIR=$(TARGET_DIR)/jkii
KHC_DIR=$(TARGET_DIR)/khc
KII_DIR=$(TARGET_DIR)/kii
TIO_DIR=$(TARGET_DIR)/tio

all:
	mkdir -p $(JKII_DIR)
	cp -r ../../jkii/src $(JKII_DIR)
	cp -r ../../jkii/include $(JKII_DIR)
	cp -r ../../jkii/libs $(JKII_DIR)
	mkdir -p $(KHC_DIR)
	cp -r ../../khc/src $(KHC_DIR)
	cp -r ../../khc/include $(KHC_DIR)
	mkdir -p $(KII_DIR)
	cp ../../kii/*.c $(KII_DIR)
	cp ../../kii/*.h $(KII_DIR)
	cp -r ../../kii/include $(KII_DIR)
	mkdir -p $(TIO_DIR)
	cp ../*.c $(TIO_DIR)
	cp ../*.h $(TIO_DIR)
	cp -r ../include $(TIO_DIR)
	mkdir -p $(TARGET_DIR)
	cp ./*.c $(TARGET_DIR)
	cp ./*.h $(TARGET_DIR)
	cp -r ccs/. $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)

.PHONY: all
