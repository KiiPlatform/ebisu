LDLIBS=-L. -lkii -Wl,-rpath=./

all: 
#	/opt/buildroot-gcc342/bin/mipsel-linux-uclibc-gcc -O0 -Wall $(LDLIBS) kii_demo.c light.c light_if.c -o kii_demo -pthread
	gcc -O0 -Wall $(LDLIBS) kii_demo.c light.c light_if.c -o kii_demo -pthread		
clean:
	rm -rf kii_demo
