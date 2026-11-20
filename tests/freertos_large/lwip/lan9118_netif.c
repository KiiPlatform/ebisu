/* lwIP's interface on the board's LAN9118, over Arm's driver for it, which
   FreeRTOS-Plus-TCP ships for the same board: frames go out from lwIP's
   thread, and come in from a task of the interface's own, which the
   controller's interrupt wakes, as FreeRTOS-Plus-TCP's own interface has it.
   See lan9118_netif.h. */
#include "lan9118_netif.h"

#include <stdbool.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <lwip/opt.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>

#include "board.h"
#include "smsc9220_eth_drv.h"

/* The controller on the MPS2 AN385 board. */
#define LAN9118_BASE 0x40200000u
#define ETHERNET_IRQ 13u
#define NVIC_ISER (*(volatile uint32_t*)0xE000E100u)
#define NVIC_ICER (*(volatile uint32_t*)0xE000E180u)

/* An Ethernet frame, its CRC and a VLAN tag included, in whole words, as the
   driver moves them. */
#define FRAME_SIZE 1524u
/* Above the suite's tasks, below lwIP's. */
#define RX_PRIORITY (configMAX_PRIORITIES - 3)
#define RX_STACK 512

static const struct smsc9220_eth_dev_cfg_t config = { .base = LAN9118_BASE };
static struct smsc9220_eth_dev_data_t data = { .state = 0 };
static const struct smsc9220_eth_dev_t device = { &config, &data };

static SemaphoreHandle_t received;
static struct netif* rx_netif;

/* Locally administered, in the range QEMU gives its devices, as the FreeRTOS
   sample's. */
static const uint8_t mac_address[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

static void wait_ms(uint32_t ms)
{
    vTaskDelay((ms * configTICK_RATE_HZ + 999) / 1000 + 1);
}

/* From lwIP's thread: the frame, in one write, from a buffer of whole words,
   as the driver writes the FIFO a word at a time. */
static err_t output(struct netif* netif, struct pbuf* p)
{
    static uint32_t frame[FRAME_SIZE / 4];
    (void)netif;
    if (p->tot_len > FRAME_SIZE) {
        return ERR_BUF;
    }
    const u16_t length = pbuf_copy_partial(p, frame, p->tot_len, 0);
    if (smsc9220_send_by_chunks(&device, length, true, (const char*)frame, length)
            != SMSC9220_ERROR_NONE) {
        return ERR_IF;
    }
    return ERR_OK;
}

/* What the controller has received, frame by frame, to lwIP's thread. A frame
   lwIP has no room for is read all the same, and dropped. */
static void receive(void* parameter)
{
    static uint32_t frame[FRAME_SIZE / 4];
    (void)parameter;
    for (;;) {
        (void)xSemaphoreTake(received, pdMS_TO_TICKS(100));
        uint32_t length;
        while ((length = smsc9220_peek_next_packet_size(&device)) != 0) {
            if (length > FRAME_SIZE) {
                board_fatal("a frame of %lu bytes", (unsigned long)length);
            }
            length = smsc9220_receive_by_chunks(&device, (char*)frame, length);
            struct pbuf* p = pbuf_alloc(PBUF_RAW, (u16_t)length, PBUF_POOL);
            if (p == NULL) {
                continue;
            }
            if (pbuf_take(p, frame, (u16_t)length) != ERR_OK
                    || rx_netif->input(p, rx_netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
        smsc9220_enable_interrupt(&device, SMSC9220_INTERRUPT_RX_STATUS_FIFO_LEVEL);
    }
}

/* Wakes the task, which enables the interrupt again once it has read what
   there is. */
void EthernetISR(void)
{
    extern uint32_t get_irq_status(const struct smsc9220_eth_dev_t* dev);
    BaseType_t woken = pdFALSE;
    if (get_irq_status(&device) & (1u << SMSC9220_INTERRUPT_RX_STATUS_FIFO_LEVEL)) {
        smsc9220_disable_interrupt(&device, SMSC9220_INTERRUPT_RX_STATUS_FIFO_LEVEL);
        (void)xSemaphoreGiveFromISR(received, &woken);
    }
    smsc9220_clear_all_interrupts(&device);
    portYIELD_FROM_ISR(woken);
}

err_t lan9118_netif_init(struct netif* netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = etharp_output;
    netif->linkoutput = output;
    netif->mtu = 1500;
    netif->hwaddr_len = sizeof(mac_address);
    memcpy(netif->hwaddr, mac_address, sizeof(mac_address));
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    if (smsc9220_init(&device, wait_ms) != SMSC9220_ERROR_NONE) {
        return ERR_IF;
    }
    NVIC_ICER = 1u << ETHERNET_IRQ;
    smsc9220_disable_all_interrupts(&device);
    smsc9220_clear_all_interrupts(&device);
    smsc9220_set_fifo_level_irq(&device, SMSC9220_FIFO_LEVEL_IRQ_RX_STATUS_POS,
            SMSC9220_FIFO_LEVEL_IRQ_LEVEL_MIN);
    uint32_t low = 0;
    uint32_t high = 0;
    memcpy(&low, mac_address, 4);
    memcpy(&high, mac_address + 4, 2);
    if (smsc9220_mac_regwrite(&device, SMSC9220_MAC_REG_OFFSET_ADDRL, low) != 0
            || smsc9220_mac_regwrite(&device, SMSC9220_MAC_REG_OFFSET_ADDRH, high) != 0) {
        return ERR_IF;
    }

    rx_netif = netif;
    received = xSemaphoreCreateBinary();
    if (received == NULL
            || xTaskCreate(receive, "EMAC", RX_STACK, NULL, RX_PRIORITY, NULL) != pdPASS) {
        return ERR_MEM;
    }
    smsc9220_enable_interrupt(&device, SMSC9220_INTERRUPT_RX_STATUS_FIFO_LEVEL);
    NVIC_ISER = 1u << ETHERNET_IRQ;
    return ERR_OK;
}
