# SPLIT_MEMORY_MODE

### New Feature

With version 1.1.0 of the library onwards, there is now a 'feature' to split the framebuffer  over two memory 'blocks' (mallocs) to work around the fact that typically the ESP32 upon 'boot up' has 2 x ~100kb chunks of memory available to use (ideally we'd want one massive chunk, but for whatever reasons this isn't the case). This allows non-contiguous memory allocations to be joined-up potentially allowing for 512x64 resolution or greater (no guarantees however). This is enabled by default with:

```
#define SPLIT_MEMORY_MODE 1
```
...in 'ESP32-RGB64x32MatrixPanel-I2S-DMA.h'


## What is it trying to resolve?

For whatever reason (and this may not be consistent across all ESP32 environments) when `heap_caps_print_heap_info(MALLOC_CAP_DMA)` is executed to print information about the available memory blocks that are DMA capable (which we need for a DMA-enabled pixel framebuffer), you may see something like this:

```
Heap summary for capabilities 0x00000008:
  At 0x3ffbdb28 len 52 free 4 allocated 0 min_free 4
    largest_free_block 4 alloc_blocks 0 free_blocks 1 total_blocks 1
  At 0x3ffb8000 len 6688 free 5256 allocated 1208 min_free 5256
    largest_free_block 5256 alloc_blocks 11 free_blocks 1 total_blocks 12
  At 0x3ffb0000 len 25480 free 17172 allocated 8228 min_free 17172
    largest_free_block 17172 alloc_blocks 2 free_blocks 1 total_blocks 3
  At 0x3ffae6e0 len 6192 free 6092 allocated 36 min_free 6092
    largest_free_block 6092 alloc_blocks 1 free_blocks 1 total_blocks 2
  At 0x3ffaff10 len 240 free 0 allocated 120 min_free 0
    largest_free_block 0 alloc_blocks 5 free_blocks 1 total_blocks 6
  At 0x3ffb6388 len 7288 free 0 allocated 6920 min_free 0
    largest_free_block 0 alloc_blocks 21 free_blocks 0 total_blocks 21
  At 0x3ffb9a20 len 16648 free 6300 allocated 9680 min_free 348
    largest_free_block 4980 alloc_blocks 38 free_blocks 4 total_blocks 42
  At 0x3ffc1818 len 124904 free 124856 allocated 0 min_free 124856
    largest_free_block 124856 alloc_blocks 0 free_blocks 1 total_blocks 1
  At 0x3ffe0440 len 15072 free 15024 allocated 0 min_free 15024
    largest_free_block 15024 alloc_blocks 0 free_blocks 1 total_blocks 1
  At 0x3ffe4350 len 113840 free 113792 allocated 0 min_free 113792
    largest_free_block 113792 alloc_blocks 0 free_blocks 1 total_blocks 1
  Totals:
    free 288496 allocated 26192 min_free 282544 largest_free_block 124856
```

So what? Well if you look closely, you will probably see two lines (blocks) like this:

```
  At 0x3ffc1818 len 124904 free 124856 allocated 0 min_free 124856
    largest_free_block 124856 alloc_blocks 0 free_blocks 1 total_blocks 1
  ...
  At 0x3ffe4350 len 113840 free 113792 allocated 0 min_free 113792
    largest_free_block 113792 alloc_blocks 0 free_blocks 1 total_blocks 1
```

What this means is there are two blocks of DMA capable memory that are about 100kB each.

The previous library was only able to use the largest single block of DMA capable memory. Now we can join the two largest together.

Given it's possible to display 128x32 with double buffering in approx. 100kB of RAM. If we use two memory blocks, and disable double buffering (which is now the default), theoretically we should be able to display 256x64 or greater resolution.

# Caveats
When SPLIT_MEMORY_MODE is enabled, the library will divide the memory framebuffer (pixel buffer) in half and split over two blocks of the same size. Therefore, if one of the free DMA memory blocks is SMALLER than 'half' the framebuffer, failure will occur.

I.e. From the above example, we could not have any half be greater than 113792 bytes. 

Experimentation will be required as available memory is highly dependant on other stuff you have in your sketch. It is best to include and use the 'ESP32-RGB64x32MatrixPanel-I2S-DMA' library as early as possible in your code and analyse the serial output of `heap_caps_print_heap_info(MALLOC_CAP_DMA)` to see what DMA memory blocks are available.
