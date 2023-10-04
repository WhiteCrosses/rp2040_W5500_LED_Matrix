#include <stdio.h>
#include <stdlib.h>
#include "port_common.h"

#include "wizchip_conf.h"
#include "w5x00_spi.h"
#include "socket.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (2 * 3072)

#define DATA_BUF_SIZE	(2 * 3072)

/* Socket */
#define SOCKET_LOOPBACK 0

/* Port */
#define PORT_LOOPBACK 5000

#define PA_OK 0x11
#define PA_NOK 0x00

#define IS_RGBW false
#define WS2812_PIN 2

#define FRAME_LENGTH_OFFSET 10
#define FRAME_NUMBER_OFFSET 12
#define CHUNK_OFFSET 16
#define CHUNK_LENGTH_OFFSET 18
#define PAYLOAD_OFFSET 20




static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}



/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 11, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};

uint16_t payload_len;
uint16_t payload_offset;
uint16_t chunks_loaded = 0;
uint16_t full_frame_chunk_number;
uint16_t frame_length;

/* Loopback */
static uint8_t g_receive_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

static uint8_t g_transmit_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

static uint8_t check_for_preamble(uint8_t *buf);

static void add_to_transmit_buf(uint8_t *buf, uint16_t size, uint16_t offset);

static int32_t receive_data(uint8_t sn, uint8_t* buf, uint16_t port);
/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    int retval = 0;

    set_clock_khz();

    stdio_init_all();

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    network_initialize(g_net_info);
    /* Get network information */
    print_network_information(g_net_info);

    /* Infinite loop */
    while (1)
    {
        receive_data(SOCKET_LOOPBACK, g_receive_buf, PORT_LOOPBACK);
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

int32_t receive_data(uint8_t sn, uint8_t* buf, uint16_t port)
{
   int32_t ret;
   uint16_t size = 0;

    //Druciarstwo jak chuj
    //Nic nie rozumiem :(((
   switch(getSn_SR(sn))
   {
        case SOCK_ESTABLISHED :
            if(getSn_IR(sn) & Sn_IR_CON){
			    setSn_IR(sn,Sn_IR_CON);
            }

		    if((size = getSn_RX_RSR(sn)) > 0){ //Check if received any data
                if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
                ret = recv(sn, buf, size); //load data to buffer
                if(ret <= 0) return ret; // If the received data length <= 0, receive failed and process end

                /*
                Processing header
                */

                if(check_for_preamble(buf) == PA_OK){
                    printf("Preamble found!\n");

                    // Get the length of the payload
                    payload_len = (buf[CHUNK_LENGTH_OFFSET] << 8) | buf[CHUNK_LENGTH_OFFSET+1];
                    printf("Length of the payload: %d\n", payload_len);

                    // Get the offset of the payload
                    payload_offset = (buf[CHUNK_OFFSET] << 8) | buf[CHUNK_OFFSET+1];
                    printf("Offset of the payload: %d\n", payload_offset);

                    // Get the length of the frame
                    frame_length = (buf[FRAME_LENGTH_OFFSET] << 8) | buf[FRAME_LENGTH_OFFSET+1];
                    printf("Frame length: %d\n", frame_length);

                    // Get the number of the frame
                    full_frame_chunk_number = (buf[FRAME_NUMBER_OFFSET] << 8) | buf[FRAME_NUMBER_OFFSET+1];
                    printf("Frame number: %d\n", full_frame_chunk_number);

                    add_to_transmit_buf(buf, payload_len, payload_offset);
                    
                    chunks_loaded += 1;
                    full_frame_chunk_number = chunks_loaded;
                    if(chunks_loaded == full_frame_chunk_number){
                        chunks_loaded = 0;

                        for(int i=0; i<frame_length;i+=3){
                            printf("%d", frame_length);
                            printf("LED output\n");
                            put_pixel(urgb_u32(g_transmit_buf[i], g_transmit_buf[i+1], g_transmit_buf[i+2]));
                        }
    
                    }
                }
            }
            break;

        case SOCK_CLOSE_WAIT :
            if((ret = disconnect(sn)) != SOCK_OK) return ret;
            break;

        case SOCK_INIT :
            if( (ret = listen(sn)) != SOCK_OK) return ret;
            break;

        case SOCK_CLOSED:
            if((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) return ret;
            break;

        default:
            break;
   }
   return 1;
}

void add_to_transmit_buf(uint8_t* buf, uint16_t size, uint16_t offset)
{
    for(int i=0; i<size; i+=1){
        g_transmit_buf[offset+i] = buf[i];
    }
}


//Validate if data in buffer is a preamble
uint8_t check_for_preamble(uint8_t* buf){
    if(buf[0] == 0xAA && buf[1] == 0x55 && buf[2] == 0xAA && buf[3] == 0x55){
            return PA_OK;
    }
    else{
            return PA_NOK;
    }
}

