#include "mraa.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "kii.h"
#include "kii_init_impl.h"

void received_callback(kii_t* kii, char* buffer, size_t buffer_size) {
	char copy[1024];
	memset(copy, 0x00, sizeof(copy));
	strncpy(copy, buffer, sizeof(copy));
	printf("buffer_size: %lu\n", buffer_size);
	printf("recieve message: %s\n", copy);
}

const char EX_AUTH_VENDOR_ID[] = "sweet_wks_2";
const char EX_AUTH_VENDOR_PASS[] = "password";
const char EX_AUTH_VENDOR_TYPE[] = "my_type";

const char EX_APP_SITE[] = "JP";
const char EX_APP_ID[] = "2fc66b6f";
const char EX_APP_KEY[] = "c273738ade7edf22bcdc66a329481c5f";

#define EX_BUFFER_SIZE 4096
#define EX_MQTT_BUFFER_SIZE 2048

const char EX_OBJECT_ID[] = "my_object";
const char EX_BUCKET_NAME[] = "my_bucket";
const char EX_TOPIC_NAME[] = "my_topic";
const char EX_OBJECT_DATA[] = "{}";
const char EX_BODY_DATA[] = "Hello world !\n";
const char EX_ENDPOINT_NAME[] = "test_topic";
/*
 * On board LED blink C example
 *
 * Demonstrate how to blink the on board LED, writing a digital value to an
 * output pin using the MRAA library.
 * No external hardware is needed.
 *
 * - digital out: on board LED
 *
 * Additional linker flags: none
 */

int main()
{
	kii_t kii;
	kii_author_t author;
	char buffer[EX_BUFFER_SIZE];
	char mqtt_buffer[EX_MQTT_BUFFER_SIZE];
	size_t buffer_size = EX_BUFFER_SIZE;
	size_t mqtt_buffer_size = EX_MQTT_BUFFER_SIZE;


	char scope_id[128];
	int ret;
	kii_bucket_t bucket;

	char object_id[KII_OBJECTID_SIZE + 1];

	memset(buffer, 0x00, buffer_size);
	memset(mqtt_buffer, 0x00, mqtt_buffer_size);

	kii_impl_init(&kii, EX_APP_SITE, EX_APP_ID, EX_APP_KEY);
	kii.kii_core.http_context.buffer = buffer;
	kii.kii_core.http_context.buffer_size = buffer_size;
	kii.kii_core.http_context.socket_context.app_context = NULL;
	kii.mqtt_buffer = mqtt_buffer;
	kii.mqtt_buffer_size = mqtt_buffer_size;
	memset(&author, 0x00, sizeof(kii_author_t));

	kii.kii_core.author = author;

	memset(&bucket, 0x00, sizeof(kii_bucket_t));
	bucket.scope = KII_SCOPE_THING;
	bucket.bucket_name = (char*)EX_BUCKET_NAME;
	memset(scope_id, 0x00, sizeof(scope_id));
	sprintf(scope_id, "VENDOR_THING_ID:%s", EX_AUTH_VENDOR_ID);
	bucket.scope_id = scope_id;

	printf("authentication\n");
	ret = kii_thing_authenticate(&kii, EX_AUTH_VENDOR_ID, EX_AUTH_VENDOR_PASS);
	if(ret == 0) {
		printf("success!\n");
	} else {
		printf("failed!\n");
	}
	printf("create new object on initial \n");
	memset(object_id, 0x00, sizeof(object_id));
	ret = kii_object_create(&kii, &bucket, EX_OBJECT_DATA, NULL, object_id);
	if(ret == 0) {
		printf("success!\n");
	} else {
		printf("failed!\n");
	}
	// select onboard LED pin based on the platform type
	// create a GPIO object from MRAA using it
	mraa_platform_t platform = mraa_get_platform_type();
	mraa_gpio_context d_pin = NULL;
	switch (platform) {
	case MRAA_INTEL_GALILEO_GEN1:
		d_pin = mraa_gpio_init_raw(3);
		break;
	case MRAA_INTEL_GALILEO_GEN2:
		d_pin = mraa_gpio_init(13);
		break ;
	case MRAA_INTEL_EDISON_FAB_C:
		d_pin = mraa_gpio_init(13);
		break;
	default:
		fprintf(stderr, "Unsupported platform, exiting");
		return MRAA_ERROR_INVALID_PLATFORM;
	}
	if (d_pin == NULL) {
		fprintf(stderr, "MRAA couldn't initialize GPIO, exiting");
		return MRAA_ERROR_UNSPECIFIED;
	}

	// set the pin as output
	if (mraa_gpio_dir(d_pin, MRAA_GPIO_OUT) != MRAA_SUCCESS) {
		fprintf(stderr, "Can't set digital pin as output, exiting");
		return MRAA_ERROR_UNSPECIFIED;
	};
	mraa_gpio_context gpio;

	gpio = mraa_gpio_init(2);
	// loop forever toggling the on board LED every second
	for (;;) {
		if(mraa_gpio_read(gpio)){
			printf("create new object on pressed\n");
			memset(object_id, 0x00, sizeof(object_id));
			ret = kii_object_create(&kii, &bucket, "{'event':'button pressed'}", NULL, object_id);
			if(ret == 0) {
				printf("success!\n");
			} else {
				printf("failed!\n");
			}
		}
		sleep(1);
	}

	return MRAA_SUCCESS;
}
