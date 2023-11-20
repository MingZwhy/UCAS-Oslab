#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/mm.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000; // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

uint32_t tx_head = 0;
uint32_t tx_tail = 0;

uint32_t rx_head = 0;
uint32_t rx_tail = RXDESCS - 1;
static void update_loop(uint64_t ptr)
{
    uint32_t *ptr1 = ptr;
    if (*ptr1 == TXDESCS - 1)
        *ptr1 = 0;
    else
        (*ptr1)++;
}

static void set_tx(int index, ptr_t pa, uint16_t length)
{
    tx_desc_array[index].addr = pa;
    tx_desc_array[index].length = length;
    tx_desc_array[index].cmd = 0x09;
    tx_desc_array[index].status = 0;
}
// tx初始化，包括单个tx结构体和整体的初始化
static void init_tx(int index)
{
    tx_desc_array[index].addr = 0;
    tx_desc_array[index].length = 0;
    tx_desc_array[index].cso = 0;
    tx_desc_array[index].cmd = 0;
    tx_desc_array[index].status = 0;
    tx_desc_array[index].css = 0;
}
static void init_all_tx()
{
    for (int index = 0; index < TXDESCS; index++)
    {
        tx_desc_array[index].addr = 0;
        tx_desc_array[index].length = 0;
        tx_desc_array[index].cso = 0;
        tx_desc_array[index].cmd = 0;
        tx_desc_array[index].status = 1;
        tx_desc_array[index].css = 0;
    }
}

// rx初始化，包括单个rx结构体和整体的初始化，这里注意addr的初始化和tx有所差别
static void init_rx(int index)
{
    rx_desc_array[index].addr = kva2pa(rx_pkt_buffer[index]);
    rx_desc_array[index].length = 0;
    rx_desc_array[index].csum = 0;
    rx_desc_array[index].status = 0;
    rx_desc_array[index].errors = 0;
    rx_desc_array[index].special = 0;
}
static void init_all_rx()
{
    for (int i = 0; i < RXDESCS; i++)
        init_rx(i);
}

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
    /* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

    /* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

    /**
     * Delay to allow any outstanding PCI transactions to complete before
     * resetting the device
     */
    latency(1);

    /* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR))
        ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    init_all_tx();
    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    uint32_t txbal = (uint32_t)(kva2pa(tx_desc_array));
    uint32_t txbah = (uint32_t)(kva2pa(tx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_TDBAL, txbal);
    e1000_write_reg(e1000, E1000_TDBAH, txbah);

    // optimization
    uint32_t txlen = (uint32_t)(TXDESCS * sizeof(struct e1000_tx_desc));
    e1000_write_reg(e1000, E1000_TDLEN, txlen);

    /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, tx_head);
    e1000_write_reg(e1000, E1000_TDT, tx_tail);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    uint32_t tctl_init = E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_CT & 0x00000100) | (E1000_TCTL_COLD & 0x00040000);
    e1000_write_reg(e1000, E1000_TCTL, tctl_init);
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    // 根据MAC数组直接进行赋值即可
    //  MAC {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53}
    uint32_t mac0 = 0x00350a00;
    uint32_t mac1 = 0x8000531e;
    e1000_write_reg_array(e1000, E1000_RA, 0, mac0);
    e1000_write_reg_array(e1000, E1000_RA, 1, mac1);

    /* TODO: [p5-task2] Initialize rx descriptors */
    init_all_rx();

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uint32_t rxbal = (uint32_t)kva2pa(rx_desc_array);
    uint32_t rxbah = (uint32_t)(kva2pa(rx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_RDBAL, rxbal);
    e1000_write_reg(e1000, E1000_RDBAH, rxbah);

    // optimization
    uint32_t rxlen = (uint32_t)(RXDESCS * sizeof(struct e1000_rx_desc));
    e1000_write_reg(e1000, E1000_RDLEN, rxlen);

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, rx_head);
    e1000_write_reg(e1000, E1000_RDT, rx_tail);

    /* TODO: [p5-task2] Program the Receive Control Register */
    uint32_t rctl_init = E1000_RCTL_EN | E1000_RCTL_BAM;
    e1000_write_reg(e1000, E1000_RCTL, rctl_init);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
}

