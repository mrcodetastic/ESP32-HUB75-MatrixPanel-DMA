/*
 * ESP32_I2S_PARALLEL_DMA (Version 3)
 * 
 * Author:      Mrfaptastic @ https://github.com/mrfaptastic/
 * 
 * Description: Multi-ESP32 product DMA setup functions for WROOM & S2, S3 mcu's.
 *
 * Credits:
 *  1) https://www.esp32.com/viewtopic.php?f=17&t=3188          for original ref. implementation 
 *  2) https://github.com/TobleMiner/esp_i2s_parallel           for a cleaner implementation
 *
 */

// Header
#include "esp32_i2s_parallel_dma.h" 
#include "esp32_i2s_parallel_mcu_def.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/periph_ctrl.h>
#include <soc/gpio_sig_map.h>

// For I2S state management.
static i2s_parallel_state_t *i2s_state  = NULL;

// ESP32-S2,S3,C3 only has IS20
// Original ESP32 has two I2S's, but we'll stick with the lowest common denominator.

#ifdef ESP32_ORIG
static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};
#else
static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0};	
#endif

callback shiftCompleteCallback;
void setShiftCompleteCallback(callback f) {
    shiftCompleteCallback = f;
}

volatile int  previousBufferOutputLoopCount = 0;
volatile bool previousBufferFree      = true;

static void IRAM_ATTR irq_hndlr(void* arg) { // if we use I2S1 (default)

//i2s_port_t port = *((i2s_port_t*) arg);

/* Saves a few cycles, no need to cast void ptr to i2s_port_t and then check 120 times second... */
        SET_PERI_REG_BITS(I2S_INT_CLR_REG(ESP32_I2S_DEVICE), I2S_OUT_EOF_INT_CLR_V, 1, I2S_OUT_EOF_INT_CLR_S);

	previousBufferFree 		= true;

/*	
    if(shiftCompleteCallback) { // we've defined a callback function ?
        shiftCompleteCallback();
 }
*/
        
} // end irq_hndlr


// For peripheral setup and configuration
static inline int get_bus_width(i2s_parallel_cfg_bits_t width) {
  switch(width) {
    case I2S_PARALLEL_WIDTH_8:
      return 8;
    case I2S_PARALLEL_WIDTH_16:
      return 16;
    case I2S_PARALLEL_WIDTH_24:
      return 24;
    default:
      return -ESP_ERR_INVALID_ARG;
  }
}

static void iomux_set_signal(int gpio, int signal) {
  if(gpio < 0) {
    return;
  }
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, GPIO_MODE_DEF_OUTPUT);
  gpio_matrix_out(gpio, signal, false, false);
  
  // More mA the better...
  gpio_set_drive_capability((gpio_num_t)gpio, (gpio_drive_cap_t)3);
  
}

static void dma_reset(i2s_dev_t* dev) {
  dev->lc_conf.in_rst = 1;
  dev->lc_conf.in_rst = 0;
  dev->lc_conf.out_rst = 1;
  dev->lc_conf.out_rst = 0;
  
  dev->lc_conf.ahbm_rst = 1;
  dev->lc_conf.ahbm_rst = 0;
    
  
}

static void fifo_reset(i2s_dev_t* dev) {
  dev->conf.rx_fifo_reset = 1;
  
#ifdef ESP32_SXXX
  while(dev->conf.rx_fifo_reset_st); // esp32-s2 only
#endif
  dev->conf.rx_fifo_reset = 0;
  
  dev->conf.tx_fifo_reset = 1;
#ifdef ESP32_SXXX
  while(dev->conf.tx_fifo_reset_st); // esp32-s2 only
#endif

  dev->conf.tx_fifo_reset = 0;
}

static void dev_reset(i2s_dev_t* dev) {
  fifo_reset(dev);
  dma_reset(dev);
  dev->conf.rx_reset=1;
  dev->conf.tx_reset=1;
  dev->conf.rx_reset=0;
  dev->conf.tx_reset=0;  
}

// DMA Linked List
// Size must be less than DMA_MAX - need to handle breaking long transfer into two descriptors before call
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



