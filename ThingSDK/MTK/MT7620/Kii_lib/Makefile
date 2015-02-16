all: 
#	/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc -shared -O0 -Wall kii.c kii_device.c kii_hal.c kii_mqtt.c kii_object.c kii_push.c kii_extension.c -o libkii.so -pthread
	gcc -shared -O0 -Wall kii.c kii_device.c kii_hal.c kii_mqtt.c kii_object.c kii_push.c kii_extension.c -o libkii.so -pthread

clean:
	rm -rf libkii.so
