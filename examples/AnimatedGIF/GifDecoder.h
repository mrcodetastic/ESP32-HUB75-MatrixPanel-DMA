#ifndef _GIFDECODER_H_
#define _GIFDECODER_H_

#include <stdint.h>
#include <Arduino.h>


typedef void (*callback)(void);
typedef void (*pixel_callback)(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
typedef void* (*get_buffer_callback)(void);

typedef bool (*file_seek_callback)(unsigned long position);
typedef unsigned long (*file_position_callback)(void);
typedef int (*file_read_callback)(void);
typedef int (*file_read_block_callback)(void * buffer, int numberOfBytes);

// LZW constants
// NOTE: LZW_MAXBITS should be set to 10 or 11 for small displays, 12 for large displays
//   all 32x32-pixel GIFs tested work with 11, most work with 10
//   LZW_MAXBITS = 12 will support all GIFs, but takes 16kB RAM
#define LZW_SIZTABLE  (1 << lzwMaxBits)

template <int maxGifWidth, int maxGifHeight, int lzwMaxBits>
class GifDecoder {
public:
    int startDecoding(void);
    int decodeFrame(void);
    
    void setScreenClearCallback(callback f);
    void setUpdateScreenCallback(callback f);
    void setDrawPixelCallback(pixel_callback f);
    void setStartDrawingCallback(callback f);

    void setFileSeekCallback(file_seek_callback f);
    void setFilePositionCallback(file_position_callback f);
    void setFileReadCallback(file_read_callback f);
    void setFileReadBlockCallback(file_read_block_callback f);

private:
    void parseTableBasedImage(void);
    void decompressAndDisplayFrame(unsigned long filePositionAfter);
    int parseData(void);
    int parseGIFFileTerminator(void);
    void parseCommentExtension(void);
    void parseApplicationExtension(void);
    void parseGraphicControlExtension(void);
    void parsePlainTextExtension(void);
    void parseGlobalColorTable(void);
    void parseLogicalScreenDescriptor(void);
    bool parseGifHeader(void);
    void copyImageDataRect(uint8_t *dst, uint8_t *src, int x, int y, int width, int height);
    void fillImageData(uint8_t colorIndex);
    void fillImageDataRect(uint8_t colorIndex, int x, int y, int width, int height);
    int readIntoBuffer(void *buffer, int numberOfBytes);
    int readWord(void);
    void backUpStream(int n);
    int readByte(void);

    void lzw_decode_init(int csize);
    int lzw_decode(uint8_t *buf, int len, uint8_t *bufend);
    void lzw_setTempBuffer(uint8_t * tempBuffer);
    int lzw_get_code(void);

    // Logical screen descriptor attributes
    int lsdWidth;
    int lsdHeight;
    int lsdPackedField;
    int lsdAspectRatio;
    int lsdBackgroundIndex;

    // Table based image attributes
    int tbiImageX;
    int tbiImageY;
    int tbiWidth;
    int tbiHeight;
    int tbiPackedBits;
    bool tbiInterlaced;

    int frameDelay;
    int transparentColorIndex;
    int prevBackgroundIndex;
    int prevDisposalMethod;
    int disposalMethod;
    int lzwCodeSize;
    bool keyFrame;
    int rectX;
    int rectY;
    int rectWidth;
    int rectHeight;

    unsigned long nextFrameTime_ms;

    int colorCount;
    rgb_24 palette[256];

    char tempBuffer[260];

    // Buffer image data is decoded into
    uint8_t imageData[maxGifWidth * maxGifHeight];

    // Backup image data buffer for saving portions of image disposal method == 3
    uint8_t imageDataBU[maxGifWidth * maxGifHeight];

    callback screenClearCallback;
    callback updateScreenCallback;
    pixel_callback drawPixelCallback;
    callback startDrawingCallback;
    file_seek_callback fileSeekCallback;
    file_position_callback filePositionCallback;
    file_read_callback fileReadCallback;
    file_read_block_callback fileReadBlockCallback;

    // LZW variables
    int bbits;
    int bbuf;
    int cursize;                // The current code size
    int curmask;
    int codesize;
    int clear_code;
    int end_code;
    int newcodes;               // First available code
    int top_slot;               // Highest code for current size
    int extra_slot;
    int slot;                   // Last read code
    int fc, oc;
    int bs;                     // Current buffer size for GIF
    int bcnt;
    uint8_t *sp;
    uint8_t * temp_buffer;

    uint8_t stack  [LZW_SIZTABLE];
    uint8_t suffix [LZW_SIZTABLE];
    uint16_t prefix [LZW_SIZTABLE];

    // Masks for 0 .. 16 bits
    unsigned int mask[17] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000F, 0x001F, 0x003F, 0x007F,
        0x00FF, 0x01FF, 0x03FF, 0x07FF,
        0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
        0xFFFF
    };
};

#include "GifDecoder_Impl.h"
#include "LzwDecoder_Impl.h"

#endif
