// Code copied from AnimatedGIF examples

#ifndef M5STACK_SD
 // for custom ESP32 builds
 #define M5STACK_SD SD
#endif


static void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  //log_d("GIFOpenFile( %s )\n", fname );
  FSGifFile = M5STACK_SD.open(fname);
  if (FSGifFile) {
    *pSize = FSGifFile.size();
    return (void *)&FSGifFile;
  }
  return NULL;
}


static void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
}


static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
      iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
      return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}


static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //log_d("Seek time = %d us\n", i);
  return pFile->iPos;
}


// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  // Respect frame X offset
  int baseX = pDraw->iX;   // Use the frame's X offset as a reference
  iWidth = pDraw->iWidth;
  if (iWidth > PANEL_RES_X)
      iWidth = PANEL_RES_X;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line (frame top + line offset)

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) { // restore to background color
    for (x=0; x<iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
          s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while(x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      // 收集连续的非透明像素到 usTemp
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // hit transparent, back up pointer
          s--;
        } else { // opaque
            *d++ = usPalette[c];
            iCount++;
        }
      } // while looking for opaque pixels

      if (iCount) { // any opaque pixels?
          for(int xOffset = 0; xOffset < iCount; xOffset++ ){
            dma_display->drawPixel(baseX + x + xOffset, y, usTemp[xOffset]); // 565 Color Format, add baseX
          }
        x += iCount;
        iCount = 0;
      }

      // look for a run of transparent pixels and skip them
      iCount = 0;
      while (s < pEnd && *s == ucTransparent) {
        s++;
        iCount++;
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x=0; x<iWidth; x++)
      dma_display->drawPixel(baseX + x, y, usPalette[*s++]); // color 565, add baseX
  }
} /* GIFDraw() */