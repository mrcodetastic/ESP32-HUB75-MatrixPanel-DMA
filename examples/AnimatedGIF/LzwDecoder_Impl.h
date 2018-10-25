/*
 * This file contains code to decompress the LZW encoded animated GIF data
 *
 * Written by: Craig A. Lindley, Fabrice Bellard and Steven A. Bennett
 * See my book, "Practical Image Processing in C", John Wiley & Sons, Inc.
 *
 * Copyright (c) 2014 Craig A. Lindley
 * Minor modifications by Louis Beaudoin (pixelmatix)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define LZWDEBUG 0

#include "GifDecoder.h"

template <int maxGifWidth, int maxGifHeight, int lzwMaxBits>
void GifDecoder<maxGifWidth, maxGifHeight, lzwMaxBits>::lzw_setTempBuffer(uint8_t * tempBuffer) {
    temp_buffer = tempBuffer;
}

// Initialize LZW decoder
//   csize initial code size in bits
//   buf input data
template <int maxGifWidth, int maxGifHeight, int lzwMaxBits>
void GifDecoder<maxGifWidth, maxGifHeight, lzwMaxBits>::lzw_decode_init (int csize) {

    // Initialize read buffer variables
    bbuf = 0;
    bbits = 0;
    bs = 0;
    bcnt = 0;

    // Initialize decoder variables
    codesize = csize;
    cursize = codesize + 1;
    curmask = mask[cursize];
    top_slot = 1 << cursize;
    clear_code = 1 << codesize;
    end_code = clear_code + 1;
    slot = newcodes = clear_code + 2;
    oc = fc = -1;
    sp = stack;
}

//  Get one code of given number of bits from stream
template <int maxGifWidth, int maxGifHeight, int lzwMaxBits>
int GifDecoder<maxGifWidth, maxGifHeight, lzwMaxBits>::lzw_get_code() {

    while (bbits < cursize) {
        if (bcnt == bs) {
            // get number of bytes in next block
            readIntoBuffer(temp_buffer, 1);
            bs = temp_buffer[0];
            readIntoBuffer(temp_buffer, bs);
            bcnt = 0;
        }
        bbuf |= temp_buffer[bcnt] << bbits;
        bbits += 8;
        bcnt++;
    }
    int c = bbuf;
    bbuf >>= cursize;
    bbits -= cursize;
    return c & curmask;
}

// Decode given number of bytes
//   buf 8 bit output buffer
//   len number of pixels to decode
//   returns the number of bytes decoded
template <int maxGifWidth, int maxGifHeight, int lzwMaxBits>
int GifDecoder<maxGifWidth, maxGifHeight, lzwMaxBits>::lzw_decode(uint8_t *buf, int len, uint8_t *bufend) {
    int l, c, code;

#if LZWDEBUG == 1
    unsigned char debugMessagePrinted = 0;
#endif

    if (end_code < 0) {
        return 0;
    }
    l = len;

    for (;;) {
        while (sp > stack) {
            // load buf with data if we're still within bounds
            if(buf < bufend) {
                *buf++ = *(--sp);
            } else {
                // out of bounds, keep incrementing the pointers, but don't use the data
#if LZWDEBUG == 1
                // only print this message once per call to lzw_decode
                if(buf == bufend)
                    Serial.println("****** LZW imageData buffer overrun *******");
#endif
            }
            if ((--l) == 0) {
                return len;
            }
        }
        c = lzw_get_code();
        if (c == end_code) {
            break;

        }
        else if (c == clear_code) {
            cursize = codesize + 1;
            curmask = mask[cursize];
            slot = newcodes;
            top_slot = 1 << cursize;
            fc= oc= -1;

        }
        else    {

            code = c;
            if ((code == slot) && (fc >= 0)) {
                *sp++ = fc;
                code = oc;
            }
            else if (code >= slot) {
                break;
            }
            while (code >= newcodes) {
                *sp++ = suffix[code];
                code = prefix[code];
            }
            *sp++ = code;
            if ((slot < top_slot) && (oc >= 0)) {
                suffix[slot] = code;
                prefix[slot++] = oc;
            }
            fc = code;
            oc = c;
            if (slot >= top_slot) {
                if (cursize < lzwMaxBits) {
                    top_slot <<= 1;
                    curmask = mask[++cursize];
                } else {
#if LZWDEBUG == 1
                    if(!debugMessagePrinted) {
                        debugMessagePrinted = 1;
                        Serial.println("****** cursize >= lzwMaxBits *******");
                    }
#endif
                }

            }
        }
    }
    end_code = -1;
    return len - l;
}
