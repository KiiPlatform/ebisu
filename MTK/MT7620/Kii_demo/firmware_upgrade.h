#ifndef FIRMWARE_UPGRADE_H
#define FIRMWARE_UPGRADE_H

//#define FWUP_PARAM_FILENAME        "/mnt/work/kii_demo/param"
//#define FWUP_DOWNLOAD_FILENAME        "/mnt/work/kii_demo/image"
#define FWUP_PARAM_FILENAME "param"
#define FWUP_DOWNLOAD_FILENAME "image"

#define FWUP_DEFAULT_PORT 80
#define FWUP_SOCKET_BUF_SIZE 2048
#define FWUP_HTTP_HEADER_SIZE 512
#define FWUP_HOST_SIZE 128

void fwup_upgrade(char* url);

#endif
