# Glediator (Desktop LED Matrix Controller + ESP32 Receiver)

Looking to stream live generative visuals, audio-reactive effects, and playlists from your computer to an ESP32 HUB75 LED matrix over Wi-Fi?

Check out the standalone **Glediator** project:

- **Main repository:** https://github.com/mrcodetastic/Glediator
- **ESP32 HUB75 use-case example (`esp32-firmware`):** https://github.com/mrcodetastic/Glediator/tree/master/esp32-firmware
- **Wire & discovery protocol docs:** https://github.com/mrcodetastic/Glediator/blob/master/docs/protocol.md

## What it provides

- **Desktop controller (Python):** Real-time effect engine, faders/filters, scene sequencer, audio capture, and OSC control.
- **ESP32 HUB75 firmware (`esp32-firmware/esp32-firmware.ino`):** Standalone Arduino sketch using `ESP32-HUB75-MatrixPanel-I2S-DMA` with double buffering (`flipDMABuffer()`), `WebSocketsServer`, and LAN discovery:
  - Broadcasts a `CODETASTIC` UDP discovery beacon on port `20586` so the desktop app finds the panel automatically.
  - Streams RGB565 frames over WebSocket (port `12345`) with full-frame (`0x81 0x81`), delta rectangle (`0x81 0x82`), and RLE (`0x81 0x83`) support, plus `ack` flow control and remote brightness commands.

This folder is intentionally a placeholder so the example stays in sync with the [`mrcodetastic/Glediator`](https://github.com/mrcodetastic/Glediator) repository.
