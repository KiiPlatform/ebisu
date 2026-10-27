#include "net.h"

#include <FreeRTOS.h>
#include <semphr.h>

#include <FreeRTOS_IP.h>

#include "log.h"
#include "random.h"

/* From FreeRTOS-Plus-TCP's driver for the LAN9118, which has no header. */
NetworkInterface_t* pxMPS2_FillInterfaceDescriptor(BaseType_t xEMACIndex,
        NetworkInterface_t* pxInterface);

static NetworkInterface_t interface;
static NetworkEndPoint_t end_point;
static SemaphoreHandle_t network_up;

/* Locally administered, in the range QEMU gives its devices. A device has the
   address its manufacturer assigned. */
static const uint8_t mac_address[ipMAC_ADDRESS_LENGTH_BYTES] = {
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56
};

BaseType_t net_init(void)
{
    network_up = xSemaphoreCreateBinary();
    configASSERT(network_up != NULL);
    /* No address, gateway or DNS server of its own: DHCP gives them. */
    static const uint8_t none[ipIP_ADDRESS_LENGTH_BYTES] = { 0 };
    pxMPS2_FillInterfaceDescriptor(0, &interface);
    FreeRTOS_FillEndPoint(&interface, &end_point, none, none, none, none, mac_address);
    end_point.bits.bWantDHCP = pdTRUE;
    return FreeRTOS_IPInit_Multi();
}

void net_wait_until_up(void)
{
    xSemaphoreTake(network_up, portMAX_DELAY);
    uint32_t address = 0;
    FreeRTOS_GetEndPointConfiguration(&address, NULL, NULL, NULL, &end_point);
    char text[16];
    FreeRTOS_inet_ntoa(address, text);
    sample_log("network up: %s", text);
}

/* FreeRTOS-Plus-TCP's hooks. */

/* When DHCP has given the end point an address, and at each renewal. */
void vApplicationIPNetworkEventHook_Multi(eIPCallbackEvent_t eNetworkEvent,
        struct xNetworkEndPoint* pxEndPoint)
{
    (void)pxEndPoint;
    if (eNetworkEvent == eNetworkUp) {
        xSemaphoreGive(network_up);
    } else {
        sample_log("network down");
    }
}

/* For DHCP's and DNS's transaction IDs, among others. */
BaseType_t xApplicationGetRandomNumber(uint32_t* pulNumber)
{
    return random_bytes(NULL, (unsigned char*)pulNumber, sizeof(*pulNumber)) == 0 ?
        pdTRUE : pdFALSE;
}

/* A TCP connection's initial sequence number, unpredictable so that no one
   off the path can inject into it (RFC 6528). 0 refuses the connection. */
uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
        uint16_t usSourcePort, uint32_t ulDestinationAddress,
        uint16_t usDestinationPort)
{
    (void)ulSourceAddress;
    (void)usSourcePort;
    (void)ulDestinationAddress;
    (void)usDestinationPort;
    uint32_t number = 0;
    if (random_bytes(NULL, (unsigned char*)&number, sizeof(number)) != 0) {
        return 0;
    }
    return number;
}
