## Estimating fillrate
  
Here are some results of simple tests on filling DMA buffer with data.
Filling DMA buffer requres lot's of memory operations on a bit level rather than doing simple byte/word wide store and copy. And it looks like it's quite a task both for esp32 core and compiler. 
I've done this while optimizing loops and bit logic along with testing compiler results.

So the testbed is:
 - Matrix modules: 4 x FM6126A based 64x64 modules chained in 256x64

A testpatters sketch:
 - allocating single DMA buffs for 256x64
 - allocating (NUM_LEDS*3) bytes for CRGB buffer
 - measuring microseconds for the following calls:
   - clearScreen() - full blanking
   - fillScreenRGB888() with monochrome/gray colors
   - fill screen using drawPixel()
   - filling some gradient into CRGB buff
   - painting CRGB buff into DMA buff with looped drawPixelRGB888()
   - drawing lines


||clearScreen()|drawPixelRGB888(), ticks|fillScreen()|fillScreen with a drawPixel()|fillRect() over Maxrix|V-line with drawPixel|fast-V-line|H-line with drawPixel|fast-H-line|
|--|--|--|--|--|--|--|--|--|--|
|v1.2.4|1503113 ticks|9244 non-cached, 675 cached|1719 us, 412272 t|47149 us, 11315418 ticks|-|24505 us, 5880209 ticks|-|24200 us|-|
|FastLines|1503113 ticks|1350 non-cached, 405 cached|1677 us, 401198 t|28511 us, 6841440 ticks|10395 us|14462 us, 3469605 ticks|10391 us, 2492743 ticks|14575 us|5180 us, 1242041 ticks|

to be continued...