// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(ESP32)

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/io_mux_reg.h"
#include "rom/lldesc.h"
//#include "esp_heap_caps.h"
#include "esp32_i2s_parallel.h"

typedef struct {
    volatile lldesc_t *dmadesc_a, *dmadesc_b;
    int desccount_a, desccount_b;
} i2s_parallel_state_t;

static i2s_parallel_state_t *i2s_state[2]={NULL, NULL};

callback shiftCompleteCallback;

void setShiftCompleteCallback(callback f) {
    shiftCompleteCallback = f;
}

volatile bool previousBufferFree = true;

static int i2snum(i2s_dev_t *dev) {
    return (dev==&I2S0)?0:1;
}

// Todo: handle IS20? (this is hard coded for I2S1 only)
static void IRAM_ATTR i2s_isr(void* arg) {
    REG_WRITE(I2S_INT_CLR_REG(1), (REG_READ(I2S_INT_RAW_REG(1)) & 0xffffffc0) | 0x3f);

    // at this point, the previously active buffer is free, go ahead and write to it
    previousBufferFree = true;

    if(shiftCompleteCallback)
        shiftCompleteCallback();
}
/*
//Calculate the amount of dma descs needed for a buffer desc
static int calc_needed_dma_descs_for(i2s_parallel_buffer_desc_t *desc) {
    int ret=0;
    for (int i=0; desc[i].memory!=NULL; i++) {
        ret+=(desc[i].size+DMA_MAX-1)/DMA_MAX;
    }
    return ret;
}

static void fill_dma_desc(volatile lldesc_t *dmadesc, i2s_parallel_buffer_desc_t *bufdesc) {
    int n=0;
    for (int i=0; bufdesc[i].memory!=NULL; i++) {
        int len=bufdesc[i].size;
        uint8_t *data=(uint8_t*)bufdesc[i].memory;
        while(len) {
            int dmalen=len;
            if (dmalen>DMA_MAX) dmalen=DMA_MAX;
            dmadesc[n].size=dmalen;
            dmadesc[n].length=dmalen;
            dmadesc[n].buf=data;
            dmadesc[n].eof=0;
            dmadesc[n].sosf=0;
            dmadesc[n].owner=1;
            dmadesc[n].qe.stqe_next=(lldesc_t*)&dmadesc[n+1];
            dmadesc[n].offset=0;
            len-=dmalen;
            data+=dmalen;
            n++;
        }
    }

    // set EOF bit in last dma descriptor
    dmadesc[n-1].eof=1;
    // link end of list back to beginning so current frame will be refreshed continously
    dmadesc[n-1].qe.stqe_next=(lldesc_t*)&dmadesc[0];

    printf("fill_dma_desc: filled %d descriptors\n", n);
}
*/

// size must be less than DMA_MAX - need to handle breaking long transfer into two descriptors before call
// DMA_MAX by the way is the maximum data packet size you can hold in one chunk
void link_dma_desc(volatile lldesc_t *dmadesc, volatile lldesc_t *prevdmadesc, void *memory, size_t size) 
{
    if(size > DMA_MAX) size = DMA_MAX;

    dmadesc->size = size;
    dmadesc->length = size;
    dmadesc->buf = memory;
    dmadesc->eof = 0;
    dmadesc->sosf = 0;
    dmadesc->owner = 1;
    dmadesc->qe.stqe_next = 0;  // will need to set this elsewhere
    dmadesc->offset = 0;

    // link previous to current
    if(prevdmadesc)
        prevdmadesc->qe.stqe_next = (lldesc_t*)dmadesc;
}

static void gpio_setup_out(int gpio, int sig) {
    if (gpio==-1) return;
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    gpio_set_direction(gpio, GPIO_MODE_DEF_OUTPUT);
    gpio_matrix_out(gpio, sig, false, false);
}

static void dma_reset(i2s_dev_t *dev) {
    dev->lc_conf.in_rst=1; dev->lc_conf.in_rst=0;
    dev->lc_conf.out_rst=1; dev->lc_conf.out_rst=0;
}