int handle_rx(uint8_t *rxbuffer)
{
    int length = rx_desc_array[rx_tail].length;
    memcpy((uint8_t *)rxbuffer, (uint8_t *)rx_pkt_buffer[rx_tail], length);
    return length;
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    // 刷新一下，保险
    local_flush_dcache();

    // 读取此时此刻的头和尾
    tx_head = e1000_read_reg(e1000, E1000_TDH);
    tx_tail = e1000_read_reg(e1000, E1000_TDT);

    // 若尾巴的status最低位是1，说明硬件已经处理结束，等待软件继续发送
    uint8_t sta = tx_desc_array[tx_tail].status & 0x1;
    if (sta == 0)
    {
        // task4 开启硬件中断
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        // enable_ext();

        current_running->status = TASK_BLOCKED;
        en_queue(&send_queue, &current_running->list);
        do_scheduler();
        e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
    }
    // 寻找物理地址，因为即将要写入物理地址
    ptr_t offset = (ptr_t)txpacket & 0xfff;
    ptr_t ppn = get_page_addr(txpacket) | offset;

    // 设置tx中的项，然后更新尾指针，同时写入对应的寄存器
    set_tx(tx_tail, ppn, length);
    update_loop(&tx_tail);
    e1000_write_reg(e1000, E1000_TDT, tx_tail);

    // 刷新一下，保险
    local_flush_dcache();

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    // 刷新一下，保险
    local_flush_dcache();

    // 读取相应的头和尾巴
    rx_head = e1000_read_reg(e1000, E1000_RDH);
    rx_tail = e1000_read_reg(e1000, E1000_RDT);
    // 起初设置tail为最后一位，假如仍然满足，则相当于没有接受，所以调度，否则tail会更新1
    if (rx_desc_array[rx_head].status == 0 && ((rx_tail + 1) % RXDESCS) == rx_head) //((rx_tail + 1) % RXDESCS) == rx_head)
    {
        // enable_ext();
        current_running->status = TASK_BLOCKED;
        en_queue(&recv_queue, &current_running->list);
        do_scheduler();
        return e1000_poll(rxbuffer);
    }
    else
        update_loop(&rx_tail);
    // 读取接受的相应长度
    int length = handle_rx((uint8_t *)rxbuffer);

    // 处理后可以直接将其归零
    init_rx(rx_tail);

    // 将tail写入寄存器
    e1000_write_reg(e1000, E1000_RDT, rx_tail);

    local_flush_dcache();

    //e1000_echo(rxbuffer,length);
    return length;
}
bool check_send_valid(void)
{
    // uint32_t read_tail = e1000_read_reg(e1000, E1000_TDT);

    if (tx_desc_array[tx_tail].status == 1)
        return true;
    else
        return false;
}

bool check_recv_valid(void)
{
    rx_head = e1000_read_reg(e1000, E1000_RDH);
    // uint32_t read_tail = e1000_read_reg(e1000, E1000_RDT);

    if (rx_desc_array[rx_tail].status == 1) //((rx_tail + 1) % RXDESCS) != rx_head)
    {
        printl("release recv!\n");
        return true;
    }
    else
        return false;
}
void e1000_handle_txqe()
{
    do_unblock(&send_queue);
}
void e1000_handle_rxdmt0()
{
    do_unblock(&recv_queue);
}
void e1000_echo(void *buffer, int length)
{
    char *curr = (char *)buffer;
    if (length != 0)
    {
        printk("hahh\n");
        for (int i = 0; i < 6; i++)
        {
            *(curr + i) = 0xff;
        }
        *(curr + 54 + 2) = 's';
        *(curr + 54 + 3) = 'p';
        *(curr + 54 + 4) = 'o';
        *(curr + 54 + 5) = 'n';
        *(curr + 54 + 6) = 's';
        *(curr + 54 + 7) = 'e';
        e1000_transmit(buffer, length);
    }
    else
    {
        return;
    }
}