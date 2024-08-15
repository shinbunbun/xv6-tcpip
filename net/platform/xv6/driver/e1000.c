#include "platform.h"

#include "memlayout.h"
#include "pci.h"

#include "util.h"

#include "e1000_dev.h"

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16

struct e1000 {
    struct e1000 *next;
    uint32_t mmio_base;
    struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));;
    struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));;
    uint8_t irq;
};

static struct e1000 *devices;

unsigned int
e1000_reg_read(struct e1000 *e1000, uint16_t reg)
{
    return *(volatile uint32_t *)(e1000->mmio_base + reg);
}

void
e1000_reg_write(struct e1000 *e1000, uint16_t reg, uint32_t val)
{
    *(volatile uint32_t *)(e1000->mmio_base + reg) = val;
}

static uint16_t
e1000_eeprom_read(struct e1000 *e1000, uint8_t addr)
{
    uint32_t eerd;

    e1000_reg_write(e1000, E1000_EERD, E1000_EERD_READ | addr << E1000_EERD_ADDR);
    while (!((eerd = e1000_reg_read(e1000, E1000_EERD)) & E1000_EERD_DONE))
        microdelay(1);
    return (uint16_t)(eerd >> E1000_EERD_DATA);
}

static void
e1000_read_addr_from_eeprom(struct e1000 *e1000, uint8_t *dst)
{
    uint16_t data;

    for (int n = 0; n < 3; n++) {
        data = e1000_eeprom_read(e1000, n);
        dst[n*2+0] = (data & 0xff);
        dst[n*2+1] = (data >> 8) & 0xff;
    }
}

static uint32_t
e1000_resolve_mmio_base(struct pci_func *pcif)
{
    uint32_t mmio_base = 0;

    for (int n = 0; n < 6; n++) {
        if (pcif->reg_base[n] > 0xffff) {
            assert(pcif->reg_size[n] == (1<<17));
            mmio_base = pcif->reg_base[n];
            break;
        }
    }
    return mmio_base;
}

static void
e1000_rx_init(struct e1000 *e1000)
{
    uint64_t base;

    // initialize rx descriptors
    for(int n = 0; n < RX_RING_SIZE; n++) {
        memset(&e1000->rx_ring[n], 0, sizeof(struct rx_desc));
        // alloc DMA buffer
        e1000->rx_ring[n].addr = (uint64_t)V2P(kalloc());
    }
    // setup rx descriptors
    base = (uint64_t)(V2P(e1000->rx_ring));
    e1000_reg_write(e1000, E1000_RDBAL, (uint32_t)(base & 0xffffffff));
    e1000_reg_write(e1000, E1000_RDBAH, (uint32_t)(base >> 32));
    // rx descriptor lengh
    e1000_reg_write(e1000, E1000_RDLEN, (uint32_t)(RX_RING_SIZE * sizeof(struct rx_desc)));
    // setup head/tail
    e1000_reg_write(e1000, E1000_RDH, 0);
    e1000_reg_write(e1000, E1000_RDT, RX_RING_SIZE-1);
    // set tx control register
    e1000_reg_write(e1000, E1000_RCTL, (
        E1000_RCTL_SBP        | /* store bad packet */
        E1000_RCTL_UPE        | /* unicast promiscuous enable */
        E1000_RCTL_MPE        | /* multicast promiscuous enab */
        E1000_RCTL_RDMTS_HALF | /* rx desc min threshold size */
        E1000_RCTL_SECRC      | /* Strip Ethernet CRC */
        E1000_RCTL_LPE        | /* long packet enable */
        E1000_RCTL_BAM        | /* broadcast enable */
        E1000_RCTL_SZ_2048    | /* rx buffer size 2048 */
        0)
    );
}

