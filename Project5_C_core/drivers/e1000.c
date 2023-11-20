#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/sched.h>

#include <os/mm.h>   //需要用到其中的get_page_addr函数得到虚地址对应的物理地址

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

//head and tail
//TDH和TDT寄存器存储的是描述符元素的索引
//因为MAX有了描述符基地址，可以直接推算出指针指向的描述符内存地址
uint32_t tx_head = 0;
uint32_t tx_tail = 0;

uint32_t rx_head = 0;
uint32_t rx_tail = RXDESCS - 1;

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);      //全局接收控制寄存器，其中EN位控制接收数据帧的全局使能
    e1000_write_reg(e1000, E1000_TCTL, 0);      //全局发送控制寄存器，其中EN位控制发送数据帧的全局使能

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);       //发送Head指针
    e1000_write_reg(e1000, E1000_TDT, 0);       //发送Tail指针

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);       //接收Head指针
    e1000_write_reg(e1000, E1000_RDT, 0);       //接收Tail指针

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    // IMS寄存器用于软件使能各个e1000网卡中断；对应掩码位设为1表示启用该中断；为0表示禁用中断
    // IMC寄存器用于软件禁用各个e1000网卡中断；对应掩码位设为1表示禁用该中断；为0表示不禁用中断
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);  

    /* Clear any pending interrupt events. */
    // ICR寄存器记录各个网卡中断触发的情况，当某个网卡中断条件满足时，该中断在ICR寄存器中对应
    // 比特将会被硬件自动置1。直到ICR
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

// init the tx_desc_array
void init_all_tx_desc(void)
{
    for(int i=0; i<TXDESCS; i++)
    {
        tx_desc_array[i].addr = 0;
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso = 0;
        tx_desc_array[i].cmd = 0;
        tx_desc_array[i].status = 1;
        tx_desc_array[i].css = 0;
    }
}

