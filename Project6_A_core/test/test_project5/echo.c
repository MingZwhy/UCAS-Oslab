#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define MAX_RECV_CNT 1
#define RX_PKT_SIZE 200
#define MEM_SIZE 1024
#define PAGE_SIZE 4096
static uint32_t recv_buffer[MAX_RECV_CNT * RX_PKT_SIZE];
static int recv_length[MAX_RECV_CNT];

int main(void)
{
    int print_location = 1; 
    int iteration = 1;

    uint32_t va_recv = sys_shmpageget(13);
    printf("va recv is %x\n",va_recv);
    memset((void *)va_recv, 0, PAGE_SIZE);
    uint32_t va_send = sys_shmpageget(13);
    uint32_t va_begin  = va_send; 
    uint32_t va_final  = va_send + MEM_SIZE;
    printf("va send is %x\n",va_send);
    while (1)
    {
        sys_move_cursor(0, print_location);
        printf("[RECV] start recv(%d): ", MAX_RECV_CNT);
        
        int ret = sys_net_recv(va_recv, MAX_RECV_CNT, recv_length);
        printf("%d, iteration = %d\n", ret, iteration++);

        char *curr = (char *)va_recv;
        char *curr_send = (char *)va_send;
        for (int i = 0; i < MAX_RECV_CNT; ++i)
        {
            sys_move_cursor(0, print_location + 1);
            if (recv_length[i] != 0)
            {
                for (int i = 0; i < 6; i++)
                {
                    *(curr_send + i) = 0xff;
                }
                *(curr_send + 54 + 2) = 's';
                *(curr_send + 54 + 3) = 'p';
                *(curr_send + 54 + 4) = 'o';
                *(curr_send + 54 + 5) = 'n';
                *(curr_send + 54 + 6) = 's';
                *(curr_send + 54 + 7) = 'e';
                sys_net_send(curr_send, recv_length[i]);
            }
            printf("packet %d:\n", i);
            for (int j = 0; j < (recv_length[i] + 15) / 16; ++j)
            {
                for (int k = 0; k < 16 && (j * 16 + k < recv_length[i]); ++k)
                {
                    printf("%02x ", (uint32_t)(*(uint8_t *)curr));
                    ++curr;
                    ++curr_send;
                }
                printf("\n");
            }
            if(curr_send >= va_final || curr >= va_final)
            {
                curr_send = va_begin;
                curr      = va_begin;
                printf("updata address!\n");
            }
        }
    }
    return 0;
}