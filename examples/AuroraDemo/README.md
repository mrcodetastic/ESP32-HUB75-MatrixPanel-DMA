## Aurora Demo
This sketch combines:

- `ESP32-HUB75-MatrixPanel-DMA` / its `VirtualMatrixPanel` to drive a chained HUB75 display.
- `GFX_Lite` for drawing and its RGB, palette, noise, and animation primitives.

`GFX_Lite` is a required dependency and is included explicitly by the sketch. The demo does not include or link FastLED separately; pixel and palette code uses the compatible APIs exposed through `GFX_Lite`.

## Instructions

- Send `n` over serial to advance, `p` to go back, and `a` to toggle automatic pattern changes.
- The physical panel dimensions and row/column arrangement are configured at the top of `AuroraDemo.ino`; virtual width and height are derived from them.

## Pattern lifecycle and memory

Pattern objects are kept in a fixed registry and are not created and destroyed during transitions. Every `Drawable` exposes `load()` and `unload()` hooks; the default implementations are no-ops for patterns that need no owned resources. The manager calls `load()` before `start()`, then `stop()` and `unload()` when leaving a pattern. A failed `load()` is reported and the frame loop safely skips that pattern.

`PatternStarfield` and `PatternFireKoz` demonstrate optional state allocated once in `load()` and released in `unload()`. This avoids large per-pattern buffers occupying RAM for the lifetime of the demo. Heap allocation on every frame is deliberately avoided. Repeatedly allocating buffers of different sizes can still fragment an embedded heap, so keep allocations bounded, check failures, pair every allocation with `unload()`, and prefer fixed/shared buffers for projects with many large pattern states or long uptimes. The display framebuffer and noise layer remain long-lived in `EffectsLayer`.

The pattern registry is also the single source of pattern ordering; pattern transitions wrap in either direction and use that registry for serial listing.