// init the rx_desc_array
void init_all_rx_desc(void)
{
    for(int i=0; i<RXDESCS; i++)
    {
        rx_desc_array[i].addr = kva2pa(rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }
}

void init_single_tx_desc(int index)
{
    tx_desc_array[index].addr = 0;
    tx_desc_array[index].length = 0;
    tx_desc_array[index].cso = 0;
    tx_desc_array[index].cmd = 0;
    tx_desc_array[index].status = 0;
    tx_desc_array[index].css = 0;
}

void init_single_rx_desc(int index)
{
    rx_desc_array[index].length = 0;
    rx_desc_array[index].csum = 0;
    rx_desc_array[index].status = 0;
    rx_desc_array[index].errors = 0;
    rx_desc_array[index].special = 0;
}

void update_head(int tx_or_rx)
{
    if(tx_or_rx == TX)
        tx_head = (tx_head + 1) % TXDESCS;
    else
        rx_head = (rx_head + 1) % TXDESCS;
}

void update_tail(int tx_or_rx)
{
    if(tx_or_rx == TX)
        tx_tail = (tx_tail + 1) % TXDESCS;
    else
        rx_tail = (rx_tail + 1) % TXDESCS;
}

//软件填充一个描述符，再交给硬件去发送
void set_single_tx_desc(int index, ptr_t paddr, uint16_t length)
{
    tx_desc_array[index].addr = paddr;
    tx_desc_array[index].length = length;

    /*CMD字段共8位，存储一些指示DMA控制器的命令
    IDE | VLE | DEXT | RSV | RS | IC | IFCS | EOP
    需要设置：
    1：DEXT位置0使得DMA控制器按Legacy布局解析描述符内容
    2：RS位置1，使得DMA控制器能够记录传输的状态于描述符的STA字段
    3：EOP位置1，因为本实验一个数据包对运营一个数据帧
    综合考虑应设为 0b00001001*/

    tx_desc_array[index].cmd = 0b00001001;
    //在设了RS位的基础上，后续硬件发送成功后会将status位置1
    tx_desc_array[index].status = 0;
}

//硬件已经接收一个描述符，软件对其进行处理。
int deal_single_rx_desc(uint8_t *rxbuffer)
{
    int length = rx_desc_array[rx_tail].length;
    memcpy((uint8_t *)rxbuffer, (uint8_t *)rx_pkt_buffer[rx_tail], length);
    return length;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    init_all_tx_desc();

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    // TDBAL/TDBAH分别存储循环数组基地址的低/高32位，TDLEN存储循环队列所占字节数。
    // hint：这里的基地址为物理地址
    uint32_t TDBAL_VAL = (uint32_t)(kva2pa(tx_desc_array));
    e1000_write_reg(e1000, E1000_TDBAL, TDBAL_VAL);
    uint32_t TDBAH_VAL = (uint32_t)(kva2pa(tx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_TDBAH, TDBAH_VAL);

    uint32_t size = (uint32_t)(TXDESCS * sizeof(struct e1000_tx_desc));
    e1000_write_reg(e1000, E1000_TDLEN, size);

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, tx_head);
    e1000_write_reg(e1000, E1000_TDT, tx_tail);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    //初始化TCTL寄存器，设置TCTL.EN=1,TCTL.PSP=1,TCTL.CT=0X10H,TCTL.COLD=0x40H
    uint32_t TCTL_VAL = (uint32_t)0x0004010a;
    e1000_write_reg(e1000, E1000_TCTL, TCTL_VAL);

    /*
    #define E1000_TCTL_RST	  0x00000001	
    #define E1000_TCTL_EN	  0x00000002	
    #define E1000_TCTL_BCE	  0x00000004	
    #define E1000_TCTL_PSP	  0x00000008	
    #define E1000_TCTL_CT	  0x00000ff0	
    #define E1000_TCTL_COLD   0x003ff000	
    #define E1000_TCTL_SWXOFF 0x00400000	
    #define E1000_TCTL_PBE	  0x00800000	
    #define E1000_TCTL_RTLC   0x01000000	
    #define E1000_TCTL_NRTU   0x02000000	
    #define E1000_TCTL_MULR   0x10000000
    */  
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    //low和high分别对应offset为0和1
    //hint：对high要置valid位，因此最高位为1
    //MAX: {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};
    uint32_t MAC_low = 0x00350a00;
    e1000_write_reg_array(e1000, E1000_RA, 0, MAC_low);
    uint32_t MAC_high = 0x8000531e;
    e1000_write_reg_array(e1000, E1000_RA, 1, MAC_high);

    /* TODO: [p5-task2] Initialize rx descriptors */
    init_all_rx_desc();

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uint32_t RDBAL_VAL = (uint32_t)(kva2pa(rx_desc_array));
    e1000_write_reg(e1000, E1000_RDBAL, RDBAL_VAL);
    uint32_t RDBAH_VAL = (uint32_t)(kva2pa(rx_desc_array) >> 32);
    e1000_write_reg(e1000, E1000_RDBAH, RDBAH_VAL);

    uint32_t size = (uint32_t)(RXDESCS * sizeof(struct e1000_rx_desc));
    e1000_write_reg(e1000, E1000_RDLEN, size);

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, rx_head);
    e1000_write_reg(e1000, E1000_RDT, rx_tail);

    /* TODO: [p5-task2] Program the Receive Control Register */
    //需要设置RCTL.EN=1,RCTL.BAM=1,RCTL.BSEX=0,RCTL.BSIZE=0
    uint32_t RCTL_VAL = 0x00008002;
    e1000_write_reg(e1000, E1000_RCTL, RCTL_VAL);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);
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
    local_flush_dcache();

    tx_head = e1000_read_reg(e1000, E1000_TDH);
 
    //step1-1:读取当前tail
    tx_tail = e1000_read_reg(e1000, E1000_TDT);

    //task3: 中断解决发送描述符不够用的情况
    if(tx_desc_array[tx_tail].status == 0)
    {
        //task4:开启中断txqe
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        //enable_ext();

        current_running->status = TASK_BLOCKED;
        Enqueue(&send_queue, &current_running->list);
        do_scheduler();

        e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE); 
    }

    //step1-2:由软件设置发送描述符
    ptr_t offset = (ptr_t)txpacket & 0xfff;
    ptr_t ppn = get_page_addr(txpacket) | offset;
    set_single_tx_desc(tx_tail, ppn, length);
    //step1-3:更新尾指针
    update_tail(TX);
    //step1-4:将新尾指针写入TDT
    e1000_write_reg(e1000, E1000_TDT, tx_tail);

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
    local_flush_dcache();
    
    //hint:rx_tail初始化时为最后一个即 RXDESCS - 1
    //也即它一开始就落后head一个位置

    //step1:读取当前head和tail
    rx_head = e1000_read_reg(e1000, E1000_RDH);
    rx_tail = e1000_read_reg(e1000, E1000_RDT);
    
    //step2:更新tail
    /*原本应当先由软件处理了描述符再更新tail，但由于tail本身
    落后一个位置，所以可以认为每回都先更新再处理。
    当然这里要考虑硬件填充失败head没有前移的情况*/
    if( ((rx_tail + 1) % RXDESCS) == rx_head )
    {
        enable_ext();
        printl("no enough desc, blocked! head = %d. tail = %d\n", tx_head, tx_tail);
        current_running->status = TASK_BLOCKED;
        Enqueue(&recv_queue, &current_running->list);
        do_scheduler();     
    }  
    else
        update_tail(RX);

    //step3:处理描述符
    int length = deal_single_rx_desc((uint8_t *)rxbuffer);

    //step4:清空该描述符
    init_single_rx_desc(rx_tail);

    //step5:将新的tail写入寄存器
    e1000_write_reg(e1000, E1000_RDT, rx_tail);

    local_flush_dcache();

    return length;
}

bool check_send_valid(void)
{
    //uint32_t read_tail = e1000_read_reg(e1000, E1000_TDT);

    if(tx_desc_array[tx_tail].status == 1)
        return true;
    else
        return false;
}

bool check_recv_valid(void)
{
    rx_head = e1000_read_reg(e1000, E1000_RDH);
    //uint32_t read_tail = e1000_read_reg(e1000, E1000_RDT);

    if( ((rx_tail + 1) % RXDESCS) != rx_head )
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