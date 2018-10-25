#ifndef FILENAME_FUNCTIONS_H
#define FILENAME_FUNCTIONS_H

#include "FS.h" // for the 'File' class
#include <SPIFFS.h>


#define FILESYSTEM SPIFFS
#define DBG_OUTPUT_PORT Serial


int enumerateGIFFiles(const char *directoryName, boolean displayFilenames);
void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer);
int openGifFilenameByIndex(const char *directoryName, int index);

bool fileSeekCallback(unsigned long position);
unsigned long filePositionCallback(void);
int fileReadCallback(void);
int fileReadBlockCallback(void * buffer, int numberOfBytes);

#endif
