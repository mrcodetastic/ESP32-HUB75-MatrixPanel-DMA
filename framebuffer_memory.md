# DMA FRAME BUFFER MEMORY ALLOCATION

With version 1.2.2 of the library onwards, the framebuffer is split over multiple 'blocks' (mallocs) as a workaround to the fact the typical Arduino sketch can have a fragmented memory map whereby a single contiguous memory allocation may not be available to fit the entire display. 

For example when `heap_caps_print_heap_info(MALLOC_CAP_DMA)` is executed to print information about the available memory blocks that are DMA capable (which we need for a DMA-enabled pixel framebuffer), you may see something like this:

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

The previous versions of the library were only able to use the largest single block of DMA capable memory. The library now is able to use non-contiguous blocks of memory.

Given it's possible to display 128x32 with double buffering in approx. 100kB of RAM. If we use two memory blocks for example, theoretically we should be able to display 256x64 or greater resolution (with double buffering disabled).

# Caveats

Experimentation will be required as available memory is highly dependant on other stuff you have in your sketch. It is best to include and use the 'ESP32-RGB64x32MatrixPanel-I2S-DMA' library as early as possible in your code and analyse the serial output of `heap_caps_print_heap_info(MALLOC_CAP_DMA)` to see what DMA memory blocks are available.
