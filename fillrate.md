## Estimating fillrate
  
Here are some results of simple tests on filling DMA buffer with data.
Filling DMA buffer requres lot's of memory operations on a bit level rather than doing simple byte/word wide store and copy. And it looks like it's quite a task both for esp32 core and compiler. 
I've done this while optimizing loops and bit logic along with testing compiler results.

So the testbed is:
 - Matrix modules: 4 x FM6126A based 64x64 modules chained in 256x64

A simple sketch:
 - allocating single DMA buffs for 256x64
 - allocating (NUM_LEDS*3) bytes for CRGB buffer
 - measuring microseconds for the following calls:
   - clearScreen() - full blanking
   - fillScreenRGB888() with monochrome/gray colors
   - filling some gradient into CRGB buff
   - painting CRGB buff into DMA buff with looped drawPixelRGB888()

|Pattern  |Reference|Ref+SPEED|Optimized|Optimized+SPEED|
|--|--|--|--|--|
|fillScreenRGB888()|14570|14570|13400 (8.5% faster)|5520 (164% faster)|
|CRGB buff fill|760|760|760|760|
|updateMatrixDMABuffer(CRGB)|7700|32080|55780 (38% faster)|33500(+4.2% slower)|

to be continued...