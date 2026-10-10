# Detailed Overview of the PARLIO Implementation

The `Bus_Parallel16` implementation utilizes the ESP32's **Parallel IO (PARLIO)** peripheral and **General Direct Memory Access (GDMA)** to establish a high-throughput 16-bit parallel output stream. Designed for targets supporting `SOC_PARLIO_SUPPORTED` (such as the ESP32-P4), it handles hardware-level pin routing, peripheral clock generation, descriptor link management, and cache synchronization to drive parallel interfaces like LED matrices.

---

## 1. Initialization and Setup Flow (`init()`)

The primary entry point for setting up the hardware bus is the `init()` method, which orchestrates the following configuration steps:

* **GPIO Signal Routing (`configure_pins()`):** Loops through the 16 parallel data pins (`pin_d0` to `pin_d15`) and the output clock pin (`pin_wr`), configuring them as digital outputs. It then calls `esp_rom_gpio_connect_out_signal()` to bind those pins directly to the PARLIO TX unit's internal data and clock output signals via the ESP ROM signal matrix.
* **AXI-GDMA Channel Allocation:** Allocates a dedicated AXI GDMA transmit channel (`gdma_new_axi_channel()`) to handle memory-to-peripheral data movement without CPU intervention.
* **Peripheral Trigger Connection:** Connects the allocated GDMA channel to the PARLIO peripheral trigger source (`GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_PARLIO, 0)`) using `gdma_connect()`.
* **Transfer Parameters and Strategy:** Configures transfer options via `gdma_config_transfer()`, setting a maximum data burst size of 16. It then applies a custom GDMA strategy (`gdma_apply_strategy()`) with `owner_check = false`, `auto_update_desc = false`, and `eof_till_data_popped = true`.
* **Alignment Constraint Discovery:** Queries the internal and external alignment requirements of the GDMA channel using `gdma_get_alignment_constraints()` and retrieves cache line constraints via `esp_cache_get_alignment()`.

---

## 2. Key PARLIO Configuration Settings (`configure_parlio_ll()`)

Low-level configuration of the PARLIO hardware registers is handled entirely within `configure_parlio_ll()`:

* **Bus Clock Enable and Reset:** Uses atomic peripheral control (`PERIPH_RCC_ATOMIC`) to enable the PARLIO bus clock and execute a hardware register reset (`parlio_ll_enable_bus_clock()` and `parlio_ll_reset_register()`).
* **Clock Source and Divider Calculation:** Retrieves the system XTAL frequency (40 MHz) via `esp_clk_tree_src_get_freq_hz()`. It computes an integer clock divider based on the user-requested `_cfg.bus_freq`, packing it into a `hal_utils_clk_div_t` structure before applying it to the PARLIO TX clock source (`PARLIO_CLK_SRC_XTAL`).
* **Continuous Clock Generation:** Calls `parlio_ll_tx_enable_clock_gating(&PARL_IO, false)` to disable clock gating. This ensures the write clock (`pin_wr`) toggles continuously for every data word pushed through the bus rather than gating or pausing between frames.
* **Bus Width and Idle States:** Sets the parallel interface width to 16 bits using `parlio_ll_tx_set_bus_width()` and establishes default idle data values via `parlio_ll_tx_set_idle_data_value()`.
* **Transaction Length and Interrupts:** Configures transaction bit length parameters and clears any residual peripheral interrupt flags.

---

## 3. Descriptor Management and Data Transfer

The driver handles payload staging through circular descriptor chains and explicit cache synchronization:

* **Linked List Allocation (`allocate_dma_desc_memory()`):** Provisions hardware-compatible linked lists (`_gdma_link_list_a` and optionally `_gdma_link_list_b` for double buffering) using `gdma_new_link_list()` with 8-byte item alignment.
* **Buffer Mounting (`create_dma_desc_link()`):** Binds individual memory blocks to the descriptor chain via `gdma_link_mount_buffers()`. When the final descriptor in the sequence is reached, `gdma_link_concat()` links it back to index 0, creating a continuous, looping circular stream.
* **Cache Synchronization (`sync_payload_for_dma()`):** Because the ESP32-P4 utilizes cached memory subsystems, CPU writes must be explicitly flushed to physical RAM before GDMA reads them. This method calculates the total descriptor array size, aligns the start address down and the total size up to 64-byte boundaries, and executes `esp_cache_msync()` with `ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA`.
* **Execution Control (`dma_transfer_start()` / `dma_transfer_stop()`):** Starts transmission by synchronizing cache payloads, resetting the PARLIO FIFO and GDMA channel, fetching the active list's head address via `gdma_link_get_head_addr()`, and starting both GDMA and PARLIO hardware units concurrently.