esp_err_t i2s_parallel_driver_install(i2s_port_t port, i2s_parallel_config_t* conf) {
  
  //port = I2S_NUM_0; /// override.
  
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  if(conf->sample_width < I2S_PARALLEL_WIDTH_8 || conf->sample_width >= I2S_PARALLEL_WIDTH_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  if(conf->sample_rate > I2S_PARALLEL_CLOCK_HZ || conf->sample_rate < 1) {
    return ESP_ERR_INVALID_ARG;
  }
  uint32_t clk_div_main = I2S_PARALLEL_CLOCK_HZ / conf->sample_rate / i2s_parallel_get_memory_width(port, conf->sample_width);
  if(clk_div_main < 2 || clk_div_main > 0xFF) {
    return ESP_ERR_INVALID_ARG;
  }

  volatile int iomux_signal_base;
  volatile int iomux_clock;
  int irq_source;
  
  // Initialize I2S0 peripheral
  if (port == I2S_NUM_0) {
      periph_module_reset(PERIPH_I2S0_MODULE);
      periph_module_enable(PERIPH_I2S0_MODULE);
      iomux_clock = I2S0O_WS_OUT_IDX;
      irq_source = ETS_I2S0_INTR_SOURCE;

      switch(conf->sample_width) {
        case I2S_PARALLEL_WIDTH_8:
        case I2S_PARALLEL_WIDTH_16:
          iomux_signal_base = I2S0O_DATA_OUT8_IDX;
          break;
        case I2S_PARALLEL_WIDTH_24:
          iomux_signal_base = I2S0O_DATA_OUT0_IDX;
          break;
        case I2S_PARALLEL_WIDTH_MAX:
          return ESP_ERR_INVALID_ARG;
      }
  } 
#ifdef ESP32_ORIG    
  // Can't compile if I2S1 if it doesn't exist with that hardware's IDF.... 
  else {
//	  I2S = &I2S1;
	  
      periph_module_reset(PERIPH_I2S1_MODULE);
      periph_module_enable(PERIPH_I2S1_MODULE);
      iomux_clock = I2S1O_WS_OUT_IDX;
      irq_source = ETS_I2S1_INTR_SOURCE;

      switch(conf->sample_width) {
        case I2S_PARALLEL_WIDTH_16:
          iomux_signal_base = I2S1O_DATA_OUT8_IDX;
          break;
        case I2S_PARALLEL_WIDTH_8:
        case I2S_PARALLEL_WIDTH_24:
          iomux_signal_base = I2S1O_DATA_OUT0_IDX;
          break;
        case I2S_PARALLEL_WIDTH_MAX:
          return ESP_ERR_INVALID_ARG;
      }
  }
#endif 

  // Setup GPIOs
  int bus_width = get_bus_width(conf->sample_width);

  // Setup I2S peripheral
  i2s_dev_t* dev =  I2S[port];
  //dev_reset(dev);    
  
  
  // Setup GPIO's
  for(int i = 0; i < bus_width; i++) {
    iomux_set_signal(conf->gpio_bus[i], iomux_signal_base + i);
  }
  iomux_set_signal(conf->gpio_clk, iomux_clock);
  
  // invert clock phase if required
  if (conf->clkphase)
    GPIO.func_out_sel_cfg[conf->gpio_clk].inv_sel = 1;

  // Setup i2s clock
  dev->sample_rate_conf.val = 0;
  
  // Third stage config, width of data to be written to IO (I think this should always be the actual data width?)
  dev->sample_rate_conf.rx_bits_mod = bus_width;
  dev->sample_rate_conf.tx_bits_mod = bus_width;
  
  dev->sample_rate_conf.rx_bck_div_num = 2;
  dev->sample_rate_conf.tx_bck_div_num = 2;
  
  // Clock configuration
  dev->clkm_conf.val=0;             // Clear the clkm_conf struct  
  
#ifdef ESP32_SXXX
  dev->clkm_conf.clk_sel = 2; // esp32-s2 only  
  dev->clkm_conf.clk_en  = 1;
#endif

#ifdef ESP32_ORIG
  dev->clkm_conf.clka_en=0;         // Use the 160mhz system clock (PLL_D2_CLK) when '0'
#endif
  
  dev->clkm_conf.clkm_div_b=0;      // Clock numerator
  dev->clkm_conf.clkm_div_a=1;      // Clock denominator  
  

  // Note: clkm_div_num must only be set here AFTER clkm_div_b, clkm_div_a, etc. Or weird things happen!
  // On original ESP32, max I2S DMA parallel speed is 20Mhz.  
  dev->clkm_conf.clkm_div_num = clk_div_main;
    

  // I2S conf2 reg
  dev->conf2.val = 0;
  dev->conf2.lcd_en = 1;
  dev->conf2.lcd_tx_wrx2_en=0; 
  dev->conf2.lcd_tx_sdx2_en=0;    

  // I2S conf reg
  dev->conf.val = 0;   
  
#ifdef ESP32_SXXX  
  dev->conf.tx_dma_equal=1;  // esp32-s2 only
  dev->conf.pre_req_en=1;    // esp32-s2 only - enable I2S to prepare data earlier? wtf?
#endif

  // Now start setting up DMA FIFO
  dev->fifo_conf.val = 0;  
  dev->fifo_conf.rx_data_num = 32; // Thresholds. 
  dev->fifo_conf.tx_data_num = 32;  
  dev->fifo_conf.dscr_en     = 1;  

#ifdef ESP32_ORIG

  // Enable "One datum will be written twice in LCD mode" - for some reason, 
  // if we don't do this in 8-bit mode, data is updated on half-clocks not clocks
  if(conf->sample_width == I2S_PARALLEL_WIDTH_8)
    dev->conf2.lcd_tx_wrx2_en=1;  

  // Not really described for non-pcm modes, although datasheet states it should be set correctly even for LCD mode
  // First stage config. Configures how data is loaded into fifo
  if(conf->sample_width == I2S_PARALLEL_WIDTH_24) {
    // Mode 0, single 32-bit channel, linear 32 bit load to fifo
    dev->fifo_conf.tx_fifo_mod = 3;
  } else {
    // Mode 1, single 16-bit channel, load 16 bit sample(*) into fifo and pad to 32 bit with zeros
    // *Actually a 32 bit read where two samples are read at once. Length of fifo must thus still be word-aligned
    dev->fifo_conf.tx_fifo_mod = 1;
  }
  
  // Dictated by ESP32 datasheet
  dev->fifo_conf.rx_fifo_mod_force_en = 1;
  dev->fifo_conf.tx_fifo_mod_force_en = 1;
  
  // Second stage config
  dev->conf_chan.val = 0;
  
  // 16-bit single channel data
  dev->conf_chan.tx_chan_mod = 1;
  dev->conf_chan.rx_chan_mod = 1;
    
#endif  

  
  // Device Reset
  dev_reset(dev);    
  dev->conf1.val = 0;
  dev->conf1.tx_stop_en = 0; 
  
  // Allocate I2S status structure for buffer swapping stuff
  i2s_state = (i2s_parallel_state_t*) malloc(sizeof(i2s_parallel_state_t));
  assert(i2s_state != NULL);
  i2s_parallel_state_t *state = i2s_state;
    
  state->desccount_a    = conf->desccount_a;
  state->desccount_b    = conf->desccount_b;
  state->dmadesc_a      = conf->lldesc_a;
  state->dmadesc_b      = conf->lldesc_b;  
  state->i2s_interrupt_port_arg  = port; // need to keep this somewhere in static memory for the ISR

  dev->timing.val = 0;

  // We using the double buffering switch logic?
  if (conf->int_ena_out_eof)
  {
      // Get ISR setup 
      esp_err_t err =  esp_intr_alloc(irq_source, 
                                     (int)(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1),
                                     irq_hndlr, 
                                     &state->i2s_interrupt_port_arg, NULL);
      
      if(err) {
          return err;
      }

      
      // Setup interrupt handler which is focussed only on the (page 322 of Tech. Ref. Manual)
      // "I2S_OUT_EOF_INT: Triggered when rxlink has finished sending a packet"
      // ... whatever the hell that is supposed to mean... One massive linked list? So all pixels in the chain?
      dev->int_ena.out_eof = 1;
  }
   
  return ESP_OK;
}

 esp_err_t i2s_parallel_stop_dma(i2s_port_t port) {
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }

  i2s_dev_t* dev =  I2S[port];
  
  // Stop all ongoing DMA operations
  dev->out_link.stop = 1;
  dev->out_link.start = 0;
  dev->conf.tx_start = 0;
  
   return ESP_OK;
}


 esp_err_t i2s_parallel_send_dma(i2s_port_t port, lldesc_t* dma_descriptor) {
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return ESP_ERR_INVALID_ARG;
  }

  i2s_dev_t* dev =  I2S[port];
  

  // Configure DMA burst mode
  dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;

  // Set address of DMA descriptor
  dev->out_link.addr = (uint32_t) dma_descriptor;
 
 // Start DMA operation
  dev->out_link.stop  = 0; 
  dev->out_link.start = 1;
  
  dev->conf.tx_start  = 1;

  return ESP_OK;
}
/*
i2s_dev_t* i2s_parallel_get_dev(i2s_port_t port) {
  if(port < I2S_NUM_0 || port >= I2S_NUM_MAX) {
    return NULL;
  }

#ifdef ESP32_ORIG
if (port == I2S_NUM_1)
	return &I2S1;
#endif  
  
  return I2S0; // HARCODE THIS TO RETURN &I2S0
}
*/
// Double buffering flipping
// Flip to a buffer: 0 for bufa, 1 for bufb
// dmadesc_a and dmadesc_b point to the same memory if double buffering isn't enabled.
void i2s_parallel_flip_to_buffer(i2s_port_t port, int buffer_id) {

    if (i2s_state == NULL) {
      return; // :-()
    }

    lldesc_t *active_dma_chain;
    if (buffer_id == 0) {
        active_dma_chain=(lldesc_t*)&i2s_state->dmadesc_a[0];
    } else {
        active_dma_chain=(lldesc_t*)&i2s_state->dmadesc_b[0];
    }

    // setup linked list to refresh from new buffer (continuously) when the end of the current list has been reached
    i2s_state->dmadesc_a[i2s_state->desccount_a-1].qe.stqe_next = active_dma_chain;
    i2s_state->dmadesc_b[i2s_state->desccount_b-1].qe.stqe_next = active_dma_chain;

    // we're still shifting out the buffer, so it shouldn't be written to yet.
    //previousBufferFree = false;
	i2s_parallel_set_previous_buffer_not_free();
}

bool i2s_parallel_is_previous_buffer_free() {
    return previousBufferFree;
}


void i2s_parallel_set_previous_buffer_not_free() {
    previousBufferFree = false;
	previousBufferOutputLoopCount = 0;
}