static void fifo_reset(i2s_dev_t *dev) {
    dev->conf.rx_fifo_reset=1; dev->conf.rx_fifo_reset=0;
    dev->conf.tx_fifo_reset=1; dev->conf.tx_fifo_reset=0;
}

void i2s_parallel_setup_without_malloc(i2s_dev_t *dev, const i2s_parallel_config_t *cfg) {
    //Figure out which signal numbers to use for routing
    //printf("Setting up parallel I2S bus at I2S%d\n", i2snum(dev));
    int sig_data_base, sig_clk;
    if (dev==&I2S0) {
        sig_data_base=I2S0O_DATA_OUT0_IDX;
        sig_clk=I2S0O_WS_OUT_IDX;
    } else {
        //printf("Setting up i2s parallel mode in %d bit mode!\n", cfg->bits);
        if (cfg->bits==I2S_PARALLEL_BITS_32) {
            sig_data_base=I2S1O_DATA_OUT0_IDX;
        } else if (cfg->bits==I2S_PARALLEL_BITS_16) {
            //Because of... reasons... the 16-bit values for i2s1 appear on d8...d23
            sig_data_base=I2S1O_DATA_OUT8_IDX;
        } else { // I2S_PARALLEL_BITS_8
            //printf("Setting up i2s parallel mode in %d bit mode -> https://www.esp32.com/viewtopic.php?f=17&t=3188 | https://www.esp32.com/viewtopic.php?f=13&t=3256", 8);
            sig_data_base=I2S1O_DATA_OUT0_IDX;
        }
        sig_clk=I2S1O_WS_OUT_IDX;
    }
    
    //Route the signals
    for (int x=0; x<cfg->bits; x++) {
        gpio_setup_out(cfg->gpio_bus[x], sig_data_base+x);
    }
    //ToDo: Clk/WS may need inversion?
    gpio_setup_out(cfg->gpio_clk, sig_clk);
    
    //Power on dev
    if (dev==&I2S0) {
        periph_module_enable(PERIPH_I2S0_MODULE);
    } else {
        periph_module_enable(PERIPH_I2S1_MODULE);
    }
    //Initialize I2S dev
    dev->conf.rx_reset=1; dev->conf.rx_reset=0;
    dev->conf.tx_reset=1; dev->conf.tx_reset=0;
    dma_reset(dev);
    fifo_reset(dev);
    
    //Enable LCD mode
    dev->conf2.val=0;
    dev->conf2.lcd_en=1;
        
    // Enable "One datum will be written twice in LCD mode" - for some reason, if we don't do this in 8-bit mode, data is updated on half-clocks not clocks
    if(cfg->bits == I2S_PARALLEL_BITS_8)
        dev->conf2.lcd_tx_wrx2_en=1;

    dev->sample_rate_conf.val=0;
    dev->sample_rate_conf.rx_bits_mod=cfg->bits;
    dev->sample_rate_conf.tx_bits_mod=cfg->bits;
    dev->sample_rate_conf.rx_bck_div_num=4; //ToDo: Unsure about what this does...

    // because conf2.lcd_tx_wrx2_en is set for 8-bit mode, the clock speed is doubled, drop it in half here
    if(cfg->bits == I2S_PARALLEL_BITS_8)
        dev->sample_rate_conf.tx_bck_div_num=2;
    else
        dev->sample_rate_conf.tx_bck_div_num=1; // datasheet says this must be 2 or greater (but 1 seems to work)
    
    dev->clkm_conf.val=0;
    dev->clkm_conf.clka_en=0;
    dev->clkm_conf.clkm_div_a=63;
    dev->clkm_conf.clkm_div_b=63;
    //We ignore the possibility for fractional division here, clkspeed_hz must round up for a fractional clock speed, must result in >= 2
    dev->clkm_conf.clkm_div_num=80000000L/(cfg->clkspeed_hz + 1);

    dev->fifo_conf.val=0;
    dev->fifo_conf.rx_fifo_mod_force_en=1;
    dev->fifo_conf.tx_fifo_mod_force_en=1;
    //dev->fifo_conf.tx_fifo_mod=1;
    dev->fifo_conf.tx_fifo_mod=1;
    dev->fifo_conf.rx_data_num=32; //Thresholds. 
    dev->fifo_conf.tx_data_num=32;
    dev->fifo_conf.dscr_en=1;
    
    dev->conf1.val=0;
    dev->conf1.tx_stop_en=0;
    dev->conf1.tx_pcm_bypass=1;
    
    dev->conf_chan.val=0;
    dev->conf_chan.tx_chan_mod=1;
    dev->conf_chan.rx_chan_mod=1;
    
    //Invert ws to be active-low... ToDo: make this configurable
    //dev->conf.tx_right_first=1;
    dev->conf.tx_right_first=0;
    //dev->conf.rx_right_first=1;
    dev->conf.rx_right_first=0;
    
    dev->timing.val=0;
    
    //Allocate DMA descriptors
    i2s_state[i2snum(dev)]=malloc(sizeof(i2s_parallel_state_t));
    assert(i2s_state[i2snum(dev)] != NULL);
    i2s_parallel_state_t *st=i2s_state[i2snum(dev)];

    st->desccount_a = cfg->desccount_a;
    st->desccount_b = cfg->desccount_b;
    st->dmadesc_a = cfg->lldesc_a;
    st->dmadesc_b = cfg->lldesc_b;

    //Reset FIFO/DMA -> needed? Doesn't dma_reset/fifo_reset do this?
    dev->lc_conf.in_rst=1; dev->lc_conf.out_rst=1; dev->lc_conf.ahbm_rst=1; dev->lc_conf.ahbm_fifo_rst=1;
    dev->lc_conf.in_rst=0; dev->lc_conf.out_rst=0; dev->lc_conf.ahbm_rst=0; dev->lc_conf.ahbm_fifo_rst=0;
    dev->conf.tx_reset=1; dev->conf.tx_fifo_reset=1; dev->conf.rx_fifo_reset=1;
    dev->conf.tx_reset=0; dev->conf.tx_fifo_reset=0; dev->conf.rx_fifo_reset=0;
    
    // setup I2S Interrupt
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(1), I2S_OUT_EOF_INT_ENA_V, 1, I2S_OUT_EOF_INT_ENA_S);
    // allocate a level 1 intterupt: lowest priority, as ISR isn't urgent and may take a long time to complete
    esp_intr_alloc(ETS_I2S1_INTR_SOURCE, (int)(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1), i2s_isr, NULL, NULL);

    //Start dma on front buffer (buffer a)
    dev->lc_conf.val=I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
    dev->out_link.addr=((uint32_t)(&st->dmadesc_a[0]));
    dev->out_link.start=1;
    dev->conf.tx_start=1;
}

//Flip to a buffer: 0 for bufa, 1 for bufb
void i2s_parallel_flip_to_buffer(i2s_dev_t *dev, int bufid) {
    int no=i2snum(dev);
    if (i2s_state[no]==NULL) return;
    lldesc_t *active_dma_chain;
    if (bufid==0) {
        active_dma_chain=(lldesc_t*)&i2s_state[no]->dmadesc_a[0];
    } else {
        active_dma_chain=(lldesc_t*)&i2s_state[no]->dmadesc_b[0];
    }

    // setup linked list to refresh from new buffer (continuously) when the end of the current list has been reached
    i2s_state[no]->dmadesc_a[i2s_state[no]->desccount_a-1].qe.stqe_next=active_dma_chain;
    i2s_state[no]->dmadesc_b[i2s_state[no]->desccount_b-1].qe.stqe_next=active_dma_chain;

    // we're still refreshing the previously buffer, so it shouldn't be written to yet
    previousBufferFree = false;
}

bool i2s_parallel_is_previous_buffer_free() {
    return previousBufferFree;
}


#endif