static void
e1000_tx_init(struct e1000 *e1000)
{
    uint64_t base;

    // initialize tx descriptors
    for (int n = 0; n < TX_RING_SIZE; n++) {
        memset(&e1000->tx_ring[n], 0, sizeof(struct tx_desc));
    }
    // setup tx descriptors
    base = (uint64_t)(V2P(e1000->tx_ring));
    e1000_reg_write(e1000, E1000_TDBAL, (uint32_t)(base & 0xffffffff));
    e1000_reg_write(e1000, E1000_TDBAH, (uint32_t)(base >> 32) );
    // tx descriptor length
    e1000_reg_write(e1000, E1000_TDLEN, (uint32_t)(TX_RING_SIZE * sizeof(struct tx_desc)));
    // setup head/tail
    e1000_reg_write(e1000, E1000_TDH, 0);
    e1000_reg_write(e1000, E1000_TDT, 0);
    // set tx control register
    e1000_reg_write(e1000, E1000_TCTL, (
        E1000_TCTL_PSP | /* pad short packets */
        0)
    );
}

static int
e1000_open(struct e1000 *e1000)
{
    // enable interrupts
    e1000_reg_write(e1000, E1000_IMS, E1000_IMS_RXT0);
    // clear existing pending interrupts
    e1000_reg_read(e1000, E1000_ICR);
    // enable RX/TX
    e1000_reg_write(e1000, E1000_RCTL, e1000_reg_read(e1000, E1000_RCTL) | E1000_RCTL_EN);
    e1000_reg_write(e1000, E1000_TCTL, e1000_reg_read(e1000, E1000_TCTL) | E1000_TCTL_EN);
    // link up
    e1000_reg_write(e1000, E1000_CTL, e1000_reg_read(e1000, E1000_CTL) | E1000_CTL_SLU);
    return 0;
}

void
e1000intr(void)
{
    struct e1000 *e1000;
    int icr;
    uint32_t tail;
    struct rx_desc *desc;

    debugf(">>>");
    for (e1000 = devices; e1000; e1000 = e1000->next) {
        icr = e1000_reg_read(e1000, E1000_ICR);
        if (icr & E1000_ICR_RXT0) {
            while (1) {
                tail = (e1000_reg_read(e1000, E1000_RDT)+1) % RX_RING_SIZE;
                desc = &e1000->rx_ring[tail];
                if (!(desc->status & E1000_RXD_STAT_DD)) {
                    /* EMPTY */
                    break;
                }
                do {
                    if (!(desc->status & E1000_RXD_STAT_EOP)) {
                        errorf("not EOP! this driver does not support packet that do not fit in one buffer");
                        break;
                    }
                    if (desc->errors) {
                        errorf("rx errors (0x%x)", desc->errors);
                        break;
                    }
                    debugf("%d bytes data receive", desc->length);
                    debugdump(P2V((uint32_t)desc->addr), desc->length);
                } while(0);
                desc->status = (uint16_t)(0);
                e1000_reg_write(e1000, E1000_RDT, tail);
            }
            // clear pending interrupts
            e1000_reg_read(e1000, E1000_ICR);
        }
    }
    debugf("<<<");
}

int
e1000init(struct pci_func *pcif)
{
    struct e1000 *e1000;
    uint8_t addr[6];

    e1000 = (struct e1000 *)memory_alloc(sizeof(*e1000));
    if (!e1000) {
        errorf("memory_alloc() failure");
        return -1;
    }
    pci_func_enable(pcif);
    e1000->mmio_base = e1000_resolve_mmio_base(pcif);
    e1000_read_addr_from_eeprom(e1000, addr);
    debugf("addr: %02x:%02x:%02x:%02x:%02x:%02x",
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    for (int n = 0; n < 128; n++)
        e1000_reg_write(e1000, E1000_MTA + (n << 2), 0);
    e1000_rx_init(e1000);
    e1000_tx_init(e1000);
    e1000->irq = pcif->irq_line;
    e1000->next = devices;
    devices = e1000;
    ioapicenable(e1000->irq, ncpu - 1);
    e1000_open(e1000);
    infof("initialized, irq=%u", e1000->irq);
    return 0;
}
